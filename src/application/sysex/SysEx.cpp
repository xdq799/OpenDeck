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

#include "SysEx.h"

void SysEx::init()
{
    for (size_t i = 0; i < MAX_CUSTOM_SYSEX_MESSAGES; i++)
        customSysExMessageIndex[i] = 0xFF;

    //retrieve all indexes for custom sysex messages from db
    for (uint32_t i = 0; i < Database::MAX_CUSTOM_SYSEX_BYTES; i++)
    {
        uint8_t byte = database.read(Database::Section::customSysEx_t::byte, i);

        if (byte == 0xFF)
            break;    //no further records

        if (!i)
        {
            //if first byte is anything other than 0xFF, obviously it's a starting index of custom sysex message
            customSysExMessageIndex[totalCustomSysExMessages++] = 0;
        }
        else if (byte == CUSTOM_SYSEX_END_MARKER)
        {
            //custom sysex message ends with CUSTOM_SYSEX_END_MARKER which means
            //the next custom message starts on next index
            customSysExMessageIndex[totalCustomSysExMessages++] = i + 1;

            //no point in checking next index in next loop iteration since it's a start of next message
            i++;
        }
    }
}

bool SysEx::send(size_t messageIndex, uint16_t value)
{
    if (messageIndex >= MAX_CUSTOM_SYSEX_MESSAGES)
        return false;

    if (customSysExMessageIndex[messageIndex] == 0xFF)
        return false;    //this message index doesn't exist in database

    uint8_t buffer[MAX_CUSTOM_SYSEX_MSG_LENGTH] = {};

    size_t length;

    for (length = 0; length < MAX_CUSTOM_SYSEX_MSG_LENGTH; length++)
    {
        uint16_t dbIndex = customSysExMessageIndex[messageIndex] + length;

        if (dbIndex >= Database::MAX_CUSTOM_SYSEX_BYTES)
            break;

        uint8_t byte = database.read(Database::Section::customSysEx_t::byte, dbIndex);

        if ((byte == CUSTOM_SYSEX_END_MARKER) || (byte == 0xFF))
            break;

        buffer[length] = byte;
    }

    if (length)
    {
        //0xF0 and 0xF7 aren't stored in DB to avoid taking space
        midi.sendSysEx(length, buffer, false);
        return true;
    }

    return false;
}