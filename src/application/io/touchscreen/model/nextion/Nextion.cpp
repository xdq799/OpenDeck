#include "Nextion.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

bool Nextion::init()
{
    rxBuffer.reset();
    return hwa.init();
}

bool Nextion::setScreen(uint8_t screenID)
{
    return writeCommand("page %u", screenID);
}

bool Nextion::update(uint8_t& buttonID, bool& state)
{
    uint8_t data;

    if (hwa.read(data))
        rxBuffer.insert(data);

    return false;
}

void Nextion::setButtonState(uint8_t index, bool state)
{
    icon_t icon;

    if (!getIcon(index, icon))
        return;
}

__attribute__((weak)) bool Nextion::getIcon(size_t index, icon_t& icon)
{
    return false;
}

bool Nextion::writeCommand(const char* line, ...)
{
    va_list args;
    va_start(args, line);

    size_t retVal = vsnprintf(commandBuffer, bufferSize, line, args);

    va_end(args);

    if (retVal < 0)
        return false;

    for (size_t i = 0; i < strlen(commandBuffer); i++)
    {
        if (!hwa.write(commandBuffer[i]))
            return false;
    }

    return endCommand();
}

bool Nextion::endCommand()
{
    if (!hwa.write(0xFF))
        return false;

    return true;
}