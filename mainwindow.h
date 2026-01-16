#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include "led_ring_widget.h"
#include "cameramanager.h"
#include "motor_lens.h"
#include "cameramanager.h"
#include "fluorescence.h"


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
    void on_pushButton_All_On_clicked();
    void on_pushButton_All_Off_clicked();

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

    void on_pushButton_AutoScan_toggled(bool checked);

    void on_comboBox_LedColorMode_currentIndexChanged(int index);

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
};
#endif // MAINWINDOW_H
