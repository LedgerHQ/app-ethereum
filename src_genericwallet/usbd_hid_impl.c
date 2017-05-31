/**
  ******************************************************************************
  * @file    usbd_hid.c
  * @author  MCD Application Team
  * @version V2.2.0
  * @date    13-June-2014
  * @brief   This file provides the HID core functions.
  *
  * @verbatim
  *
  *          ===================================================================
  *                                HID Class  Description
  *          ===================================================================
  *           This module manages the HID class V1.11 following the "Device
  *Class Definition
  *           for Human Interface Devices (HID) Version 1.11 Jun 27, 2001".
  *           This driver implements the following aspects of the specification:
  *             - The Boot Interface Subclass
  *             - Usage Page : Generic Desktop
  *             - Usage : Vendor
  *             - Collection : Application
  *
  * @note     In HS mode and when the DMA is used, all variables and data
  *structures
  *           dealing with the DMA during the transaction process should be
  *32-bit aligned.
  *
  *
  *  @endverbatim
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */
#include "os.h"

/* Includes ------------------------------------------------------------------*/
#include "usbd_hid.h"
#include "usbd_ctlreq.h"

#include "usbd_core.h"
#include "usbd_conf.h"

#include "usbd_def.h"
#include "os_io_seproxyhal.h"

#include "u2f_service.h"
#include "u2f_transport.h"

/** @togroup STM32_USB_DEVICE_LIBRARY
  * @{
  */

/** @defgroup USBD_HID
  * @brief usbd core module
  * @{
  */

/** @defgroup USBD_HID_Private_TypesDefinitions
  * @{
  */
/**
  * @}
  */

/** @defgroup USBD_HID_Private_Defines
  * @{
  */

/**
  * @}
  */

/** @defgroup USBD_HID_Private_Macros
  * @{
  */
/**
  * @}
  */
/** @defgroup USBD_HID_Private_FunctionPrototypes
  * @{
  */


/**
  * @}
  */

/** @defgroup USBD_HID_Private_Variables
  * @{
  */

#define HID_EPIN_ADDR 0x82
#define HID_EPIN_SIZE 0x40

#define HID_EPOUT_ADDR 0x02
#define HID_EPOUT_SIZE 0x40

#define USBD_LANGID_STRING 0x409

#ifdef HAVE_VID_PID_PROBER
#define USBD_VID 0x2581
#define USBD_PID 0xf1d1
#else
#define USBD_VID 0x2C97
#if defined(TARGET_BLUE) // blue
#define USBD_PID 0x0000
const uint8_t const USBD_PRODUCT_FS_STRING[] = {
    4 * 2 + 2, USB_DESC_TYPE_STRING, 'B', 0, 'l', 0, 'u', 0, 'e', 0,
};

#elif defined(TARGET_NANOS) // nano s
#define USBD_PID 0x0001
const uint8_t const USBD_PRODUCT_FS_STRING[] = {
    6 * 2 + 2, USB_DESC_TYPE_STRING,
    'N',       0,
    'a',       0,
    'n',       0,
    'o',       0,
    ' ',       0,
    'S',       0,
};
#elif defined(TARGET_ARAMIS) // aramis
#define USBD_PID 0x0002
const uint8_t const USBD_PRODUCT_FS_STRING[] = {
    6 * 2 + 2, USB_DESC_TYPE_STRING,
    'A',       0,
    'r',       0,
    'a',       0,
    'm',       0,
    'i',       0,
    's',       0,
};
#else
#error unknown TARGET_ID
#endif
#endif

/* USB Standard Device Descriptor */
const uint8_t const USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] = {
    USB_LEN_LANGID_STR_DESC, USB_DESC_TYPE_STRING, LOBYTE(USBD_LANGID_STRING),
    HIBYTE(USBD_LANGID_STRING),
};

const uint8_t const USB_SERIAL_STRING[] = {
    4 * 2 + 2, USB_DESC_TYPE_STRING, '0', 0, '0', 0, '0', 0, '1', 0,
};

const uint8_t const USBD_MANUFACTURER_STRING[] = {
    6 * 2 + 2, USB_DESC_TYPE_STRING,
    'L',       0,
    'e',       0,
    'd',       0,
    'g',       0,
    'e',       0,
    'r',       0,
};

#define USBD_INTERFACE_FS_STRING USBD_PRODUCT_FS_STRING
#define USBD_CONFIGURATION_FS_STRING USBD_PRODUCT_FS_STRING

