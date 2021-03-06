/*

Copyright 2015-2021 Igor Petrovic

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

*/

#include "board/common/usb/descriptors/Descriptors.h"
#include "usbd_core.h"
#include "board/Board.h"
#include "board/Internal.h"

namespace
{
    typedef struct
    {
        uint32_t          data[CDC_NOTIFICATION_EPSIZE / 4U]; /* Force 32bits alignment */
        uint8_t           CmdOpCode;
        uint8_t           CmdLength;
        volatile uint32_t TxLength;
        volatile uint32_t TxState;
        volatile uint32_t RxState;
    } cdcData_t;

    typedef struct
    {
        int8_t (*Control)(uint8_t cmd, uint8_t* pbuf, uint16_t length);
        int8_t (*TransmitCplt)(uint8_t* Buf, uint32_t* Len, uint8_t epnum);
    } cdcCallback_t;

    USBD_HandleTypeDef hUsbDeviceFS;
    cdcData_t          cdcData;
    volatile uint8_t   rxBuffer[CDC_TXRX_EPSIZE];
    volatile uint8_t   txBuffer[CDC_TXRX_EPSIZE];

    uint8_t initCallback(USBD_HandleTypeDef* pdev, uint8_t cfgidx)
    {
        UNUSED(cfgidx);

        cdcData.TxState = 0;
        cdcData.RxState = 0;

        /* Open EP IN */
        (void)USBD_LL_OpenEP(pdev, CDC_IN_EPADDR, USB_EP_TYPE_BULK, CDC_TXRX_EPSIZE);

        pdev->ep_in[CDC_IN_EPADDR & 0xFU].is_used = 1U;

        /* Open EP OUT */
        (void)USBD_LL_OpenEP(pdev, CDC_OUT_EPADDR, USB_EP_TYPE_BULK, CDC_TXRX_EPSIZE);

        pdev->ep_out[CDC_OUT_EPADDR & 0xFU].is_used           = 1U;
        pdev->ep_in[CDC_NOTIFICATION_EPADDR & 0xFU].bInterval = CDC_POLLING_TIME;

        /* Open Command IN EP */
        (void)USBD_LL_OpenEP(pdev, CDC_NOTIFICATION_EPADDR, USBD_EP_TYPE_INTR, CDC_NOTIFICATION_EPSIZE);
        pdev->ep_in[CDC_NOTIFICATION_EPADDR & 0xFU].is_used = 1U;

        /* Prepare Out endpoint to receive next packet */
        (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EPADDR, (uint8_t*)rxBuffer, CDC_TXRX_EPSIZE);

        return (uint8_t)USBD_OK;
    }

    uint8_t deInitCallback(USBD_HandleTypeDef* pdev, uint8_t cfgidx)
    {
        UNUSED(cfgidx);
        uint8_t ret = 0U;

        /* Close EP IN */
        (void)USBD_LL_CloseEP(pdev, CDC_IN_EPADDR);
        pdev->ep_in[CDC_IN_EPADDR & 0xFU].is_used = 0U;

        /* Close EP OUT */
        (void)USBD_LL_CloseEP(pdev, CDC_OUT_EPADDR);
        pdev->ep_out[CDC_OUT_EPADDR & 0xFU].is_used = 0U;

        /* Close Command IN EP */
        (void)USBD_LL_CloseEP(pdev, CDC_NOTIFICATION_EPADDR);
        pdev->ep_in[CDC_NOTIFICATION_EPADDR & 0xFU].is_used   = 0U;
        pdev->ep_in[CDC_NOTIFICATION_EPADDR & 0xFU].bInterval = 0U;

        return ret;
    }

