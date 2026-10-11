#include "ports/ui2c_stm32f0.h"
#ifndef UI2C_TEST_DLL
#include <stdio.h>
#endif

/* Deterministic event injection, not a cycle-accurate peripheral simulator.
 * W1C/START side effects are applied explicitly between injected events. */
#define CHECK(x)                                                                                                       \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            return __LINE__;                                                                                           \
        }                                                                                                              \
    } while (0)
DMA_TypeDef test_dma;
DMA_Channel_TypeDef test_channels[5];
uint32_t test_primask;
static uint32_t now;
static unsigned calls, callback_error, cases;
static ui2c_status_t result;
static I2C_TypeDef hw;
static ui2c_stm32f0_t port;
static uint8_t tx[65536], rx[65538];

static uint32_t clock_ms(void* arg) {
    (void)arg;
    return now;
}

static void completed(ui2c_t* bus, ui2c_status_t status, void* arg) {
    if (arg != &calls || ui2c_busy(bus) || test_primask != 0U) {
        callback_error++;
    }
    calls++;
    result = status;
}

static void reset_observation(void) {
    calls = 0;
    callback_error = 0;
    result = UI2C_STATUS_IO_ERROR;
}

static void apply_w1c(void) {
    hw.ISR &= ~hw.ICR;
    hw.ICR = 0;
    test_dma.ISR &= ~test_dma.IFCR;
    test_dma.IFCR = 0;
}

static void event(uint32_t flags) {
    hw.ISR = flags;
    ui2c_stm32f0_irq(&port);
    apply_w1c();
}

static int setup(bool dma) {
    port = (ui2c_stm32f0_t){0};
    hw = (I2C_TypeDef){0};
    test_dma = (DMA_TypeDef){0};
    for (unsigned i = 0; i < 5; i++) {
        test_channels[i] = (DMA_Channel_TypeDef){0};
    }
    now = 0;
    test_primask = 0;
    reset_observation();
    hw.CR1 = I2C_CR1_PE;
    hw.TIMINGR = 0x00201D2BU;
    ui2c_stm32f0_config_t config = {.instance = &hw, .dma_threshold = 16};
    if (dma) {
        config.tx_dma = (ui2c_stm32f0_dma_t){DMA1_Channel2, 2};
        config.rx_dma = (ui2c_stm32f0_dma_t){DMA1_Channel3, 3};
        DMA1_Channel2->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PL_0;
        DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PL_1;
    }
    ui2c_config_t core = {.time_ms = clock_ms, .timeout_ms = 100};
    CHECK(ui2c_stm32f0_init(&port, &config, &core) == UI2C_STATUS_OK);
    CHECK(hw.TIMINGR == 0x00201D2BU);
    CHECK(ui2c_stm32f0_init(&port, &config, &core) == UI2C_STATUS_BUSY);
    apply_w1c();
    return 0;
}

static uint8_t pattern(size_t i) { return (uint8_t)(i * 37U + 11U); }

/* Walk the requested wire protocol independently of the driver's cursor.
 * Test both DMA-before-I2C and I2C-before-DMA boundary IRQ orderings. */