const uint8_t const HID_ReportDesc[] = {
    0x06, 0xD0, 0xF1, // Usage page (vendor defined)
    0x09, 0x01,       // Usage ID (vendor defined)
    0xA1, 0x01,       // Collection (application)

    // The Input report
    0x09, 0x03,          // Usage ID - vendor defined
    0x15, 0x00,          // Logical Minimum (0)
    0x26, 0xFF, 0x00,    // Logical Maximum (255)
    0x75, 0x08,          // Report Size (8 bits)
    0x95, HID_EPIN_SIZE, // Report Count (64 fields)
    0x81, 0x08,          // Input (Data, Variable, Absolute)

    // The Output report
    0x09, 0x04,           // Usage ID - vendor defined
    0x15, 0x00,           // Logical Minimum (0)
    0x26, 0xFF, 0x00,     // Logical Maximum (255)
    0x75, 0x08,           // Report Size (8 bits)
    0x95, HID_EPOUT_SIZE, // Report Count (64 fields)
    0x91, 0x08,           // Output (Data, Variable, Absolute)
    0xC0};

#define PAGE_FIDO 0xF1D0
#define PAGE_GENERIC 0xFFA0

uint8_t HID_DynReportDesc[sizeof(HID_ReportDesc)];
bool fidoActivated;

/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN const uint8_t const USBD_HID_CfgDesc[] __ALIGN_END = {
    0x09,                        /* bLength: Configuration Descriptor size */
    USB_DESC_TYPE_CONFIGURATION, /* bDescriptorType: Configuration */
    0x29,
    /* wTotalLength: Bytes returned */
    0x00, 0x01,           /*bNumInterfaces: 1 interface*/
    0x01,                 /*bConfigurationValue: Configuration value*/
    USBD_IDX_PRODUCT_STR, /*iConfiguration: Index of string descriptor
describing
the configuration*/
    0xC0,                 /*bmAttributes: bus powered */
    0x32, /*MaxPower 100 mA: this current is used for detecting Vbus*/

    /************** Descriptor of CUSTOM HID interface ****************/
    /* 09 */
    0x09,                    /*bLength: Interface Descriptor size*/
    USB_DESC_TYPE_INTERFACE, /*bDescriptorType: Interface descriptor type*/
    0x00,                    /*bInterfaceNumber: Number of Interface*/
    0x00,                    /*bAlternateSetting: Alternate setting*/
    0x02,                    /*bNumEndpoints*/
    0x03,                    /*bInterfaceClass: HID*/
    0x00,                    /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
    0x00,                 /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
    USBD_IDX_PRODUCT_STR, /*iInterface: Index of string descriptor*/
    /******************** Descriptor of HID *************************/
    /* 18 */
    0x09,                /*bLength: HID Descriptor size*/
    HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
    0x11,                /*bHIDUSTOM_HID: HID Class Spec release number*/
    0x01,
    0x00, /*bCountryCode: Hardware target country*/
    0x01, /*bNumDescriptors: Number of HID class descriptors to follow*/
    0x22, /*bDescriptorType*/
    sizeof(
        HID_DynReportDesc), /*wItemLength: Total length of Report descriptor*/
    0x00,
    /******************** Descriptor of Custom HID endpoints
       ********************/
    /* 27 */
    0x07,                   /*bLength: Endpoint Descriptor size*/
    USB_DESC_TYPE_ENDPOINT, /*bDescriptorType:*/
    HID_EPIN_ADDR,          /*bEndpointAddress: Endpoint Address (IN)*/
    0x03,                   /*bmAttributes: Interrupt endpoint*/
    HID_EPIN_SIZE,          /*wMaxPacketSize: 2 Byte max */
    0x00,
    0x01, /*bInterval: Polling Interval (20 ms)*/
    /* 34 */

    0x07,                   /* bLength: Endpoint Descriptor size */
    USB_DESC_TYPE_ENDPOINT, /* bDescriptorType: */
    HID_EPOUT_ADDR,         /*bEndpointAddress: Endpoint Address (OUT)*/
    0x03,                   /* bmAttributes: Interrupt endpoint */
    HID_EPOUT_SIZE,         /* wMaxPacketSize: 2 Bytes max  */
    0x00, 0x01,             /* bInterval: Polling Interval (20 ms) */
                            /* 41 */
};

