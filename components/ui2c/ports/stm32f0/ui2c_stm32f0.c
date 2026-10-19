#include "ports/ui2c_stm32f0.h"

#define UI2C_IRQ_BITS                                                                                                  \
    (I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_ADDRIE | I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE)
#define UI2C_DMA_BITS    (I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN)
#define UI2C_CLEAR_FLAGS (I2C_ICR_NACKCF | I2C_ICR_STOPCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF)

static ui2c_lock_key_t lock(ui2c_t* bus) {
    (void)bus;
    uint32_t key = __get_PRIMASK();
    __disable_irq();
    __DMB();
    return key;
}

static void unlock(ui2c_t* bus, ui2c_lock_key_t key) {
    (void)bus;
    __DMB();
    __set_PRIMASK((uint32_t)key);
}

static uint32_t dma_shift(const ui2c_stm32f0_dma_t* dma) { return 4U * (dma->number - 1U); }

static void dma_disable(const ui2c_stm32f0_dma_t* dma) {
    if (dma->channel != NULL) {
        dma->channel->CCR &= ~DMA_CCR_EN;
        (void)dma->channel->CCR;
        __DSB();
        /* ES0202 2.4.1: disable first; do not clear the global interrupt flag. */
        DMA1->IFCR = 0xEU << dma_shift(dma);
    }
}

static void stop_dma(ui2c_stm32f0_t* port) {
    port->hw.instance->CR1 &= ~UI2C_DMA_BITS;
    if (port->dma != NULL) {
        dma_disable(port->dma);
        port->dma = NULL;
    }
}

static void quiesce(ui2c_stm32f0_t* port) {
    I2C_TypeDef* hw = port->hw.instance;
    hw->CR1 &= ~(UI2C_IRQ_BITS | UI2C_DMA_BITS);
    stop_dma(port);
    port->left = 0U;
    port->pending_error = UI2C_STATUS_OK;
}

static void release_transfer(ui2c_stm32f0_t* port) {
    I2C_TypeDef* hw = port->hw.instance;
    quiesce(port);
    /* STOP or ARLO already ended local ownership. Keep PE/BUSY observation
     * intact: another controller may still be using the bus. */
    hw->CR2 = 0U;
    if ((hw->ISR & I2C_ISR_RXNE) != 0U) {
        (void)hw->RXDR;
    }
    hw->ICR = UI2C_CLEAR_FLAGS;
    /* Any prefetched TX byte is flushed by start_transfer, after BUSY check. */
}

static void abort_transfer(ui2c_t* bus) {
    ui2c_stm32f0_t* port = bus->port;
    I2C_TypeDef* hw = port->hw.instance;
    if ((hw->ISR & (I2C_ISR_ARLO | I2C_ISR_STOPF)) != 0U) {
        release_transfer(port);
        return;
    }
    quiesce(port);
    /* Forced local reset for an unfinished transfer. No GPIO recovery.
     * Keep PE low for >=3 APB cycles; readbacks/DSB and register writes ensure it. */
    hw->CR1 &= ~I2C_CR1_PE;
    (void)hw->CR1;
    __DSB();
    hw->CR2 = 0U;
    hw->ICR = UI2C_CLEAR_FLAGS;
    hw->ISR = I2C_ISR_TXE; /* Flush a prefetched TX byte. */
    hw->CR1 |= I2C_CR1_PE;
}

static void finish(ui2c_stm32f0_t* port, ui2c_status_t result) {
    if ((port->hw.instance->ISR & (I2C_ISR_ARLO | I2C_ISR_STOPF)) != 0U) {
        release_transfer(port);
    } else {
        abort_transfer(&port->bus);
    }
    ui2c_port_complete(&port->bus, result);
}