static int transfer(ui2c_msg_t* msgs, size_t num, bool dma, bool dma_late) {
    int err = setup(dma);
    CHECK(err == 0);
    CHECK(ui2c_xfer(&port.bus, 0x76, msgs, num, completed, &calls) == UI2C_STATUS_OK);
    CHECK(ui2c_busy(&port.bus));
    size_t index = 0, offset = 0, wire = 0;
    unsigned guard = 0;
    while (port.bus.state == UI2C_ACTIVE) {
        CHECK(++guard < 200000U);
        ui2c_msg_t* msg = &msgs[index];
        uint32_t cr2 = hw.CR2;
        bool read = (msg->flags & UI2C_MSG_READ) != 0;
        bool start = offset == 0 && (index == 0 || (msg->flags & UI2C_MSG_RESTART));
        size_t count = (cr2 & I2C_CR2_NBYTES) >> I2C_CR2_NBYTES_Pos;
        CHECK((cr2 & I2C_CR2_SADD) == (0x76U << 1));
        CHECK(((cr2 & I2C_CR2_START) != 0) == start);
        CHECK(((cr2 & I2C_CR2_RD_WRN) != 0) == read);
        CHECK(count <= msg->len - offset);
        CHECK(count > 0 || (num == 1 && msg->len == 0));
        bool last = offset + count == msg->len;
        bool reload = !last || (index + 1 < num && !(msgs[index + 1].flags & UI2C_MSG_RESTART));
        bool stop = last && index + 1 == num;
        CHECK(((cr2 & I2C_CR2_RELOAD) != 0) == reload);
        CHECK(((cr2 & I2C_CR2_AUTOEND) != 0) == stop);
        if (read && reload) {
            CHECK(count == 1);
        }
        hw.CR2 &= ~I2C_CR2_START;
        size_t preloaded = start && !read && count != 0 ? 1 : 0;
        if (preloaded) {
            CHECK(hw.TXDR == msg->data.tx[offset]);
        }
        bool uses_dma = (hw.CR1 & (I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN)) != 0;
        uint32_t end_flag = stop ? I2C_ISR_STOPF : (reload ? I2C_ISR_TCR : I2C_ISR_TC);
        if (uses_dma) {
            DMA_Channel_TypeDef* channel = read ? DMA1_Channel3 : DMA1_Channel2;
            uint32_t shift = read ? 8U : 4U;
            CHECK(channel->CNDTR == count - preloaded);
            CHECK(channel->CCR & DMA_CCR_EN);
            CHECK(((channel->CCR & DMA_CCR_DIR) != 0) == !read);
            CHECK((channel->CCR & DMA_CCR_PL) == (read ? DMA_CCR_PL_1 : DMA_CCR_PL_0));
            const uint8_t* expected = read ? msg->data.rx + offset : msg->data.tx + offset + preloaded;
            CHECK(channel->CMAR == (uint32_t)(uintptr_t)expected);
            CHECK(channel->CPAR == (uint32_t)(uintptr_t)(read ? &hw.RXDR : &hw.TXDR));
            if (dma_late) {
                event(end_flag);
                CHECK(port.bus.state == UI2C_ACTIVE && calls == 0);
                CHECK((hw.CR1 & (I2C_CR1_STOPIE | I2C_CR1_TCIE)) == 0);
            }
            if (read) {
                for (size_t i = 0; i < count; i++) {
                    msg->data.rx[offset + i] = pattern(wire + i);
                }
            }
            channel->CNDTR = 0;
            test_dma.ISR = 2U << shift;
            hw.ISR = dma_late ? end_flag : 0;
            ui2c_stm32f0_dma_irq(&port);
            apply_w1c();
            if (!dma_late) {
                CHECK(port.bus.state == UI2C_ACTIVE && calls == 0);
                event(end_flag);
            }
            CHECK((channel->CCR & DMA_CCR_EN) == 0 || port.bus.state == UI2C_ACTIVE);
        } else {
            for (size_t i = preloaded; i < count; i++) {
                if (read) {
                    hw.RXDR = pattern(wire + i);
                }
                event(read ? I2C_ISR_RXNE : I2C_ISR_TXIS);
                CHECK(port.bus.state == UI2C_ACTIVE);
                if (!read) {
                    CHECK(hw.TXDR == msg->data.tx[offset + i]);
                }
            }
            event(end_flag);
        }
        if (read) {
            for (size_t i = 0; i < count; i++) {
                CHECK(msg->data.rx[offset + i] == pattern(wire + i));
            }
        }
        wire += count;
        offset += count;
        if (offset == msg->len) {
            index++;
            offset = 0;
        }
        CHECK(calls == 0);
    }
    CHECK(index == num);
    CHECK(ui2c_busy(&port.bus));
    CHECK((hw.CR1 & (I2C_CR1_TXDMAEN | I2C_CR1_RXDMAEN)) == 0);
    CHECK(!(DMA1_Channel2->CCR & DMA_CCR_EN) && !(DMA1_Channel3->CCR & DMA_CCR_EN));
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_OK && callback_error == 0);
    ui2c_process(&port.bus);
    event(I2C_ISR_STOPF); /* Stale IRQ must not cause another callback. */
    ui2c_process(&port.bus);
    CHECK(calls == 1 && !ui2c_busy(&port.bus));
    cases++;
    return 0;
}

