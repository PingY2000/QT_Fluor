//motor_lens.cpp
#include "motor_lens.h"

MotorLens::MotorLens(SerialManager* serialManager, QObject *parent)
    : QObject(parent), serial(serialManager)
{
}

void MotorLens::setEnabled(bool enable)
{
    if (motorEnabled != enable) {
        motorEnabled = enable;
        sendMotorCommand();
    }
}

void MotorLens::setDirection(bool direction)
{
    if (motorDirection != direction) {
        motorDirection = direction;
        sendMotorCommand();
    }
}

void MotorLens::setFrequency(uint16_t frequency)
{
    if (motorFrequency != frequency) {
        motorFrequency = frequency;
        sendMotorCommand();
    }
}

void MotorLens::update()
{
    sendMotorCommand();
}

void MotorLens::sendMotorCommand()
{
    QByteArray data;
    data.append(0xA5);  // HEADER
    data.append(0x5A);
    data.append(0x02);
    data.append(0x04);
    data.append(static_cast<char>(motorEnabled ? 1 : 0));
    data.append(static_cast<char>(motorDirection ? 1 : 0));
    data.append(static_cast<char>((motorFrequency >> 8) & 0xFF));  // 高位
    data.append(static_cast<char>(motorFrequency & 0xFF));         // 低位

    if (serial && serial->isOpen()) {
        serial->sendDatawithCRC(data);
    }
}