/* USB HID device Configuration Descriptor */
__ALIGN_BEGIN const uint8_t const USBD_HID_Desc[] __ALIGN_END = {
    /* 18 */
    0x09,                /*bLength: HID Descriptor size*/
    HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
    0x11,                /*bHIDUSTOM_HID: HID Class Spec release number*/
    0x01, 0x00,          /*bCountryCode: Hardware target country*/
    0x01, /*bNumDescriptors: Number of HID class descriptors to follow*/
    0x22, /*bDescriptorType*/
    sizeof(
        HID_DynReportDesc), /*wItemLength: Total length of Report descriptor*/
    0x00,
};

/* USB Standard Device Descriptor */
__ALIGN_BEGIN const uint8_t const USBD_HID_DeviceQualifierDesc[] __ALIGN_END = {
    USB_LEN_DEV_QUALIFIER_DESC,
    USB_DESC_TYPE_DEVICE_QUALIFIER,
    0x00,
    0x02,
    0x00,
    0x00,
    0x00,
    0x40,
    0x01,
    0x00,
};

/* USB Standard Device Descriptor */
const uint8_t const USBD_DeviceDesc[USB_LEN_DEV_DESC] = {
    0x12,                 /* bLength */
    USB_DESC_TYPE_DEVICE, /* bDescriptorType */
    0x00,                 /* bcdUSB */
    0x02,
    0x00,             /* bDeviceClass */
    0x00,             /* bDeviceSubClass */
    0x00,             /* bDeviceProtocol */
    USB_MAX_EP0_SIZE, /* bMaxPacketSize */
    LOBYTE(USBD_VID), /* idVendor */
    HIBYTE(USBD_VID), /* idVendor */
    LOBYTE(USBD_PID), /* idVendor */
    HIBYTE(USBD_PID), /* idVendor */
    0x00,             /* bcdDevice rel. 2.00 */
    0x02,
    USBD_IDX_MFC_STR,          /* Index of manufacturer string */
    USBD_IDX_PRODUCT_STR,      /* Index of product string */
    USBD_IDX_SERIAL_STR,       /* Index of serial number string */
    USBD_MAX_NUM_CONFIGURATION /* bNumConfigurations */
};                             /* USB_DeviceDescriptor */

/**
  * @brief  Returns the device descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_DeviceDesc);
    return (uint8_t *)USBD_DeviceDesc;
}

/**
  * @brief  Returns the LangID string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_LangIDStrDescriptor(USBD_SpeedTypeDef speed,
                                      uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_LangIDDesc);
    return (uint8_t *)USBD_LangIDDesc;
}

/**
  * @brief  Returns the product string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_ProductStrDescriptor(USBD_SpeedTypeDef speed,
                                       uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_PRODUCT_FS_STRING);
    return (uint8_t *)USBD_PRODUCT_FS_STRING;
}

/**
  * @brief  Returns the manufacturer string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed,
                                            uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_MANUFACTURER_STRING);
    return (uint8_t *)USBD_MANUFACTURER_STRING;
}

/**
  * @brief  Returns the serial number string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_SerialStrDescriptor(USBD_SpeedTypeDef speed,
                                      uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USB_SERIAL_STRING);
    return (uint8_t *)USB_SERIAL_STRING;
}

/**
  * @brief  Returns the configuration string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_ConfigStrDescriptor(USBD_SpeedTypeDef speed,
                                      uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_CONFIGURATION_FS_STRING);
    return (uint8_t *)USBD_CONFIGURATION_FS_STRING;
}

/**
  * @brief  Returns the interface string descriptor.
  * @param  speed: Current device speed
  * @param  length: Pointer to data length variable
  * @retval Pointer to descriptor buffer
  */
uint8_t *USBD_HID_InterfaceStrDescriptor(USBD_SpeedTypeDef speed,
                                         uint16_t *length) {
    UNUSED(speed);
    *length = sizeof(USBD_INTERFACE_FS_STRING);
    return (uint8_t *)USBD_INTERFACE_FS_STRING;
}

/**
* @brief  DeviceQualifierDescriptor
*         return Device Qualifier descriptor
* @param  length : pointer data length
* @retval pointer to descriptor buffer
*/
uint8_t *USBD_HID_GetDeviceQualifierDesc_impl(uint16_t *length) {
    *length = sizeof(USBD_HID_DeviceQualifierDesc);
    return (uint8_t *)USBD_HID_DeviceQualifierDesc;
}

