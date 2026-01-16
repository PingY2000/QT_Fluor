//motor_lens.h
#ifndef MOTOR_LENS_H
#define MOTOR_LENS_H

#include <QObject>
#include <QByteArray>
#include "serialmanager.h"  // 或者你自己的串口管理类

class MotorLens : public QObject
{
    Q_OBJECT

public:
    explicit MotorLens(SerialManager* serialManager, QObject *parent = nullptr);

    void setEnabled(bool enable);
    void setDirection(bool direction);
    void setFrequency(uint16_t frequency);

    void update();  // 主动更新串口发送内容

private:
    bool motorEnabled = false;
    bool motorDirection = false;
    uint16_t motorFrequency = 0;

    SerialManager* serial = nullptr;

    void sendMotorCommand();
};

#endif // MOTOR_LENS_H
