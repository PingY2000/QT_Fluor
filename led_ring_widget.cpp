// led_ring_widget.cpp
#include "led_ring_widget.h"
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsProxyWidget>
#include <QVBoxLayout>
#include <QtMath>
#include <QColor>
#include <QString>
#include <QDebug>

LedRingWidget::LedRingWidget(QWidget *parent, SerialManager *manager)
    : QWidget(parent), serialManager(manager)
{
    auto *layout = new QVBoxLayout(this);
    auto *view = new QGraphicsView(this);
    view->setRenderHint(QPainter::Antialiasing);

    auto *scene = new QGraphicsScene(this);
    view->setScene(scene);
    layout->addWidget(view);

    QVector<int> counts = {1, 6, 12, 24};
    QVector<int> radii = {0, 56, 120, 180};


    int centerX = 250;
    int centerY = 250;


    QPen pen(Qt::cyan);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(10); // 线宽

    for (int radius : radii) {
        scene->addEllipse(centerX - radius, centerY - radius,
                          radius * 2, radius * 2,
                          pen);
    }

    //int currentIndex = 0;
    for (int r = 0; r < counts.size(); ++r) {
        for (int i = 0; i < counts[r]; ++i) {
            double angle = (360.0 / counts[r]) * i;
            if (r>1) angle=180-angle;
            double rad = qDegreesToRadians(angle);
            double x = centerX + radii[r] * cos(rad);
            double y = centerY + radii[r] * sin(rad);

            QPushButton *button = new QPushButton(QString::number(ledButtons.size() + 1));
            button->setProperty("ledIndex", ledButtons.size());
            button->setFixedSize(30, 30);
            button->setAttribute(Qt::WA_TranslucentBackground);
            button->setStyleSheet("background-color: #000000; border-radius: 15px; border: 3px solid gray;color:  gray;");
            connect(button, &QPushButton::clicked, this, &LedRingWidget::handleLedButtonClicked);


            QGraphicsProxyWidget *proxy = scene->addWidget(button);
            proxy->setPos(x - 15, y - 15);

            ledButtons.append(button);
        }
    }


    int totalLeds = 1 + 6 + 12 + 24;
    ledStates.resize(totalLeds);

    // 初始化状态
    for (int i = 0; i < totalLeds; ++i) {
        ledStates[i].on = false;
        ledStates[i].color = "#ffffff"; // 默认白色
    }

}


void LedRingWidget::refreshLed()
{

    for (int i = 0; i < ledButtons.size(); ++i) {
        if (ledStates[i].on)
            ledButtons[i]->setStyleSheet(QString("background-color:%1; border-radius: 15px; border: 2px solid gray;color: gray;").arg(ledStates[i].color));
        else
            ledButtons[i]->setStyleSheet("background-color:#000000; border-radius: 15px; border: 3px solid gray;color: gray;");
    }
    QByteArray data;
    data.append(0xA5);  // HEADER
    data.append(0x5A);
    data.append(0x01);
    data.append(17);

    uint8_t accumulator = 0;
    int bitPos = 0; // 当前 bit 写入位置（0~7）

    for (int i = 0; i < 43; i++) {

        const auto& s = ledStates[i];
        QColor c(s.color);  // 解析 QString 颜色

        int r = c.red()   > 0 ? 1 : 0;
        int g = c.green() > 0 ? 1 : 0;
        int b = c.blue()  > 0 ? 1 : 0;

        uint8_t v = ledStates[i].on ? (r << 2) | (g << 1) | b : 0;

        for (int bit = 0; bit < 3; bit++) {

            // 写入这一 bit
            accumulator |= ((v >> bit) & 1) << bitPos;
            bitPos++;

            // 填满 8 bit，就 push 到 data
            if (bitPos == 8) {
                data.append(accumulator);
                accumulator = 0;
                bitPos = 0;
            }
        }
    }

    // 最后不足 8bit 的也要 append
    if (bitPos != 0)
        data.append(accumulator);

    if (serialManager && serialManager->isOpen()) {
        serialManager->sendDatawithCRC(data);
    }


}

void LedRingWidget::handleLedButtonClicked() {
    QPushButton* senderButton = qobject_cast<QPushButton*>(sender());
    if (!senderButton) return;

    int index = senderButton->property("ledIndex").toInt();  // 获取索引

    // 切换状态示例：开启->关闭->开启
    ledStates[index].on = !ledStates[index].on;
    refreshLed();

}

QVector<QPair<bool, QString>> LedRingWidget::getLedStates() const
{
    QVector<QPair<bool, QString>> states;
    for (const auto &led : ledStates) {
        states.append(qMakePair(led.on, led.color));
    }
    return states;
}

void LedRingWidget::clickLed(int index)
{
    if (index < 0 || index >= ledStates.size())
        return;

    // 切换 LED 状态
    ledStates[index].on = !ledStates[index].on;

    // 刷新显示和发送串口命令
    refreshLed();

}

