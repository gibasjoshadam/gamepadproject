#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define BIT_INPUT(var, pin, pos) var |= (gpio_get_level(pin) << pos)

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

static const char *TAG = "Control";
uint16_t input_pre_st = 0;

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

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

static const uint8_t hid_report_descriptor[] = {
    //Generated with Waratah
    0x05, 0x01,    // UsagePage(Generic Desktop[0x0001])
    0x09, 0x05,    // UsageId(Gamepad[0x0005])
    0xA1, 0x01,    // Collection(Application)
    0x85, 0x01,    //     ReportId(1)
    0x05, 0x09,    //     UsagePage(Button[0x0009])
    0x19, 0x01,    //     UsageIdMin(Button 1[0x0001])
    0x29, 0x0A,    //     UsageIdMax(Button 10[0x000A])
    0x15, 0x00,    //     LogicalMinimum(0)
    0x25, 0x01,    //     LogicalMaximum(1)
    0x95, 0x0A,    //     ReportCount(10)
    0x75, 0x01,    //     ReportSize(1)
    0x81, 0x02,    //     Input(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, BitField)
    0x95, 0x01,    //     ReportCount(1)
    0x75, 0x06,    //     ReportSize(6)
    0x81, 0x03,    //     Input(Constant, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, BitField)
    0xC0           // EndCollection()
};

static const char *hid_string_descriptor[5] = {
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "GLS",             // 1: Manufacturer
    "GLS Gamepad",      // 2: Product
    "676767",              // 3: Serials, should use chip ID
    "Example HID interface",  // 4: HID
};

static const tusb_desc_device_t hid_device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,             // USB 2.0
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0x303A,             // Your Spoofed VID (Espressif)
    .idProduct          = 0x4002,             // Your Spoofed PID (Change this to refresh Windows)
    .bcdDevice          = 0x0104,             // Device Release Number
    
    .iManufacturer      = 0x01,               // Index of Manufacturer String
    .iProduct           = 0x02,               // Index of Product String
    .iSerialNumber      = 0x03,               // Index of Serial Number String
    
    .bNumConfigurations = 0x01
};

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};


uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}


void send_report(uint16_t data)
{
    data ^= 0x7F; //XOR for pull-upped inputs (0x7F is for 7 bits 01111111)
    tud_hid_report(1, &data, 2);
}

void get_toggle_sw(void) {
    uint16_t input_cur_st = 0;
    
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

    get_toggle_sw();
    while (1) {
        get_toggle_sw();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}