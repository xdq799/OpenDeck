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

#include <inttypes.h>
#include "io/touchscreen/Touchscreen.h"
#include "core/src/general/RingBuffer.h"

class Viewtech : public IO::Touchscreen::Model, public IO::Touchscreen::Model::Common
{
    public:
    Viewtech(IO::Touchscreen::Model::HWA& hwa)
        : hwa(hwa)
    {}

    bool                       init() override;
    bool                       deInit() override;
    bool                       setScreen(size_t screenID) override;
    IO::Touchscreen::tsEvent_t update(IO::Touchscreen::tsData_t& data) override;
    void                       setIconState(IO::Touchscreen::icon_t& icon, bool state) override;
    bool                       setBrightness(IO::Touchscreen::brightness_t brightness) override;

    private:
    enum class response_t : uint32_t
    {
        xyUpdate          = 0x08810605,
        buttonStateChange = 0x05820002
    };

    void pollXY();

    IO::Touchscreen::Model::HWA& hwa;

    //as fast as possible on viewtech - minimum variable response is 80ms
    static constexpr uint32_t XY_POLL_TIME_MS = 80;

    //there are 7 levels of brighness - scale them to available range (0-64)
    const uint8_t brightnessMapping[7] = {
        6,
        16,
        32,
        48,
        51,
        58,
        64
    };
};