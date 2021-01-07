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

#pragma once

#include "database/Database.h"
#include "midi/src/MIDI.h"

class SysEx
{
    public:
    static constexpr size_t MAX_CUSTOM_SYSEX_MESSAGES   = 100;
    static constexpr size_t MAX_CUSTOM_SYSEX_MSG_LENGTH = 50;

    enum class type_t : uint8_t
    {
        custom,
        moogMatriarch,
        AMOUNT
    };

    SysEx(Database& database, MIDI& midi)
        : database(database)
        , midi(midi)
    {}

    void init();
    bool send(size_t messageIndex, uint16_t value);

    private:
    /// Special byte used to mark the end of custom SysEx message in database.
    /// All custom SysEx messages are stored in a single stream of bytes.
    /// If byte after end marker isn't 0xFF, new custom SysEx message is started.
    /// Otherwise, there are no more messages.
    static constexpr uint8_t CUSTOM_SYSEX_END_MARKER = 0x80;

    using sysExHandler_t = void (SysEx::*)(size_t messageIndex, uint16_t value);

    Database& database;
    MIDI&     midi;

    /// Holds count of total found custom SysEx messages in database.
    size_t totalCustomSysExMessages = 0;

    /// Array holding start index in database for each found custom SysEx message.
    /// This is used for faster access since it avoids lookups in database every time.
    size_t customSysExMessageIndex[MAX_CUSTOM_SYSEX_MESSAGES];

    size_t nextFreeCustomSysExIndex = 0;

    void handleMoogMatriarch(size_t messageIndex, uint16_t value);
    void handleCustom(size_t messageIndex, uint16_t value);

    // sysExHandler_t sysExHandler[static_cast<uint8_t>(type_t::AMOUNT)] = {
    //     &SysEx::handleCustom,
    //     &SysEx::handleMoogMatriarch,
    // };
};