    uint8_t USBD_CDC_Setup(USBD_HandleTypeDef* pdev, USBD_SetupReqTypedef* req)
    {
        uint8_t            ifalt       = 0U;
        uint16_t           status_info = 0U;
        USBD_StatusTypeDef ret         = USBD_OK;

        switch (req->bmRequest & USB_REQ_TYPE_MASK)
        {
        case USB_REQ_TYPE_CLASS:
            if (req->wLength != 0U)
            {
                if ((req->bmRequest & 0x80U) != 0U)
                {
                    ((cdcCallback_t*)pdev->pUserData)->Control(req->bRequest, (uint8_t*)cdcData.data, req->wLength);

                    (void)USBD_CtlSendData(pdev, (uint8_t*)cdcData.data, req->wLength);
                }
                else
                {
                    cdcData.CmdOpCode = req->bRequest;
                    cdcData.CmdLength = (uint8_t)req->wLength;

                    (void)USBD_CtlPrepareRx(pdev, (uint8_t*)cdcData.data, req->wLength);
                }
            }
            else
            {
                ((cdcCallback_t*)pdev->pUserData)->Control(req->bRequest, (uint8_t*)req, 0U);
            }
            break;

        case USB_REQ_TYPE_STANDARD:
            switch (req->bRequest)
            {
            case USB_REQ_GET_STATUS:
                if (pdev->dev_state == USBD_STATE_CONFIGURED)
                {
                    (void)USBD_CtlSendData(pdev, (uint8_t*)&status_info, 2U);
                }
                else
                {
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                }
                break;

            case USB_REQ_GET_INTERFACE:
                if (pdev->dev_state == USBD_STATE_CONFIGURED)
                {
                    (void)USBD_CtlSendData(pdev, &ifalt, 1U);
                }
                else
                {
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                }
                break;

            case USB_REQ_SET_INTERFACE:
                if (pdev->dev_state != USBD_STATE_CONFIGURED)
                {
                    USBD_CtlError(pdev, req);
                    ret = USBD_FAIL;
                }
                break;

            case USB_REQ_CLEAR_FEATURE:
                break;

            default:
                USBD_CtlError(pdev, req);
                ret = USBD_FAIL;
                break;
            }
            break;

        default:
            USBD_CtlError(pdev, req);
            ret = USBD_FAIL;
            break;
        }

        return (uint8_t)ret;
    }

    uint8_t USBD_CDC_EP0_RxReady(USBD_HandleTypeDef* pdev)
    {
        if ((pdev->pUserData != NULL) && (cdcData.CmdOpCode != 0xFFU))
        {
            ((cdcCallback_t*)pdev->pUserData)->Control(cdcData.CmdOpCode, (uint8_t*)cdcData.data, (uint16_t)cdcData.CmdLength);
            cdcData.CmdOpCode = 0xFFU;
        }

        return (uint8_t)USBD_OK;
    }

    uint8_t USBD_CDC_DataIn(USBD_HandleTypeDef* pdev, uint8_t epnum)
    {
        PCD_HandleTypeDef* hpcd = (PCD_HandleTypeDef*)pdev->pData;

        if ((pdev->ep_in[epnum].total_length > 0U) &&
            ((pdev->ep_in[epnum].total_length % hpcd->IN_ep[epnum].maxpacket) == 0U))
        {
            /* Update the packet total length */
            pdev->ep_in[epnum].total_length = 0U;

            /* Send ZLP */
            (void)USBD_LL_Transmit(pdev, epnum, NULL, 0U);
        }
        else
        {
            cdcData.TxState = 0U;
        }

        return (uint8_t)USBD_OK;
    }

    uint8_t USBD_CDC_DataOut(USBD_HandleTypeDef* pdev, uint8_t epnum)
    {
        uint32_t length = USBD_LL_GetRxDataSize(pdev, epnum);

        Board::USB::onCDCRx((uint8_t*)rxBuffer, length);

        (void)USBD_LL_PrepareReceive(pdev, CDC_OUT_EPADDR, (uint8_t*)rxBuffer, CDC_TXRX_EPSIZE);

        return (uint8_t)USBD_OK;
    }

