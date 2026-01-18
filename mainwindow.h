#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <functional>
#include "led_ring_widget.h"
#include "cameramanager.h"
#include "motor_lens.h"
#include "cameramanager.h"
#include "fluorescence.h"

enum class AutoScanState {
    Idle,               // 空闲
    MoveTurntable,      // 转盘运动（输出频率）
    WaitTurntableStable,// 等待转盘机械稳定
    Capture,            // 触发拍照
    WaitCaptureDone,    // 等待图像保存完成
    NextPosition,       // 计算下一个位置
    Finished            // 扫描完成
};

struct CaptureTask {
    QString modeName;   // "Bright", "365", "488", "532"
    int position;       // 对应 UI 的 "转盘位置" spinBox
    int exposure;       // 对应 UI 的 "曝光" spinBox
    int gain;           // 对应 UI 的 "增益" spinBox
    bool enabled;       // 对应 UI 的 "启用" checkBox
};



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void refreshSerialPorts();
    //void connectSerial();

    void on_pushButton_PortSwitch_toggled(bool checked);

    void on_pushButton_HoldOFF_toggled(bool checked);

    void on_verticalSlider_LensSpeed_valueChanged(int value);

    void on_verticalSlider_LensSpeed_sliderReleased();

    void on_pushButton_AutoExpo_toggled(bool checked);

    void on_spinBox_ExposureTime_valueChanged(int arg1);

    void on_pushButton_CameraSwitch_toggled(bool checked);

    void onImageArrived();
    void onImageReady(const QImage& img);
    void onStillImageArrived();

    void updateExposureDisplay();

    void on_spinBox_ExpoGain_valueChanged(int arg1);

    void on_pushButton_Snap_clicked();

    void appendSerialData(const QByteArray &data);

    void on_pushButton_SnapScheduled_toggled(bool checked);


    void on_toolButton_FilePath_clicked();

    void on_lineEdit_FilePath_textChanged(const QString &arg1);

    void on_pushButton_Laser365_toggled(bool checked);

    void on_pushButton_Laser488_toggled(bool checked);

    void on_pushButton_Laser532_toggled(bool checked);

    void on_horizontalSlider_FliterSpeed_sliderReleased();

    void on_horizontalSlider_FliterSpeed_valueChanged(int value);

    void on_pushButton_MotorFliter_ENA_toggled(bool checked);



    void on_pushButton_LightBright_toggled(bool checked);


    void on_spinBox_SnapScheduledTimes_valueChanged(int arg1);

private:

    Ui::MainWindow *ui;
    LedRingWidget *ringWidget;
    QSerialPort *serial;

    QTimer *snapshotTimer;
    QTimer *portScanTimer;

    QStringList previousPortNames;

    CameraManager* camera;
    MotorLens* motorLens;
    FluorescenceControl* fluorescence;

    // === 自动扫描控制 ===
    QTimer* autoScanTimer = nullptr;
    AutoScanState scanState = AutoScanState::Idle;
    int currentTaskIndex = 0;
    QVector<CaptureTask> scanTasks;

    // 转盘物理参数（可根据实际效果调整）
    const int turntablePositions = 4;
    const int turntableFrequency = 1015; // 运动频率
    const int moveDurationMs = 8000;     // 旋转 90 度所需的时间 (ms)
    const int settleDurationMs = 500;   // 停稳后的缓冲时间 (ms)

    void autoScanStep();                // 状态机核心跳转函数
    void startMotion(int frequency);    // 开启电机
    void stopMotion();                  // 关闭电机
    void setModeButtonsEnabled(bool enabled);

    void handleTurntableSwitch(int targetIndex, std::function<void()> onFinished = nullptr);

    int m_currentPositionIndex = 0; // 记录当前转盘在哪个位置 (0, 1, 2, 3)

    int m_RepeatTimes; // 重复次数
    int m_completedCycles; // 记录当前是第几次循环
    int m_currentPeriodSeconds; // 当前周期已过秒数
    QTimer* m_periodTimer;      // 用于更新秒表显示的 1秒定时器

    void startNewScanCycle();

};
#endif // MAINWINDOW_H
