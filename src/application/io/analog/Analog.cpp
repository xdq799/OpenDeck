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

#include "Analog.h"
#include "core/src/general/Helpers.h"

using namespace IO;

void Analog::update(bool forceResend)
{
    //check values
    for (int i = 0; i < MAX_NUMBER_OF_ANALOG; i++)
    {
        if (!forceResend)
        {
            uint16_t value;

            if (!_hwa.value(i, value))
                continue;

            processReading(i, value);
        }
        else
        {
            if (_database.read(Database::Section::analog_t::enable, i))
            {
                analogDescriptor_t analogDescriptor;
                fillAnalogDescriptor(i, analogDescriptor);

                sendMessage(i, analogDescriptor, _lastValue[i]);
            }
        }
    }
}

Analog::adcType_t Analog::adcType()
{
    return _filter.adcType();
}

void Analog::processReading(uint8_t analogID, uint16_t value)
{
    //don't process component if it's not enabled
    if (!_database.read(Database::Section::analog_t::enable, analogID))
        return;

    analogDescriptor_t analogDescriptor;
    fillAnalogDescriptor(analogID, analogDescriptor);

    if (!_filter.isFiltered(analogID, analogDescriptor.type, value, value))
        return;

    bool send = false;

    if (analogDescriptor.type != type_t::button)
    {
        switch (analogDescriptor.type)
        {
        case type_t::potentiometerControlChange:
        case type_t::potentiometerNote:
        case type_t::nrpn7b:
        case type_t::nrpn14b:
        case type_t::pitchBend:
        case type_t::cc14bit:
        {
            if (checkPotentiometerValue(analogID, analogDescriptor, value))
                send = true;
        }
        break;

        case type_t::fsr:
        {
            if (checkFSRvalue(analogID, analogDescriptor, value))
                send = true;
        }
        break;

        default:
            break;
        }
    }
    else
    {
        if (_buttonHandler != nullptr)
            _buttonHandler(analogID, value);
    }

    if (send)
    {
        sendMessage(analogID, analogDescriptor, value);
        _lastValue[analogID] = value;
    }

    _cInfo.send(Database::block_t::analog, analogID);
}

bool Analog::checkPotentiometerValue(uint8_t analogID, analogDescriptor_t& descriptor, uint16_t& value)
{
    uint16_t maxLimit;

    MIDI::encDec_14bit_t encDec_14bit;

    if ((descriptor.type == type_t::nrpn14b) || (descriptor.type == type_t::pitchBend) || (descriptor.type == type_t::cc14bit))
    {
        //14-bit values are already read
        maxLimit = MIDI_14_BIT_VALUE_MAX;
    }
    else
    {
        maxLimit = MIDI_7_BIT_VALUE_MAX;

        //use 7-bit MIDI ID and limits
        encDec_14bit.value = descriptor.midiID;
        encDec_14bit.split14bit();
        descriptor.midiID = encDec_14bit.low;

        encDec_14bit.value = descriptor.lowerLimit;
        encDec_14bit.split14bit();
        descriptor.lowerLimit = encDec_14bit.low;

        encDec_14bit.value = descriptor.upperLimit;
        encDec_14bit.split14bit();
        descriptor.upperLimit = encDec_14bit.low;
    }

    if (value > maxLimit)
        return false;

    uint32_t scaledMIDIvalue;

    if (descriptor.lowerLimit > descriptor.upperLimit)
    {
        scaledMIDIvalue = core::misc::mapRange(static_cast<uint32_t>(value), static_cast<uint32_t>(0), static_cast<uint32_t>(maxLimit), static_cast<uint32_t>(descriptor.upperLimit), static_cast<uint32_t>(descriptor.lowerLimit));

        if (!descriptor.inverted)
            scaledMIDIvalue = descriptor.upperLimit - (scaledMIDIvalue - descriptor.lowerLimit);
    }
    else
    {
        scaledMIDIvalue = core::misc::mapRange(static_cast<uint32_t>(value), static_cast<uint32_t>(0), static_cast<uint32_t>(maxLimit), static_cast<uint32_t>(descriptor.lowerLimit), static_cast<uint32_t>(descriptor.upperLimit));

        if (descriptor.inverted)
            scaledMIDIvalue = descriptor.upperLimit - (scaledMIDIvalue - descriptor.lowerLimit);
    }

    if (scaledMIDIvalue == _lastValue[analogID])
        return false;

    value = scaledMIDIvalue;

    return true;
}

bool Analog::checkFSRvalue(uint8_t analogID, analogDescriptor_t& descriptor, uint16_t& value)
{
    //don't allow touchscreen components to be processed as FSR
    if (analogID >= MAX_NUMBER_OF_ANALOG)
        return false;

    if (value > 0)
    {
        if (!_fsrPressed[analogID])
        {
            //sensor is really pressed
            _fsrPressed[analogID] = true;
            return true;
        }
    }
    else
    {
        if (_fsrPressed[analogID])
        {
            _fsrPressed[analogID] = false;
            return true;
        }
    }

    return false;
}