/**
  * @brief  USBD_CUSTOM_HID_GetCfgDesc
  *         return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
uint8_t *USBD_HID_GetCfgDesc_impl(uint16_t *length) {
    *length = sizeof(USBD_HID_CfgDesc);
    return (uint8_t *)USBD_HID_CfgDesc;
}

uint8_t *USBD_HID_GetHidDescriptor_impl(uint16_t *len) {
    *len = sizeof(USBD_HID_Desc);
    return (uint8_t *)USBD_HID_Desc;
}

uint8_t *USBD_HID_GetReportDescriptor_impl(uint16_t *len) {
    *len = sizeof(HID_DynReportDesc);
    return (uint8_t *)HID_DynReportDesc;
}

/**
  * @}
  */

/**
  * @brief  USBD_HID_DataOut
  *         handle data OUT Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  *
  * This function is the default behavior for our implementation when data are
 * sent over the out hid endpoint
  */
extern volatile unsigned short G_io_apdu_length;

uint8_t USBD_HID_DataOut_impl(USBD_HandleTypeDef *pdev, uint8_t epnum,
                              uint8_t *buffer) {
    UNUSED(epnum);

    // prepare receiving the next chunk (masked time)
    USBD_LL_PrepareReceive(pdev, HID_EPOUT_ADDR, HID_EPOUT_SIZE);

    if (fidoActivated) {
#ifdef HAVE_U2F
        u2f_transport_handle(&u2fService, buffer,
                             io_seproxyhal_get_ep_rx_size(HID_EPOUT_ADDR),
                             U2F_MEDIA_USB);
#endif
    } else {
        // avoid troubles when an apdu has not been replied yet
        if (G_io_apdu_media == IO_APDU_MEDIA_NONE) {
            // add to the hid transport
            switch (io_usb_hid_receive(
                io_usb_send_apdu_data, buffer,
                io_seproxyhal_get_ep_rx_size(HID_EPOUT_ADDR))) {
            default:
                break;

            case IO_USB_APDU_RECEIVED:
                G_io_apdu_media = IO_APDU_MEDIA_USB_HID; // for application code
                G_io_apdu_state = APDU_USB_HID; // for next call to io_exchange
                G_io_apdu_length = G_io_usb_hid_total_length;
                break;
            }
        }
    }

    return USBD_OK;
}

/** @defgroup USBD_HID_Private_Functions
  * @{
  */

// note: how core lib usb calls the hid class
static const USBD_DescriptorsTypeDef const HID_Desc = {
    USBD_HID_DeviceDescriptor,          USBD_HID_LangIDStrDescriptor,
    USBD_HID_ManufacturerStrDescriptor, USBD_HID_ProductStrDescriptor,
    USBD_HID_SerialStrDescriptor,       USBD_HID_ConfigStrDescriptor,
    USBD_HID_InterfaceStrDescriptor,    NULL,
};

static const USBD_ClassTypeDef const USBD_HID = {
    USBD_HID_Init, USBD_HID_DeInit, USBD_HID_Setup, NULL, /*EP0_TxSent*/
    NULL, /*EP0_RxReady*/                                 /* STATUS STAGE IN */
    NULL,                                                 /*DataIn*/
    USBD_HID_DataOut_impl,                                /*DataOut*/
    NULL,                                                 /*SOF */
    NULL, NULL, USBD_HID_GetCfgDesc_impl, USBD_HID_GetCfgDesc_impl,
    USBD_HID_GetCfgDesc_impl, USBD_HID_GetDeviceQualifierDesc_impl,
};

void USB_power_U2F(unsigned char enabled, unsigned char fido) {
    uint16_t page = (fido ? PAGE_FIDO : PAGE_GENERIC);
    os_memmove(HID_DynReportDesc, HID_ReportDesc, sizeof(HID_ReportDesc));
    HID_DynReportDesc[1] = (page & 0xff);
    HID_DynReportDesc[2] = ((page >> 8) & 0xff);
    fidoActivated = (fido ? true : false);

    os_memset(&USBD_Device, 0, sizeof(USBD_Device));

    if (enabled) {
        os_memset(&USBD_Device, 0, sizeof(USBD_Device));
        /* Init Device Library */
        USBD_Init(&USBD_Device, (USBD_DescriptorsTypeDef *)&HID_Desc, 0);

        /* Register the HID class */
        USBD_RegisterClass(&USBD_Device, (USBD_ClassTypeDef *)&USBD_HID);

        /* Start Device Process */
        USBD_Start(&USBD_Device);
    } else {
        USBD_DeInit(&USBD_Device);
    }
}

/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
