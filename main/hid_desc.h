#ifndef HID_DESCRIPTOR_H
#define HID_DESCRIPTOR_H

#include <stdint.h>

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

static const uint8_t hid_report_descriptor[] = {
    0x05, 0xFF, //  UsagePage(Vendor-defined[0xFF])
    0x09, 0x01, //  UsageId(vendor-page1[0x01])
    0xA1, 0x01, //  Collection(Application)
    0x85, 0x01, //  ReportId(1)

    //inputs
    0x09, 0x01, //  UsageId(GPIO_Button[0x01])
    0x15, 0x00, //  LogicalMinimum(0)
    0x25, 0x01, //  LogicalMaximum(1)
    0x95, 0x07, //  ReportCount(7)
    0x75, 0x01, //  ReportSize(1)
    0x81, 0x02, //  Input(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, BitField)
    //input padding
    0x95, 0x01, //  ReportCount(1)
    0x75, 0x01, //  ReportSize(1)
    0x81, 0x03, //  Input(Constant, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, BitField)
    

    //output section
    0x09, 0x02, //  UsageId for output
    0x15, 0x00, //  LogicalMinimum(0)
    0x25, 0x01, //  LogicalMaximum(1)
    0x95, 0x01, //  ReportCount(1)
    0x75, 0x01, //  ReportSize(1)
    0x91, 0x01, //  Output
    //output padding
    0x95, 0x01, //  ReportCount(1)
    0x75, 0x07, //  ReportSize(7)
    0x91, 0x03, //  Output
    0xC0        //  EndCollection()
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
    .bDeviceClass       = TUSB_CLASS_UNSPECIFIED,
    .bDeviceSubClass    = TUSB_CLASS_UNSPECIFIED,
    .bDeviceProtocol    = TUSB_CLASS_UNSPECIFIED,
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
    TUD_HID_DESCRIPTOR(0, 2, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
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


#endif