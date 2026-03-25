#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"
#include "hid_desc.h"
#include "gpio_cfg.h"

//Macro
#define BIT_INPUT(var, pin, pos) var |= (gpio_get_level(pin) << pos)

static const char *TAG = "Control";

//Global Variable
uint16_t input_pre_st = 0;

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    printf("RAW USB DATA: ID=%d, Type=%d, Len=%d, Data[0]=%d\n", 
            report_id, report_type, bufsize, buffer[0]);

    ESP_LOGI("HID_DEBUG", "Received ID: %d, Type: %d, Size: %d", report_id, report_type, bufsize);

    if (report_type == HID_REPORT_TYPE_FEATURE && report_id == 1) 
    {
        uint8_t gear_state = buffer[0]; 
        
        if (gear_state == 1) {
            ESP_LOGI("GEAR", "X-Plane: Gear is DOWN");
        } else {
            ESP_LOGI("GEAR", "X-Plane: Gear is UP");
        }
    }
}

void send_report(uint8_t data)
{
    data ^= 0x7F; //XOR for pull-upped inputs (0x7F is for 7 bits 01111111)
    tud_hid_report(1, &data, 1);
}

void get_toggle_sw(void) {
    uint8_t input_cur_st = 0;
    
    BIT_INPUT(input_cur_st, INPUT_SW_1, 0);
    BIT_INPUT(input_cur_st, INPUT_SW_2, 1);
    BIT_INPUT(input_cur_st, INPUT_SW_3, 2);
    BIT_INPUT(input_cur_st, INPUT_SW_4, 3);
    BIT_INPUT(input_cur_st, INPUT_TAC_1, 4);
    BIT_INPUT(input_cur_st, INPUT_TAC_2, 5);
    BIT_INPUT(input_cur_st, INPUT_TAC_3, 6);

    if (!(input_cur_st == input_pre_st)) {
        input_pre_st = input_cur_st;
        send_report(input_cur_st);
    }
}

void app_main(void)
{
    cfg_gpio();
    
    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = &hid_device_descriptor;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) {
        get_toggle_sw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}