static int protocol_matrix(void) {
    const size_t sizes[] = {1, 2, 15, 16, 17, 254, 255, 256, 510, 511, 65535, 65536};
    for (size_t i = 0; i < sizeof tx; i++) {
        tx[i] = pattern(i);
    }
    for (unsigned mode = 0; mode < 4; mode++) {
        bool dma = mode >= 2, late = mode & 1U;
        for (unsigned dir = 0; dir < 2; dir++) {
            for (size_t i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
                rx[0] = 0xA5;
                rx[sizes[i] + 1] = 0x5A;
                ui2c_msg_t msg = {.len = sizes[i], .flags = dir ? UI2C_MSG_READ : UI2C_MSG_WRITE};
                if (dir) {
                    msg.data.rx = rx + 1;
                } else {
                    msg.data.tx = tx;
                }
                int err = transfer(&msg, 1, dma, late);
                if (err) {
                    return err;
                }
                CHECK(rx[0] == 0xA5 && rx[sizes[i] + 1] == 0x5A);
            }
        }
        /* Every legal pair, including same-direction repeated START. */
        for (unsigned a = 0; a < 2; a++) {
            for (unsigned b = 0; b < 2; b++) {
                for (unsigned restart = 0; restart < 2; restart++) {
                    if (a != b && !restart) {
                        continue;
                    }
                    ui2c_msg_t msgs[2] = {
                        {.len = 256, .flags = a ? UI2C_MSG_READ : 0},
                        {.len = 32, .flags = (b ? UI2C_MSG_READ : 0) | (restart ? UI2C_MSG_RESTART : 0)},
                    };
                    if (a) {
                        msgs[0].data.rx = rx;
                    } else {
                        msgs[0].data.tx = tx;
                    }
                    if (b) {
                        msgs[1].data.rx = rx + 256;
                    } else {
                        msgs[1].data.tx = tx + 256;
                    }
                    int err = transfer(msgs, 2, dma, late);
                    if (err) {
                        return err;
                    }
                }
            }
        }
        ui2c_msg_t triple[] = {
            {.data.tx = tx, .len = 1},
            {.data.tx = tx + 1, .len = 32},
            {.data.rx = rx, .len = 32, .flags = UI2C_MSG_READ | UI2C_MSG_RESTART},
        };
        int err = transfer(triple, 3, dma, late);
        if (err) {
            return err;
        }
        ui2c_msg_t probe = {0};
        err = transfer(&probe, 1, dma, late);
        if (err) {
            return err;
        }
    }
    return 0;
}

static int errors(void) {
    ui2c_msg_t msg = {.data.tx = tx, .len = 32};
    const uint32_t flags[] = {I2C_ISR_ARLO | I2C_ISR_STOPF, I2C_ISR_NACKF | I2C_ISR_STOPF, I2C_ISR_OVR, I2C_ISR_STOPF};
    const ui2c_status_t statuses[] = {UI2C_STATUS_ARBITRATION_LOST, UI2C_STATUS_NACK, UI2C_STATUS_IO_ERROR,
                                      UI2C_STATUS_BUS_ERROR};
    for (unsigned i = 0; i < 4; i++) {
        CHECK(setup(false) == 0);
        CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
        event(flags[i]);
        ui2c_cancel(&port.bus); /* Already completed error wins. */
        ui2c_process(&port.bus);
        CHECK(calls == 1 && result == statuses[i] && callback_error == 0);
        CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
        ui2c_cancel(&port.bus);
        ui2c_process(&port.bus);
        CHECK(calls == 2 && result == UI2C_STATUS_CANCELLED);
        cases++;
    }
    CHECK(setup(true) == 0);
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    test_dma.ISR = 8U << 4;
    ui2c_stm32f0_dma_irq(&port);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_DMA_ERROR);
    CHECK(!(DMA1_Channel2->CCR & DMA_CCR_EN));
    CHECK(setup(true) == 0);
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    event(I2C_ISR_BERR);
    CHECK(port.bus.state == UI2C_ACTIVE); /* F051 erratum: ignored spurious BERR. */
    event(I2C_ISR_NACKF);
    CHECK(port.bus.state == UI2C_ACTIVE && !(DMA1_Channel2->CCR & DMA_CCR_EN));
    event(I2C_ISR_STOPF);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_NACK);
    cases += 2;
    return 0;
}

