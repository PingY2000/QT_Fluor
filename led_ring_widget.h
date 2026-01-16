// led_ring_widget.h
#ifndef LED_RING_WIDGET_H
#define LED_RING_WIDGET_H

#include <QWidget>
#include <QVector>
#include <QPushButton>

#include "serialmanager.h"

class LedRingWidget : public QWidget
{
    Q_OBJECT
public:
    explicit LedRingWidget(QWidget *parent = nullptr, SerialManager *manager = nullptr);
    void refreshLed();

    QVector<QPair<bool, QString>> getLedStates() const;


    void setLedColor(int index, const QColor &color);
    int ledCount() const;

    struct LedState {
        bool on;
        QString color;
    };
    QVector<LedState> ledStates;

    void clickLed(int index);

public slots:
    void handleLedButtonClicked();

private:
    void createLedRings();

    QVector<QPushButton*> ledButtons;

    SerialManager *serialManager;


};

#endif // LED_RING_WIDGET_H
