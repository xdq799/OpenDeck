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

#include "board/Board.h"
#include "board/common/usb/descriptors/cdc/Descriptors.h"

#ifndef UART_CHANNEL_TOUCHSCREEN
#define UART_CHANNEL 0
#else
#define UART_CHANNEL UART_CHANNEL_TOUCHSCREEN
#endif

namespace
{
    uint32_t currentBaudRate;
    uint8_t  rxValue;

    void checkIncomingData()
    {
        static bool filled = true;

        //if filled is false, last filling failed
        //possibly due to USB CDC being busy with existing transfer
        //in this case proceed with further reading only when filling succeeds
        //use previously read value
        if (!filled)
        {
            if (Board::USB::fillCDCTxBuffer(rxValue))
                filled = true;
        }

        if (filled)
        {
            if (Board::UART::read(UART_CHANNEL, rxValue))
            {
                if (!Board::USB::fillCDCTxBuffer(rxValue))
                    filled = false;
            }
            else
            {
                Board::USB::flushCDCTxBuffer();
            }
        }
    }
}    // namespace

namespace Board
{
    namespace USB
    {
        void onCDCRx(uint8_t* buffer, size_t size)
        {
            for (size_t i = 0; i < size; i++)
                Board::UART::write(UART_CHANNEL, buffer[i]);
        }

        void onCDCBaudRateSet(uint32_t baudRate)
        {
            currentBaudRate = baudRate;
            Board::UART::init(UART_CHANNEL, currentBaudRate);
        }

        void onCDCBaudRateGet(uint32_t& baudRate)
        {
            baudRate = currentBaudRate;
        }
    }    // namespace USB

    namespace timer
    {
        void ms()
        {
            //use this timer to check for incoming traffic on UART
            static size_t timerCounter = 0;

            if (++timerCounter == CDC_POLLING_TIME)
            {
                timerCounter = 0;
                checkIncomingData();
            }
        }
    }    // namespace timer
}    // namespace Board

int main()
{
    Board::init();

    while (1)
    {
    }
}