static unsigned aborts;
static ui2c_status_t start_result;

static ui2c_lock_key_t fake_lock(ui2c_t* bus) {
    (void)bus;
    uint32_t key = test_primask;
    test_primask = 1;
    return key;
}

static void fake_unlock(ui2c_t* bus, ui2c_lock_key_t key) {
    (void)bus;
    test_primask = (uint32_t)key;
}

static ui2c_status_t fake_start(ui2c_t* bus) {
    (void)bus;
    return start_result;
}

static void fake_abort(ui2c_t* bus) {
    (void)bus;
    aborts++;
}

static const ui2c_port_ops_t fake_ops = {
    .lock = fake_lock, .unlock = fake_unlock, .start = fake_start, .abort = fake_abort};
static ui2c_msg_t probe;

static void resubmit(ui2c_t* bus, ui2c_status_t status, void* arg) {
    completed(bus, status, arg);
    if (ui2c_xfer(bus, 0x76, &probe, 1, completed, &calls) != UI2C_STATUS_OK) {
        callback_error++;
    }
}

static int core_contract(void) {
    ui2c_t bus = {0};
    ui2c_config_t config = {.time_ms = clock_ms, .timeout_ms = 100};
    reset_observation();
    test_primask = 0;
    aborts = 0;
    start_result = UI2C_STATUS_OK;
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_NOT_INITIALIZED);
    CHECK(ui2c_port_init(&bus, &fake_ops, NULL, &config, UI2C_CAP_PROBE) == UI2C_STATUS_OK);
    CHECK(ui2c_xfer(&bus, 0x80, &probe, 1, completed, &calls) == UI2C_STATUS_UNSUPPORTED);
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, NULL, NULL) == UI2C_STATUS_INVALID_ARG);
    CHECK(ui2c_xfer(&bus, 0x76, NULL, 1, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    ui2c_msg_t invalid = {.len = 1};
    CHECK(ui2c_xfer(&bus, 0x76, &invalid, 1, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    invalid = (ui2c_msg_t){.data.tx = tx, .len = 1, .flags = UI2C_MSG_RESTART};
    CHECK(ui2c_xfer(&bus, 0x76, &invalid, 1, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    invalid.flags = 0x8000;
    CHECK(ui2c_xfer(&bus, 0x76, &invalid, 1, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    invalid = (ui2c_msg_t){.flags = UI2C_MSG_READ};
    CHECK(ui2c_xfer(&bus, 0x76, &invalid, 1, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    ui2c_msg_t pair[] = {{.data.tx = tx, .len = 1}, {.data.rx = rx, .len = 1, .flags = UI2C_MSG_READ}};
    CHECK(ui2c_xfer(&bus, 0x76, pair, 2, completed, &calls) == UI2C_STATUS_INVALID_ARG);
    pair[1].flags |= UI2C_MSG_RESTART;
    CHECK(ui2c_xfer(&bus, 0x76, pair, 2, completed, &calls) == UI2C_STATUS_UNSUPPORTED);
    pair[1].flags = 0;
    pair[1].data.tx = tx;
    CHECK(ui2c_xfer(&bus, 0x76, pair, 2, completed, &calls) == UI2C_STATUS_UNSUPPORTED);
    start_result = UI2C_STATUS_BUS_BUSY;
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_BUS_BUSY);
    CHECK(!ui2c_busy(&bus) && bus.msgs == NULL && calls == 0);
    start_result = UI2C_STATUS_OK;
    now = UINT32_MAX - 49U;
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_OK);
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_BUSY);
    now = 49;
    ui2c_process(&bus);
    CHECK(calls == 0);
    now = 50;
    ui2c_process(&bus);
    CHECK(calls == 1 && result == UI2C_STATUS_TIMEOUT && aborts == 1);
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, resubmit, &calls) == UI2C_STATUS_OK);
    ui2c_port_complete(&bus, UI2C_STATUS_OK);
    ui2c_port_complete(&bus, UI2C_STATUS_IO_ERROR);
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_BUSY);
    CHECK(ui2c_cancel(&bus) == UI2C_STATUS_OK && aborts == 1);
    now += 1000;
    ui2c_process(&bus);
    CHECK(calls == 2 && result == UI2C_STATUS_OK && ui2c_busy(&bus));
    CHECK(ui2c_cancel(&bus) == UI2C_STATUS_OK && aborts == 2 && calls == 2);
    ui2c_port_complete(&bus, UI2C_STATUS_OK);
    ui2c_process(&bus);
    CHECK(calls == 3 && result == UI2C_STATUS_CANCELLED && callback_error == 0);
    CHECK(ui2c_cancel(&bus) == UI2C_STATUS_OK && aborts == 2);
    test_primask = 1;
    CHECK(!ui2c_busy(&bus) && test_primask == 1);
    test_primask = 0;
    cases++;
    return 0;
}

static int hardware_lifecycle(void) {
    CHECK(setup(true) == 0);
    ui2c_msg_t msg = {.data.rx = rx, .len = 32, .flags = UI2C_MSG_READ};
    hw.ISR = I2C_ISR_BUSY;
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_BUS_BUSY);
    hw.ISR = 0;
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    event(I2C_ISR_STOPF); /* DMA final write still pending. */
    CHECK(port.bus.state == UI2C_ACTIVE);
    now = 100;
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_TIMEOUT && !(DMA1_Channel3->CCR & DMA_CCR_EN));
    test_dma.ISR = 2U << 8;
    ui2c_stm32f0_dma_irq(&port);
    event(I2C_ISR_STOPF);
    ui2c_process(&port.bus);
    CHECK(calls == 1);
    test_dma.ISR = 0;
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    test_primask = 1;
    ui2c_cancel(&port.bus);
    CHECK(test_primask == 1 && !(DMA1_Channel3->CCR & DMA_CCR_EN));
    test_primask = 0;
    ui2c_process(&port.bus);
    CHECK(calls == 2 && result == UI2C_STATUS_CANCELLED);
    cases++;
    return 0;
}

static int simultaneous_events(void) {
    CHECK(setup(false) == 0);
    ui2c_msg_t msg = {.data.rx = rx, .len = 1, .flags = UI2C_MSG_READ};
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    hw.RXDR = 0x93;
    event(I2C_ISR_RXNE | I2C_ISR_STOPF);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_OK && rx[0] == 0x93);
    CHECK(setup(true) == 0);
    msg.len = 32;
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    test_dma.ISR = 8U << 8;
    event(I2C_ISR_ARLO | I2C_ISR_STOPF);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_ARBITRATION_LOST);
    CHECK(!(hw.CR2 & I2C_CR2_STOP) && !(DMA1_Channel3->CCR & DMA_CCR_EN));
    CHECK(setup(true) == 0);
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    test_dma.ISR = 8U << 8;
    hw.ISR = I2C_ISR_ARLO | I2C_ISR_STOPF;
    ui2c_stm32f0_dma_irq(&port);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_ARBITRATION_LOST);
    CHECK(!(hw.CR2 & I2C_CR2_STOP) && !(DMA1_Channel3->CCR & DMA_CCR_EN));
    CHECK(setup(true) == 0);
    CHECK(ui2c_xfer(&port.bus, 0x76, &msg, 1, completed, &calls) == UI2C_STATUS_OK);
    /* Wire event before DMA IRQ must still notice DMA transfer error. */
    test_dma.ISR = 8U << 8;
    event(I2C_ISR_STOPF);
    ui2c_process(&port.bus);
    CHECK(calls == 1 && result == UI2C_STATUS_DMA_ERROR);
    cases += 4;
    return 0;
}