static void start_dma(ui2c_stm32f0_t* port, const ui2c_stm32f0_dma_t* dma, bool read) {
    I2C_TypeDef* hw = port->hw.instance;
    const ui2c_msg_t* msg = &port->bus.msgs[port->index];
    dma_disable(dma);
    DMA_Channel_TypeDef* ch = dma->channel;
    ch->CPAR = (uint32_t)(uintptr_t)(read ? &hw->RXDR : &hw->TXDR);
    ch->CMAR = (uint32_t)(uintptr_t)(read ? &msg->data.rx[port->offset] : &msg->data.tx[port->offset]);
    ch->CNDTR = port->left;
    /* Preserve the board's direction, width, increment, mode and priority. */
    ch->CCR = (ch->CCR & ~(DMA_CCR_TCIE | DMA_CCR_HTIE | DMA_CCR_TEIE)) | DMA_CCR_TCIE | DMA_CCR_TEIE;
    port->dma = dma;
    port->dma_count = port->left;
    __DMB();
    ch->CCR |= DMA_CCR_EN;
    hw->CR1 |= read ? I2C_CR1_RXDMAEN : I2C_CR1_TXDMAEN;
}

/* The outgoing boundary decides RELOAD, not whether this is the first message.
 * Never insert an unrequested START to work around a hardware length limit. */
static void start_chunk(ui2c_stm32f0_t* port, bool start) {
    I2C_TypeDef* hw = port->hw.instance;
    const ui2c_msg_t* msg = &port->bus.msgs[port->index];
    size_t remaining = msg->len - port->offset;
    bool read = (msg->flags & UI2C_MSG_READ) != 0U;
    port->boundary = ui2c_port_boundary(&port->bus, port->index);
    size_t count = remaining > 255U ? 255U : remaining;
    port->reload = remaining > count || port->boundary == UI2C_BOUNDARY_CONTINUE;

    /* ES0202 2.12.9: RX reload must use NBYTES=1. The final non-reload chunk
     * can still use DMA. This preserves protocol semantics under arbitrary IRQ
     * latency rather than assuming DMA will always win the erratum race. */
    if (read && port->reload) {
        count = 1U;
    }

    port->left = (uint16_t)count;
    hw->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | UI2C_DMA_BITS);

    if (start && !read && port->left != 0U) {
        /* ES0202 2.12.12: preload before START, including DMA transfers. */
        hw->TXDR = msg->data.tx[port->offset++];
        --port->left;
    }

    const ui2c_stm32f0_dma_t* dma = read ? &port->hw.rx_dma : &port->hw.tx_dma;
    uint16_t threshold = port->hw.dma_threshold == 0U ? 16U : port->hw.dma_threshold;

    if (dma->channel != NULL && port->left >= threshold && port->left > 1U) {
        start_dma(port, dma, read);
    } else if (port->left != 0U) {
        hw->CR1 |= read ? I2C_CR1_RXIE : I2C_CR1_TXIE;
    }

    uint32_t cr2 = (uint32_t)port->bus.addr.value << 1U;
    cr2 |= (uint32_t)count << I2C_CR2_NBYTES_Pos;

    if (read) {
        cr2 |= I2C_CR2_RD_WRN;
    }

    if (port->reload) {
        cr2 |= I2C_CR2_RELOAD;
    } else if (port->boundary == UI2C_BOUNDARY_STOP) {
        cr2 |= I2C_CR2_AUTOEND;
    }

    if (start) {
        cr2 |= I2C_CR2_START;
    }

    hw->CR1 |= I2C_CR1_NACKIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE;
    hw->CR2 = cr2;
}

static ui2c_status_t start_transfer(ui2c_t* bus) {
    ui2c_stm32f0_t* port = bus->port;
    I2C_TypeDef* hw = port->hw.instance;
    if ((hw->CR1 & I2C_CR1_PE) == 0U) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }
    if ((hw->ISR & I2C_ISR_BUSY) != 0U) {
        return UI2C_STATUS_BUS_BUSY;
    }
    if ((port->hw.tx_dma.channel != NULL && (port->hw.tx_dma.channel->CCR & DMA_CCR_EN) != 0U)
        || (port->hw.rx_dma.channel != NULL && (port->hw.rx_dma.channel->CCR & DMA_CCR_EN) != 0U)) {
        return UI2C_STATUS_BUSY;
    }
    hw->ICR = UI2C_CLEAR_FLAGS;
    hw->ISR = I2C_ISR_TXE;
    port->index = 0U;
    port->offset = 0U;
    port->dma = NULL;
    port->pending_error = UI2C_STATUS_OK;
    start_chunk(port, true);
    return UI2C_STATUS_OK;
}

