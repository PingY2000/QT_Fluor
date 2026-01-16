// fluorescence.cpp
#include "fluorescence.h"

FluorescenceControl::FluorescenceControl(SerialManager* serialManager, QObject *parent)
    : QObject(parent), serial(serialManager)
{
}

void FluorescenceControl::setEnabled(bool enable)
{
    if (motorEnabled != enable) {
        motorEnabled = enable;
        sendMotorCommand();
    }
}

void FluorescenceControl::setDirection(bool direction)
{
    if (motorDirection != direction) {
        motorDirection = direction;
        sendMotorCommand();
    }
}

void FluorescenceControl::setFrequency(uint16_t frequency)
{
    if (motorFrequency != frequency) {
        motorFrequency = frequency;
        sendMotorCommand();
    }
}

void FluorescenceControl::update()
{
    sendMotorCommand();
}

void FluorescenceControl::sendMotorCommand()
{
    QByteArray data;
    data.append(0xA5);  // HEADER
    data.append(0x5A);
    data.append(0x03);
    data.append(0x04);
    data.append(static_cast<char>(motorEnabled ? 1 : 0));
    data.append(static_cast<char>(motorDirection ? 1 : 0));
    data.append(static_cast<char>((motorFrequency >> 8) & 0xFF));  // 高位
    data.append(static_cast<char>(motorFrequency & 0xFF));         // 低位

    if (serial && serial->isOpen()) {
        serial->sendDatawithCRC(data);
    }
}

void FluorescenceControl::sendFluorescenceCommand(bool laser365On, bool laser488On, bool laser532On)
{
    QByteArray data;
    data.append(0xA5);  // Header byte 1
    data.append(0x5A);  // Header byte 2
    data.append(0x04);  // 命令类型，比如03表示光源控制
    data.append(0x03);  // 数据长度，3个通道

    // 激光器状态（1表示开启，0表示关闭）
    data.append(static_cast<char>(laser365On ? 1 : 0));
    data.append(static_cast<char>(laser488On ? 1 : 0));
    data.append(static_cast<char>(laser532On ? 1 : 0));

    if (serial && serial->isOpen()) {
        serial->sendDatawithCRC(data);
    }
}
