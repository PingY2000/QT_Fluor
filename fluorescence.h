#ifndef FLUORESCENCE_H
#define FLUORESCENCE_H

#include <QObject>
#include "serialmanager.h"



class FluorescenceControl : public QObject
{
    Q_OBJECT  // Qt 元对象宏（必须要加，否则 moc 无法处理）

public:
    explicit FluorescenceControl(SerialManager* serialManager, QObject *parent = nullptr);

    void setEnabled(bool enable);
    void setDirection(bool direction);
    void setFrequency(uint16_t frequency);
    void update();  // 主动更新串口发送内容
    void sendFluorescenceCommand(bool laser365On, bool laser488On, bool laser532On);

private:
    bool motorEnabled = false;
    bool motorDirection = false;
    uint16_t motorFrequency = 0;

    SerialManager* serial = nullptr;

    void sendMotorCommand();
};

#endif // FLUORESCENCE_H
