//CenterFillSlider.h
#pragma once

#include <QSlider>
#include <QPainter>
#include <QStyleOptionSlider>

class CenterFillSlider : public QSlider
{
    Q_OBJECT

public:
    explicit CenterFillSlider(Qt::Orientation orientation, QWidget *parent = nullptr)
        : QSlider(orientation, parent) {}

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QSlider::paintEvent(event);  // 先绘制默认外框

        QPainter painter(this);
        QStyleOptionSlider opt;
        initStyleOption(&opt);

        QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);

        // 中点
        int mid = groove.left() + groove.width() / 2;
        int handleCenter = handle.center().x();

        QColor fillColor = QColor(100, 180, 255, 150);  // 半透明蓝色填充

        QRect fillRect;
        if (handleCenter >= mid) {
            // 向右滑动：中间到滑块之间填充
            fillRect = QRect(mid, groove.center().y() - 2, handleCenter - mid, 4);
        } else {
            // 向左滑动：滑块到中间之间填充
            fillRect = QRect(handleCenter, groove.center().y() - 2, mid - handleCenter, 4);
        }

        painter.setPen(Qt::NoPen);
        painter.setBrush(fillColor);
        painter.drawRect(fillRect);
    }
};