static int initialization(void) {
    ui2c_t bus = {0};
    ui2c_config_t config = {.time_ms = clock_ms};
    CHECK(ui2c_port_init(&bus, &fake_ops, NULL, &config, 0) == UI2C_STATUS_INVALID_ARG);
    config.timeout_ms = UINT32_MAX;
    CHECK(ui2c_port_init(&bus, &fake_ops, NULL, &config, 0) == UI2C_STATUS_INVALID_ARG);
    config.timeout_ms = 100;
    ui2c_port_ops_t invalid_ops = fake_ops;
    invalid_ops.abort = NULL;
    CHECK(ui2c_port_init(&bus, &invalid_ops, NULL, &config, 0) == UI2C_STATUS_INVALID_ARG);
    CHECK(ui2c_port_init(&bus, &fake_ops, NULL, &config, 0) == UI2C_STATUS_OK);
    CHECK(ui2c_xfer(&bus, 0x76, &probe, 1, completed, &calls) == UI2C_STATUS_UNSUPPORTED);
    CHECK(ui2c_port_init(&bus, &fake_ops, NULL, &config, 0) == UI2C_STATUS_BUSY);
    CHECK(setup(false) == 0);
    ui2c_stm32f0_t other = {0};
    ui2c_stm32f0_config_t hw_config = {.instance = &hw, .tx_dma = {DMA1_Channel2, 3}};
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_INVALID_ARG);
    hw_config.tx_dma.number = 2;
    hw_config.rx_dma = hw_config.tx_dma;
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_INVALID_ARG);
    hw_config.rx_dma = (ui2c_stm32f0_dma_t){0};
    DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_DIR;
    hw.ISR = I2C_ISR_BUSY;
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_BUS_BUSY);
    hw.ISR = 0;
    hw.CR1 = 0;
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_NOT_INITIALIZED);
    CHECK(other.bus.ops == NULL);
    hw.CR1 = I2C_CR1_PE;
    const uint32_t invalid_dma[] = {
        DMA_CCR_CIRC, DMA_CCR_PINC, DMA_CCR_PSIZE_0, DMA_CCR_MSIZE_0, DMA_CCR_MEM2MEM, DMA_CCR_EN,
    };
    for (size_t i = 0; i < sizeof invalid_dma / sizeof invalid_dma[0]; ++i) {
        DMA1_Channel2->CCR = DMA_CCR_MINC | DMA_CCR_DIR | invalid_dma[i];
        CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_INVALID_ARG);
        CHECK(other.bus.ops == NULL);
    }
    DMA1_Channel2->CCR = DMA_CCR_MINC; /* Wrong TX direction. */
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_INVALID_ARG);
    DMA1_Channel2->CCR = DMA_CCR_DIR; /* Missing memory increment. */
    CHECK(ui2c_stm32f0_init(&other, &hw_config, &config) == UI2C_STATUS_INVALID_ARG);
    cases++;
    return 0;
}

#ifdef UI2C_TEST_DLL
__declspec(dllexport)
#endif
int ui2c_tests(void) {
    cases = 0;
    int err = core_contract();
    if (err) {
        return err;
    }
    err = protocol_matrix();
    if (err) {
        return err;
    }
    err = errors();
    if (err) {
        return err;
    }
    err = hardware_lifecycle();
    if (err) {
        return err;
    }
    err = simultaneous_events();
    if (err) {
        return err;
    }
    return initialization();
}
#ifdef UI2C_TEST_DLL
__declspec(dllexport) unsigned ui2c_test_cases(void) { return cases; }
#else
int main(void) {
    int err = ui2c_tests();
    if (err) {
        printf("FAIL test_ui2c.c:%d (%u completed cases)\n", err, cases);
        return 1;
    }
    printf("PASS: %u protocol/lifecycle cases\n", cases);
    return 0;
}
#endif
