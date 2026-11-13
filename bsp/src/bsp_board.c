#include "bsp_board.h"
#include "bsp_hw.h"
#include "bsp_pins.h"

/* Check the calculations before narrowing their results to peripheral registers. */
_Static_assert(1000000UL <= PLATFORM_HCLK_HZ, "The microsecond counter clock must be achievable");
_Static_assert(BSP_TIM_US_PSC <= TIM_PSC_PSC && PLATFORM_HCLK_HZ % (BSP_TIM_US_PSC + 1U) == 0U
                   && PLATFORM_HCLK_HZ / (BSP_TIM_US_PSC + 1U) == 1000000UL,
               "The microsecond prescaler must fit 16 bits and produce the exact counter clock");
_Static_assert(BSP_BOARD_BUZZER_INITIAL_RATE_HZ > 0U
                   && PLATFORM_HCLK_HZ % ((BSP_TIM_US_PSC + 1U) * BSP_BOARD_BUZZER_INITIAL_RATE_HZ) == 0U
                   && BSP_TIM16_ARR > 0U && BSP_TIM16_ARR <= UINT16_MAX,
               "The initial TIM16 period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_ADC_TARGET_RATE_HZ > 0U
                   && PLATFORM_HCLK_HZ % BSP_BOARD_HV_ADC_TARGET_RATE_HZ == 0U
                   && BSP_TIM3_ARR > 0U && BSP_TIM3_ARR <= UINT16_MAX
                   && TIMER_CALC_RATE_HZ(PLATFORM_HCLK_HZ, 0U, BSP_TIM3_ARR)
                          == BSP_BOARD_HV_ADC_TARGET_RATE_HZ,
               "The TIM3 ADC trigger period must be exact and fit its 16-bit counter");
_Static_assert(BSP_BOARD_HV_PWM_RATE_HZ > 0U && PLATFORM_HCLK_HZ % BSP_BOARD_HV_PWM_RATE_HZ == 0U
                   && BSP_TIM17_ARR > 0U && BSP_TIM17_ARR <= UINT16_MAX,
               "The TIM17 PWM period must be exact and fit its 16-bit counter");
_Static_assert((BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT) > 0U
                   && PLATFORM_HCLK_HZ % (BSP_BOARD_WS2812_BIT_RATE_HZ * BSP_BOARD_WS2812_SYMBOLS_PER_BIT) == 0U
                   && BSP_TIM2_ARR > 0U && BSP_TIM2_ARR <= TIM_ARR_ARR && (BSP_TIM2_ARR + 1U) % 2U == 0U,
               "The TIM2 DMA slot must fit its 32-bit counter and have an exact midpoint");

static void analog_pins(GPIO_TypeDef* gpio, uint32_t pins) {
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            gpio->MODER |= GPIO_MODER_MODER0 << (pin * GPIO_MODER_MODER1_Pos);
            gpio->PUPDR &= ~(GPIO_PUPDR_PUPDR0 << (pin * GPIO_PUPDR_PUPDR1_Pos));
        }
    }
}

static void output_pins(GPIO_TypeDef* gpio, uint32_t pins) {
    /* MODER/PUPDR/OSPEEDR use the same two-bit field positions on STM32F051. */
    uint32_t fields = 0U;
    uint32_t modes = 0U;
    for (uint32_t pin = 0U; pin < 16U; ++pin) {
        if ((pins & (1U << pin)) != 0U) {
            fields |= GPIO_MODER_MODER0 << (pin * GPIO_MODER_MODER1_Pos);
            modes |= GPIO_MODER_MODER0_0 << (pin * GPIO_MODER_MODER1_Pos);
        }
    }
    gpio->OTYPER &= ~pins;
    gpio->PUPDR &= ~fields;
    gpio->OSPEEDR |= fields;
    gpio->MODER = (gpio->MODER & ~fields) | modes;
}