    uint8_t* getDeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_Device_t* desc = USBgetDeviceDescriptor(length);
        return (uint8_t*)desc;
    }

    uint8_t* getLangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_String_t* desc = USBgetLanguageString(length);
        return (uint8_t*)desc;
    }

    uint8_t* getManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_String_t* desc = USBgetManufacturerString(length);
        return (uint8_t*)desc;
    }

    uint8_t* getProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_String_t* desc = USBgetProductString(length);
        return (uint8_t*)desc;
    }

    uint8_t* getSerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);

        Board::uniqueID_t uid;
        Board::uniqueID(uid);

        const USB_Descriptor_UID_String_t* desc = USBgetSerialIDString(length, uid.uid);
        return (uint8_t*)desc;
    }

    uint8_t* getConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_String_t* desc = USBgetManufacturerString(length);
        return (uint8_t*)desc;
    }

    uint8_t* getInterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t* length)
    {
        UNUSED(speed);
        const USB_Descriptor_String_t* desc = USBgetManufacturerString(length);
        return (uint8_t*)desc;
    }

    uint8_t* getConfigDescriptor(uint16_t* length)
    {
        const USB_Descriptor_Configuration_t* cfg = USBgetCfgDescriptor(length);
        return (uint8_t*)cfg;
    }

    int8_t CDC_Control_FS(uint8_t cmd, uint8_t* pbuf, uint16_t length)
    {
        switch (cmd)
        {
        case CDC_REQ_SetLineEncoding:
        {
            uint32_t baudRate = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
            Board::USB::onCDCBaudRateSet(baudRate);
        }
        break;

        case CDC_REQ_GetLineEncoding:
        {
            uint32_t baudRate = 0;
            Board::USB::onCDCBaudRateGet(baudRate);

            pbuf[0] = (uint8_t)(baudRate);
            pbuf[1] = (uint8_t)(baudRate >> 8);
            pbuf[2] = (uint8_t)(baudRate >> 16);
            pbuf[3] = (uint8_t)(baudRate >> 24);

            //hardcoded
            pbuf[4] = 0;       //1 stop bit
            pbuf[5] = 0;       //no parity
            pbuf[6] = 0x08;    //8bit word
        }
        break;

        default:
            break;
        }

        return USBD_OK;
    }

    uint8_t USBD_CDC_RegisterInterface(USBD_HandleTypeDef* pdev, cdcCallback_t* fops)
    {
        if (fops == NULL)
        {
            return (uint8_t)USBD_FAIL;
        }

        pdev->pUserData = fops;

        return (uint8_t)USBD_OK;
    }

    void sendBuffer()
    {
        cdcData.TxState = 1;

        hUsbDeviceFS.ep_in[CDC_IN_EPADDR & 0xFU].total_length = cdcData.TxLength;
        (void)USBD_LL_Transmit(&hUsbDeviceFS, CDC_IN_EPADDR, (uint8_t*)txBuffer, cdcData.TxLength);
    }

    USBD_DescriptorsTypeDef DeviceDescriptor = {
        getDeviceDescriptor,
        getLangIDStrDescriptor,
        getManufacturerStrDescriptor,
        getProductStrDescriptor,
        getSerialStrDescriptor,
        getConfigStrDescriptor,
        getInterfaceStrDescriptor
    };

    USBD_ClassTypeDef USB_CDC = {
        initCallback,
        deInitCallback,
        USBD_CDC_Setup,
        NULL,
        USBD_CDC_EP0_RxReady,
        USBD_CDC_DataIn,
        USBD_CDC_DataOut,
        NULL,
        NULL,
        NULL,
        NULL,
        getConfigDescriptor,
        NULL,
        NULL,
    };

    cdcCallback_t USBD_Interface_fops_FS = {
        CDC_Control_FS,
        NULL
    };
}    // namespace

namespace Board
{
    namespace USB
    {
        bool fillCDCTxBuffer(uint8_t value)
        {
            //block filling if a transfer is in progress
            if (!cdcData.TxState)
                return false;

            txBuffer[cdcData.TxLength++] = value;

            if (cdcData.TxLength >= CDC_TXRX_EPSIZE)
            {
                flushCDCTxBuffer();
            }

            return true;
        }

        void flushCDCTxBuffer()
        {
            if (cdcData.TxLength)
            {
                sendBuffer();
                cdcData.TxLength = 0;
            }
        }
    }    // namespace USB

    namespace detail
    {
        namespace setup
        {
            void usb()
            {
                if (USBD_Init(&hUsbDeviceFS, &DeviceDescriptor, DEVICE_FS) != USBD_OK)
                    Board::detail::errorHandler();

                if (USBD_RegisterClass(&hUsbDeviceFS, &USB_CDC) != USBD_OK)
                    Board::detail::errorHandler();

                if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
                    Board::detail::errorHandler();

                if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
                    Board::detail::errorHandler();
            }
        }    // namespace setup
    }        // namespace detail
}    // namespace Board