void Analog::sendMessage(uint8_t analogID, analogDescriptor_t& descriptor, uint16_t value)
{
    switch (descriptor.type)
    {
    case type_t::potentiometerControlChange:
    case type_t::potentiometerNote:
    {
        if (descriptor.type == type_t::potentiometerControlChange)
        {
            _midi.sendControlChange(descriptor.midiID, value, descriptor.channel);
            _display.displayMIDIevent(Display::eventType_t::out, Display::event_t::controlChange, descriptor.midiID, value, descriptor.channel + 1);
        }
        else
        {
            _midi.sendNoteOn(descriptor.midiID, value, descriptor.channel);
            _display.displayMIDIevent(Display::eventType_t::out, Display::event_t::noteOn, descriptor.midiID, value, descriptor.channel + 1);
        }
    }
    break;

    case type_t::nrpn7b:
    case type_t::nrpn14b:
    case type_t::cc14bit:
    {
        MIDI::encDec_14bit_t encDec_14bit;

        //when nrpn/cc14bit is used, MIDI ID is split into two messages
        //first message contains higher byte

        encDec_14bit.value = descriptor.midiID;
        encDec_14bit.split14bit();

        if (descriptor.type != type_t::cc14bit)
        {
            _midi.sendControlChange(99, encDec_14bit.high, descriptor.channel);
            _midi.sendControlChange(98, encDec_14bit.low, descriptor.channel);
        }

        if (descriptor.type == type_t::nrpn7b)
        {
            _midi.sendControlChange(6, value, descriptor.channel);
        }
        else
        {
            descriptor.midiID = encDec_14bit.low;

            //send 14-bit value in another two messages
            //first message contains higher byte
            encDec_14bit.value = value;
            encDec_14bit.split14bit();

            if (descriptor.type == type_t::cc14bit)
            {
                if (descriptor.midiID >= 96)
                    break;    //not allowed

                _midi.sendControlChange(descriptor.midiID, encDec_14bit.high, descriptor.channel);
                _midi.sendControlChange(descriptor.midiID + 32, encDec_14bit.low, descriptor.channel);
            }
            else
            {
                _midi.sendControlChange(6, encDec_14bit.high, descriptor.channel);
                _midi.sendControlChange(38, encDec_14bit.low, descriptor.channel);
            }
        }

        _display.displayMIDIevent(Display::eventType_t::out, (descriptor.type == type_t::cc14bit) ? Display::event_t::controlChange : Display::event_t::nrpn, descriptor.midiID, value, descriptor.channel + 1);
    }
    break;

    case type_t::pitchBend:
    {
        _midi.sendPitchBend(value, descriptor.channel);
        _display.displayMIDIevent(Display::eventType_t::out, Display::event_t::pitchBend, descriptor.midiID, value, descriptor.channel + 1);
    }
    break;

    case type_t::fsr:
    {
        if (_fsrPressed[analogID])
        {
            _midi.sendNoteOn(descriptor.midiID, value, descriptor.channel);
            _display.displayMIDIevent(Display::eventType_t::out, Display::event_t::noteOn, descriptor.midiID, value, descriptor.channel + 1);
            _leds.midiToState(MIDI::messageType_t::noteOn, descriptor.midiID, value, descriptor.channel, LEDs::dataSource_t::internal);
        }
        else
        {
            _midi.sendNoteOff(descriptor.midiID, 0, descriptor.channel);
            _display.displayMIDIevent(Display::eventType_t::out, Display::event_t::noteOff, descriptor.midiID, value, descriptor.channel + 1);
            _leds.midiToState(MIDI::messageType_t::noteOff, descriptor.midiID, 0, descriptor.channel, LEDs::dataSource_t::internal);
        }
    }
    break;

    default:
        break;
    }
}

void Analog::debounceReset(uint16_t index)
{
    _fsrPressed[index] = false;
    _lastValue[index]  = 0xFFFF;
    _filter.reset(index);
}

void Analog::registerButtonHandler(buttonHandler_t handler)
{
    _buttonHandler = std::move(handler);
}

void Analog::fillAnalogDescriptor(uint8_t analogID, analogDescriptor_t& analogDescriptor)
{
    analogDescriptor.type       = static_cast<type_t>(_database.read(Database::Section::analog_t::type, analogID));
    analogDescriptor.lowerLimit = _database.read(Database::Section::analog_t::lowerLimit, analogID);
    analogDescriptor.upperLimit = _database.read(Database::Section::analog_t::upperLimit, analogID);
    analogDescriptor.midiID     = _database.read(Database::Section::analog_t::midiID, analogID);
    analogDescriptor.channel    = _database.read(Database::Section::analog_t::midiChannel, analogID);
    analogDescriptor.inverted   = _database.read(Database::Section::analog_t::invert, analogID);
}