static void gpio_init(void) {
    RCC->AHBENR |=
        RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN | RCC_AHBENR_GPIOCEN | RCC_AHBENR_GPIODEN | RCC_AHBENR_GPIOFEN;
    (void)RCC->AHBENR;
    analog_pins(GPIOF, GPIO_ODR_0 | GPIO_ODR_1 | GPIO_ODR_4 | GPIO_ODR_5 | GPIO_ODR_6 | GPIO_ODR_7);
    analog_pins(GPIOA, GPIO_ODR_0 | GPIO_ODR_1 | GPIO_ODR_3 | GPIO_ODR_4 | GPIO_ODR_8 | GPIO_ODR_11 | GPIO_ODR_12
                           | GPIO_ODR_15);

    /* The .ioc starts all eight anodes and sixteen cathodes LOW.
     * Preload every latch, then configure the electrical fields before MODER. */
    uint32_t outputs_a = BSP_NIXIE_ANODE_A_MASK | BSP_NIXIE_CATHODE_A_MASK;
    uint32_t outputs_b = BSP_NIXIE_ANODE_B_MASK | BSP_NIXIE_CATHODE_B_MASK;
    uint32_t outputs_c = BSP_NIXIE_ANODE_C_MASK | BSP_NIXIE_CATHODE_C_MASK;
    GPIOA->BSRR = outputs_a << 16U;
    GPIOB->BSRR = outputs_b << 16U;
    GPIOC->BSRR = outputs_c << 16U;
    output_pins(GPIOA, outputs_a);
    output_pins(GPIOB, outputs_b);
    output_pins(GPIOC, outputs_c);

    /* HIGH keeps the inverting HV gate driver off before TIM17 takes over. */
    GPIOA->BSRR = GPIO_BSRR_BS_7;
    GPIOA->OTYPER &= ~GPIO_OTYPER_OT_7;
    GPIOA->PUPDR &= ~GPIO_PUPDR_PUPDR7;
    GPIOA->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR7;
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODER7) | GPIO_MODER_MODER7_0;
    GPIOC->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER3);
    GPIOC->PUPDR = (GPIOC->PUPDR & ~(GPIO_PUPDR_PUPDR0 | GPIO_PUPDR_PUPDR1 | GPIO_PUPDR_PUPDR2 | GPIO_PUPDR_PUPDR3))
                   | GPIO_PUPDR_PUPDR0_0 | GPIO_PUPDR_PUPDR1_0 | GPIO_PUPDR_PUPDR2_0 | GPIO_PUPDR_PUPDR3_0;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0]
                         & ~(SYSCFG_EXTICR1_EXTI0 | SYSCFG_EXTICR1_EXTI1 | SYSCFG_EXTICR1_EXTI2 | SYSCFG_EXTICR1_EXTI3))
                        | SYSCFG_EXTICR1_EXTI0_PC | SYSCFG_EXTICR1_EXTI1_PC | SYSCFG_EXTICR1_EXTI2_PC
                        | SYSCFG_EXTICR1_EXTI3_PC;
    EXTI->RTSR |= EXTI_RTSR_TR0 | EXTI_RTSR_TR1 | EXTI_RTSR_TR2 | EXTI_RTSR_TR3;
    EXTI->FTSR |= EXTI_FTSR_TR0 | EXTI_FTSR_TR1 | EXTI_FTSR_TR2 | EXTI_FTSR_TR3;
    EXTI->PR = EXTI_PR_PR0 | EXTI_PR_PR1 | EXTI_PR_PR2 | EXTI_PR_PR3;
    EXTI->IMR |= EXTI_IMR_MR0 | EXTI_IMR_MR1 | EXTI_IMR_MR2 | EXTI_IMR_MR3;

    /* Preserve the board's open-drain WS2812 output. */
    GPIOD->BSRR = GPIO_BSRR_BR_2;
    GPIOD->OTYPER |= GPIO_OTYPER_OT_2;
    GPIOD->OSPEEDR |= GPIO_OSPEEDR_OSPEEDR2;
    GPIOD->PUPDR &= ~GPIO_PUPDR_PUPDR2;
    GPIOD->MODER = (GPIOD->MODER & ~GPIO_MODER_MODER2) | GPIO_MODER_MODER2_0;
    NVIC_SetPriority(EXTI0_1_IRQn, BSP_IRQ_PRIORITY_BACKGROUND);
    NVIC_EnableIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn, BSP_IRQ_PRIORITY_BACKGROUND);
    NVIC_EnableIRQ(EXTI2_3_IRQn);
}

static void dma_init(void) {
    RCC->AHBENR |= RCC_AHBENR_DMA1EN;
    (void)RCC->AHBENR;
    NVIC_SetPriority(DMA1_Channel1_IRQn, BSP_IRQ_PRIORITY_WS2812);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    NVIC_SetPriority(DMA1_Channel2_3_IRQn, BSP_IRQ_PRIORITY_IO);
    NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
    NVIC_SetPriority(DMA1_Channel4_5_IRQn, BSP_IRQ_PRIORITY_IO);
    NVIC_EnableIRQ(DMA1_Channel4_5_IRQn);
}

void bsp_board_init(void) {
    /* Preserve the original preparation order, before any runtime driver starts. */
    gpio_init();
    dma_init();
    bsp_rtc_hw_init();
    bsp_console_hw_init();
    bsp_i2c_hw_init();
    bsp_buzzer_hw_init();
    bsp_ws2812_hw_init();
    bsp_hv_hw_init();
    bsp_nixie_hw_init();
}
