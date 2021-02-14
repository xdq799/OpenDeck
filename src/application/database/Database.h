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

#include "dbms/src/LESSDB.h"

class Database : public LESSDB
{
    public:
    static constexpr uint32_t MAX_CUSTOM_SYSEX_BYTES = 1024;

    class Handlers
    {
        public:
        Handlers()                                = default;
        virtual void presetChange(uint8_t preset) = 0;
        virtual void factoryResetStart()          = 0;
        virtual void factoryResetDone()           = 0;
        virtual void initialized()                = 0;
    };

    Database(Handlers& handlers, LESSDB::StorageAccess& storageAccess, bool initializeData)
        : LESSDB(storageAccess)
        , handlers(handlers)
        , initializeData(initializeData)
    {}

    enum class block_t : uint8_t
    {
        global,
        buttons,
        encoders,
        analog,
        leds,
        display,
        touchscreen,
        customSysEx,
        AMOUNT
    };

    class Section
    {
        public:
        Section() {}

        enum class global_t : uint8_t
        {
            midiFeatures,
            midiMerge,
            AMOUNT
        };

        enum class button_t : uint8_t
        {
            type,
            midiMessage,
            midiID,
            velocity,
            midiChannel,
            AMOUNT
        };

        enum class encoder_t : uint8_t
        {
            enable,
            invert,
            mode,
            midiID,
            midiChannel,
            pulsesPerStep,
            acceleration,
            remoteSync,
            AMOUNT
        };

        enum class analog_t : uint8_t
        {
            enable,
            invert,
            type,
            midiID,
            lowerLimit,
            upperLimit,
            midiChannel,
            AMOUNT
        };

        enum class leds_t : uint8_t
        {
            global,
            activationID,
            rgbEnable,
            controlType,
            activationValue,
            midiChannel,
            AMOUNT
        };

        enum class display_t : uint8_t
        {
            features,
            setting,
            AMOUNT
        };

        enum class touchscreen_t : uint8_t
        {
            setting,
            xPos,
            yPos,
            width,
            height,
            onScreen,
            offScreen,
            pageSwitchEnabled,
            pageSwitchIndex,
            analogPage,
            analogStartXCoordinate,
            analogEndXCoordinate,
            analogStartYCoordinate,
            analogEndYCoordinate,
            analogType,
            analogResetOnRelease,
            AMOUNT
        };

        enum class customSysEx_t : uint8_t
        {
            byte,
            AMOUNT
        };
    };

    using LESSDB::read;
    using LESSDB::update;

    template<typename T>
    int32_t read(T section, size_t index)
    {
        block_t blockIndex = block(section);
        return LESSDB::read(static_cast<uint8_t>(blockIndex), static_cast<uint8_t>(section), index);
    }

    template<typename T>
    bool read(T section, size_t index, int32_t& value)
    {
        block_t blockIndex = block(section);
        return LESSDB::read(static_cast<uint8_t>(blockIndex), static_cast<uint8_t>(section), index, value);
    }

    template<typename T>
    bool update(T section, size_t index, int32_t value)
    {
        block_t blockIndex = block(section);
        return LESSDB::update(static_cast<uint8_t>(blockIndex), static_cast<uint8_t>(section), index, value);
    }

    bool    init();
    bool    factoryReset();
    uint8_t getSupportedPresets();
    bool    setPreset(uint8_t preset);
    uint8_t getPreset();
    bool    setPresetPreserveState(bool state);
    bool    getPresetPreserveState();
    bool    isInitialized();

    void customInitGlobal();
    void customInitButtons();
    void customInitEncoders();
    void customInitAnalog();
    void customInitLEDs();
    void customInitDisplay();
    void customInitTouchscreen();

    private:
    block_t block(Section::global_t section)
    {
        return block_t::global;
    }

    block_t block(Section::button_t section)
    {
        return block_t::buttons;
    }

    block_t block(Section::encoder_t section)
    {
        return block_t::encoders;
    }

    block_t block(Section::analog_t section)
    {
        return block_t::analog;
    }

    block_t block(Section::leds_t section)
    {
        return block_t::leds;
    }

    block_t block(Section::display_t section)
    {
        return block_t::display;
    }

    block_t block(Section::touchscreen_t section)
    {
        return block_t::touchscreen;
    }

    block_t block(Section::customSysEx_t section)
    {
        return block_t::customSysEx;
    }

    bool     isSignatureValid();
    uint16_t getDbUID();
    bool     setDbUID(uint16_t uid);
    bool     setPresetInternal(uint8_t preset);

    Handlers& handlers;

    const bool initializeData;

    /// Address at which user data starts (after system block).
    /// Used to set correct offset in database for user layout.
    uint32_t userDataStartAddress;

    /// Address at which next preset should start.
    /// Used to calculate the start address of next preset.
    uint32_t lastPresetAddress;

    /// Holds total number of supported presets.
    uint16_t supportedPresets;

    /// Holds currently active preset.
    uint8_t activePreset = 0;

    bool initialized = false;
};