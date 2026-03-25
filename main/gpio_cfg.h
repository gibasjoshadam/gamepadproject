#ifndef GPIO_CONFIGS
#define GPIO_CONFIGS

#include <stdint.h>

typedef enum
{
    INPUT_TAC_1 = 4,
    INPUT_TAC_2 = 5,
    INPUT_TAC_3 = 6
} input_button;

typedef enum
{
    INPUT_SW_1 = 35,
    INPUT_SW_2 = 36,
    INPUT_SW_3 = 37,
    INPUT_SW_4 = 38
} input_switch;

void cfg_gpio(void) //configuration of gpio pins
{
    uint64_t GPIO_ULL =
        (1ULL << INPUT_SW_1) |
        (1ULL << INPUT_SW_2) |
        (1ULL << INPUT_SW_3) |
        (1ULL << INPUT_SW_4) |
        (1ULL << INPUT_TAC_1) |
        (1ULL << INPUT_TAC_2) |
        (1ULL << INPUT_TAC_3);

    gpio_config_t gpiocfg = {
        .pin_bit_mask = GPIO_ULL,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&gpiocfg);
}


#endif