static const ui2c_port_ops_t ops = {
    .lock = lock,
    .unlock = unlock,
    .start = start_transfer,
    .abort = abort_transfer,
};

static bool valid_dma(const ui2c_stm32f0_dma_t* dma, bool read) {
    if (dma->channel == NULL) {
        return true;
    }
    DMA_Channel_TypeDef* const channels[] = {
        DMA1_Channel1, DMA1_Channel2, DMA1_Channel3, DMA1_Channel4, DMA1_Channel5,
    };
    uint32_t mask = DMA_CCR_EN | DMA_CCR_DIR | DMA_CCR_CIRC | DMA_CCR_PINC | DMA_CCR_MINC | DMA_CCR_PSIZE
                    | DMA_CCR_MSIZE | DMA_CCR_MEM2MEM;
    uint32_t required = DMA_CCR_MINC | (read ? 0U : DMA_CCR_DIR);
    return dma->number >= 1U && dma->number <= 5U && dma->channel == channels[dma->number - 1U]
           && (dma->channel->CCR & mask) == required;
}

ui2c_status_t ui2c_stm32f0_init(ui2c_stm32f0_t* port, const ui2c_stm32f0_config_t* hw, const ui2c_config_t* config) {
    if (port == NULL || hw == NULL || hw->instance == NULL || !valid_dma(&hw->tx_dma, false)
        || !valid_dma(&hw->rx_dma, true) || (hw->tx_dma.channel != NULL && hw->tx_dma.channel == hw->rx_dma.channel)) {
        return UI2C_STATUS_INVALID_ARG;
    }
    if (port->bus.ops != NULL) {
        return UI2C_STATUS_BUSY;
    }
    if ((hw->instance->CR1 & I2C_CR1_PE) == 0U) {
        return UI2C_STATUS_NOT_INITIALIZED;
    }
    /* NOSTRETCH requires PE=0 to change. Validate board configuration rather
     * than resetting a controller that is observing an external transaction. */
    if ((hw->instance->CR1 & I2C_CR1_NOSTRETCH) != 0U) {
        return UI2C_STATUS_INVALID_ARG;
    }
    if ((hw->instance->CR2 & I2C_CR2_START) != 0U || (hw->instance->ISR & I2C_ISR_ADDR) != 0U) {
        return UI2C_STATUS_BUSY; /* Existing local transfer/address match. */
    }
    uint32_t caps = UI2C_CAP_RESTART | UI2C_CAP_CONTINUE | UI2C_CAP_PROBE;
    ui2c_status_t status = ui2c_port_init(&port->bus, &ops, port, config, caps);
    if (status != UI2C_STATUS_OK) {
        return status;
    }
    port->hw = *hw;
    ui2c_lock_key_t key = lock(&port->bus);
    dma_disable(&port->hw.tx_dma);
    dma_disable(&port->hw.rx_dma);
    /* Disable address recognition without resetting PE or losing BUSY.
     * RM0091 permits clearing OA1EN/OA2EN while the peripheral is enabled. */
    hw->instance->OAR1 &= ~I2C_OAR1_OA1EN;
    hw->instance->OAR2 &= ~I2C_OAR2_OA2EN;
    hw->instance->CR1 &= ~(I2C_CR1_GCEN | I2C_CR1_SBC);
    quiesce(port);
    hw->instance->ICR = UI2C_CLEAR_FLAGS;
    unlock(&port->bus, key);
    return UI2C_STATUS_OK;
}

