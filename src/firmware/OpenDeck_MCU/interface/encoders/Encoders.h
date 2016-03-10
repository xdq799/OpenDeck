#ifndef ENCODERS_H_
#define ENCODERS_H_

#include "../../hardware/board/Board.h"

class Encoders  {

    public:
    Encoders();
    void init();
    void update();
    uint8_t getParameter(uint8_t messageType, uint8_t parameterID);
    bool setParameter(uint8_t messageType, uint8_t parameter, uint8_t newParameter);
    bool getEncoderEnabled(uint8_t encoderID);

    private:

    //set
    bool setEncoderEnabled(uint8_t encoderID, uint8_t state);
    bool setEncoderInvertState(uint8_t encoderID, uint8_t state);
    bool setEncodingMode(uint8_t encoderID, uint8_t type);
    bool setMIDIid(uint8_t encoderID, uint8_t _midiID);

    //get
    bool getEncoderInvertState(uint8_t encoderID);
    encoderType getEncodingMode(uint8_t encoderID);
    uint8_t getMIDIid(uint8_t encoderID);

};

extern Encoders encoders;

#endif
