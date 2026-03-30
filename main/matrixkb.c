#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define bit_map(var, pin, position) var |= (gpio_get_level(pin) << position)

uint16_t prev_state = 0;


// Pin Configuration and Settings

    // columns are high; rows are reading

#define OFFSET 10 //10C7R Layout

#define colBut(x) \
        x(40)     \
        x(21)     \
        x(47)     \
        x(41)     \
        x(45)     \
        x(0)      \
        x(35)     \
        x(36)     \
        x(37)     \
        x(39)

#define rowBut(x) \
        x(1)

const uint8_t colList[] = {
    #define x(val) val,
    colBut(x)
    #undef x
};

const uint8_t rowList[] = {
    #define x(val) val,
    rowBut(x)
    #undef x
};

uint8_t colSize = sizeof(colList)/sizeof(colList[0]), rowSize = sizeof(rowList)/sizeof(rowList[0]);

void cfg_gpio(void)
{
    uint64_t gpio_row = 0, gpio_col = 0;

    for (uint8_t i = 0; i < rowSize; i++){
        gpio_row |= (1ULL << rowList[i]);
    }

    for (uint8_t i = 0; i < colSize; i++){
        gpio_col |= (1ULL << colList[i]);
    }

    gpio_config_t gpio_active = {
        .pin_bit_mask = gpio_col,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    gpio_config_t gpio_inactive = {
        .pin_bit_mask = gpio_row,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };

    gpio_config(&gpio_active);
    gpio_config(&gpio_inactive);
}


static const char *TAG = "example";

/* Flag to indicate if the host has suspended the USB bus */
static bool suspended = false;
/* Flag of possibility to Wakeup Host via Remote Wakeup feature */
static bool wakeup_host = false;

/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE))
};

/**
 * @brief String descriptor
 */
const char *hid_string_descriptor[5] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "TinyUSB",             // 1: Manufacturer
    "TinyUSB Device",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "Example HID interface",  // 4: HID
};

/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
}

void manual(void){
    for (uint8_t i = 0; i < colSize; i++){
        gpio_set_level(colList[i], 1);
    }
}

uint16_t check_changes(void){
    uint16_t cur_state = 0;

    manual();
    //ESP_LOGI(TAG, "~~~~~~~~~~~~~~~~~~");
    for (uint8_t c = 0; c < colSize; c++){
        gpio_set_level(colList[c], 0);
        for (uint8_t i = 0; i < rowSize; i++){
            uint8_t vars = i+c;
            //ESP_LOGI(TAG, "state=%d, num=%d, c=%d, row=%d", gpio_get_level(rowList[i]), vars, c, rowList[i]);
            bit_map(cur_state, rowList[i], vars);
        }
        gpio_set_level(colList[c], 1);
    }
    
    if(prev_state != cur_state){
        ESP_LOGI(TAG, "CHANGE DETECTED (%d > %d)", prev_state, cur_state);
        prev_state = cur_state;
        return prev_state;
    }
    return 0;
}

/*
void get_pressed_button(bitmap){
    // 0x

}*/

/********* Application ***************/


static void app_send_hid_demo(void)
{
    // Keyboard output: Send key 'a/A' pressed and released
    ESP_LOGI(TAG, "Sending Keyboard report");
    uint8_t keycode[6] = {HID_KEY_A};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
    vTaskDelay(pdMS_TO_TICKS(50));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGI(TAG, "USB device suspended");
    suspended = true;
    if (remote_wakeup_en) {
        ESP_LOGI(TAG, "Remote wakeup available, press the button to wake up the Host");
        wakeup_host = true;
    } else {
        ESP_LOGI(TAG, "Remote wakeup not available");
    }
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB device resumed");
    suspended = false;
}

void app_main(void)
{
    cfg_gpio();

    ESP_LOGI(TAG, "USB initialization");
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = NULL;
    tusb_cfg.descriptor.full_speed_config = hid_configuration_descriptor;
    tusb_cfg.descriptor.string = hid_string_descriptor;
    tusb_cfg.descriptor.string_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]);

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) {
        check_changes();
        vTaskDelay(pdMS_TO_TICKS(30));

        /*
        if (tud_mounted()) {
            static bool send_hid_data = true;
            if (send_hid_data) {
                if (!suspended) {
                    app_send_hid_demo();
                } else {
                    if (wakeup_host) {
                        ESP_LOGI(TAG, "Waking up the Host");
                        tud_remote_wakeup();
                        wakeup_host = false;
                    } else {
                        ESP_LOGI(TAG, "USB Host remote wakeup is not available.");
                    }
                }
            }
            send_hid_data = !gpio_get_level(APP_BUTTON);
        }
        vTaskDelay(pdMS_TO_TICKS(100));*/
    }
}