/* Called with all port IRQs serialized. One hardware event snapshot per call. */
static void service(ui2c_stm32f0_t* port) {
    I2C_TypeDef* hw = port->hw.instance;
    if (port->bus.state != UI2C_ACTIVE) {
        hw->ICR = UI2C_CLEAR_FLAGS;
        return;
    }
    uint32_t flags = hw->ISR;
    if ((flags & I2C_ISR_ARLO) != 0U) {
        finish(port, UI2C_STATUS_ARBITRATION_LOST);
        return;
    }
    /* ES0202 2.12.8: spurious BERR in controller mode does not stop transfer. */
    if ((flags & I2C_ISR_BERR) != 0U) {
        hw->ICR = I2C_ICR_BERRCF;
    }
    if ((flags & I2C_ISR_OVR) != 0U) {
        finish(port, UI2C_STATUS_IO_ERROR);
        return;
    }
    if (port->dma != NULL && (DMA1->ISR & (8U << dma_shift(port->dma))) != 0U) {
        finish(port, UI2C_STATUS_IO_ERROR);
        return;
    }
    if ((flags & I2C_ISR_NACKF) != 0U) {
        port->pending_error = UI2C_STATUS_NACK;
        stop_dma(port);
        hw->CR1 &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE);
        hw->ICR = I2C_ICR_NACKCF;
    }
    if (port->pending_error != UI2C_STATUS_OK) {
        if ((flags & I2C_ISR_STOPF) != 0U) {
            finish(port, port->pending_error);
        }
        return; /* Hardware generates STOP after NACK; timeout bounds failure. */
    }
    const ui2c_msg_t* msg = &port->bus.msgs[port->index];
    if (port->dma == NULL && port->left != 0U) {
        if ((msg->flags & UI2C_MSG_READ) != 0U && (flags & I2C_ISR_RXNE) != 0U) {
            msg->data.rx[port->offset++] = (uint8_t)hw->RXDR;
            --port->left;
        } else if ((msg->flags & UI2C_MSG_READ) == 0U && (flags & I2C_ISR_TXIS) != 0U) {
            hw->TXDR = msg->data.tx[port->offset++];
            --port->left;
        }
    }
    uint32_t boundary_flags = flags & (I2C_ISR_STOPF | I2C_ISR_TC | I2C_ISR_TCR);
    if (boundary_flags == 0U) {
        return;
    }
    if (port->dma != NULL) {
        if (port->dma->channel->CNDTR != 0U) {
            /* RX DMA may still have its final write pending at STOP/TC. Avoid
             * an interrupt storm; DMA completion will resume service. */
            hw->CR1 &= ~(I2C_CR1_STOPIE | I2C_CR1_TCIE);
            return;
        }
        stop_dma(port);
        __DMB();
        port->offset += port->dma_count;
        port->left = 0U;
    }
    if (port->left != 0U) {
        finish(port, UI2C_STATUS_BUS_ERROR);
        return;
    }
    if ((flags & I2C_ISR_STOPF) != 0U) {
        bool complete = port->index + 1U == port->bus.num_msgs && port->offset == msg->len;
        finish(port, complete ? UI2C_STATUS_OK : UI2C_STATUS_BUS_ERROR);
        return;
    }
    if (((flags & I2C_ISR_TCR) != 0U) != port->reload) {
        finish(port, UI2C_STATUS_BUS_ERROR);
        return;
    }
    bool start = !port->reload;
    if (port->offset == msg->len) {
        if (port->boundary == UI2C_BOUNDARY_STOP || (start != (port->boundary == UI2C_BOUNDARY_RESTART))) {
            finish(port, UI2C_STATUS_BUS_ERROR);
            return;
        }
        ++port->index;
        port->offset = 0U;
    } else if (start) {
        finish(port, UI2C_STATUS_BUS_ERROR);
        return;
    }
    start_chunk(port, start);
}

void ui2c_stm32f0_irq(ui2c_stm32f0_t* port) {
    if (port == NULL || port->bus.ops == NULL) {
        return;
    }
    ui2c_lock_key_t key = lock(&port->bus);
    service(port);
    unlock(&port->bus, key);
}

void ui2c_stm32f0_dma_irq(ui2c_stm32f0_t* port) {
    if (port == NULL || port->bus.ops == NULL) {
        return;
    }
    ui2c_lock_key_t key = lock(&port->bus);
    if (port->dma != NULL) {
        uint32_t shift = dma_shift(port->dma);
        uint32_t flags = DMA1->ISR >> shift;
        if ((flags & (8U | 2U)) != 0U) {
            if ((flags & 8U) == 0U) {
                DMA1->IFCR = 6U << shift;
            }
            port->hw.instance->CR1 |= I2C_CR1_STOPIE | I2C_CR1_TCIE;
            /* Both IRQ vectors share error precedence (ARLO before DMA TE). */
            service(port);
        }
    }
    unlock(&port->bus, key);
}
