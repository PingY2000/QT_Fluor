//miinwindows.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "led_ring_widget.h"
#include "serialmanager.h"
#include "motor_lens.h"
#include "cameramanager.h"
#include "fluorescence.h"
#include <QMessageBox>
#include <QDebug>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QFileDialog>
#include <QSettings>
#include <QPainter>




SerialManager *serialManager;

QTimer* autoScanTimer = nullptr;
int currentLedIndex = 0;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    ui->setupUi(this);

    //串口初始化
    serialManager = new SerialManager(this);
    previousPortNames = QStringList();
    refreshSerialPorts();

    connect(serialManager, &SerialManager::dataReceived, this, &MainWindow::appendSerialData);

    //定时扫描串口
    portScanTimer = new QTimer(this);
    connect(portScanTimer, &QTimer::timeout, this, &MainWindow::refreshSerialPorts);
    portScanTimer->start(2000);

    //相机
    camera = new CameraManager(this);
    ui->pushButton_AutoExpo->setChecked(true);
    ui->spinBox_ExposureTime->setReadOnly(true);
    ui->spinBox_ExpoGain->setReadOnly(true);
    camera->setAutoExposure(true);
    connect(camera, &CameraManager::exposureChanged, this, &MainWindow::updateExposureDisplay);


    //led控件初始化
    ringWidget = new LedRingWidget(this, serialManager);  // 将其传给 LED 控件
    ui->verticalLayout->addWidget(ringWidget);

    //led颜色设置
    // 颜色模式列表
    struct ColorMode {
        QString name;
        QString hex; // 或 QColor
    };

    QVector<ColorMode> colorModes = {
        {"白",   "#ffffff"},
        {"红",   "#ff0000"},
        {"绿",   "#00ff00"},
        {"蓝",   "#0000ff"},
    };

    // 添加到下拉框
    for (const auto& mode : colorModes) {
        QPixmap pix(20,20);
        pix.fill(Qt::transparent);
        QPainter p(&pix);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(mode.hex));
        p.setPen(Qt::NoPen);
        p.drawEllipse(0,0,20,20);
        ui->comboBox_LedColorMode->addItem(QIcon(pix), mode.name, mode.hex);
    }


    //镜头电机组件
    motorLens = new MotorLens(serialManager, this);

    //定时拍照
    snapshotTimer = new QTimer(this);
    connect(snapshotTimer, &QTimer::timeout, this, &MainWindow::on_pushButton_Snap_clicked);

    // 设置初始目录
    QSettings settings("YourCompany", "YourApp");
    QString lastDir = settings.value("lastSaveDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    ui->lineEdit_FilePath->setText(lastDir);
    camera->setSaveDirectory(ui->lineEdit_FilePath->text());

    //荧光
    fluorescence = new FluorescenceControl(serialManager, this);

}

MainWindow::~MainWindow()
{

    delete ui;
    delete snapshotTimer;
    delete motorLens;
    delete ringWidget;
    delete camera;
    delete portScanTimer;
    delete serialManager;
}

void MainWindow::appendSerialData(const QByteArray &data)
{
    // 显示在 QTextEdit 控件中（例如 ui->textEdit_SerialMonitor）
    ui->textEdit_SerialMonitor->moveCursor(QTextCursor::End);
    ui->textEdit_SerialMonitor->insertPlainText(QString::fromUtf8(data));
}

// 图像到达通知：从相机中拉图像
void MainWindow::onImageArrived() {
    if (camera) {
        camera->fetchImage();
    }
}

// 图像准备好：显示到 UI
void MainWindow::onImageReady(const QImage& img) {
    ui->labeldisplay->setPixmap(QPixmap::fromImage(img).scaled(ui->labeldisplay->size(), Qt::KeepAspectRatio));

}

// 拍照完成通知：从相机中拉静态图像
void MainWindow::onStillImageArrived() {
    if (camera) {
        camera->fetchStillImage();
    }
}

void MainWindow::updateExposureDisplay() {
    unsigned exp = 0;
    unsigned short gain = 0;
    if (camera->getExposureTime(exp)) {
        ui->spinBox_ExposureTime->blockSignals(true); // 防止触发 valueChanged
        ui->spinBox_ExposureTime->setValue(exp / 1000);
        ui->spinBox_ExposureTime->blockSignals(false);
    }
    if (camera->getGain(gain)) {
        ui->spinBox_ExpoGain->blockSignals(true); // 防止触发 valueChanged
        ui->spinBox_ExpoGain->setValue(gain);
        ui->spinBox_ExpoGain->blockSignals(false);
    }
}

void MainWindow::on_pushButton_All_On_clicked()
{
    for (int i = 0; i <ringWidget->ledStates.size(); ++i) {
        ringWidget->ledStates[i].on = true;
    }
    ringWidget->refreshLed();
}

void MainWindow::on_pushButton_All_Off_clicked()
{
    for (int i = 0; i <ringWidget->ledStates.size(); ++i) {
        ringWidget->ledStates[i].on = false;
    }
    ringWidget->refreshLed();
}

void MainWindow::refreshSerialPorts()
{
    QStringList currentPortNames;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        currentPortNames << info.portName();
    }

    // 如果串口列表没有变化就不刷新
    if (currentPortNames == previousPortNames)
        return;

    previousPortNames = currentPortNames;

    ui->comboBox_Port->clear();
    foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
        //QString fullName = QString("%1 %2").arg(info.portName(),info.description());
        //ui->comboBox_Port->addItem(fullName, info.portName()); // 使用显示名，但内部存储 portName
        ui->comboBox_Port->addItem(info.portName());
    }


}

void MainWindow::on_pushButton_PortSwitch_toggled(bool checked)
{

    if (checked) {
        QString portName = ui->comboBox_Port->currentText();

        if (ui->comboBox_Port){
            serialManager->openPort(portName, 115200);
            if (serialManager->isOpen()){
                ui->pushButton_PortSwitch->setText("关闭");
            }
            else
            {
                ui->pushButton_PortSwitch->blockSignals(true);
                ui->pushButton_PortSwitch->setChecked(false);
                ui->pushButton_PortSwitch->blockSignals(false);
            }
        }
    } else {
        serialManager->closePort();
        if (!serialManager->isOpen()){
            ui->pushButton_PortSwitch->setText("打开");
        }
        else{
            ui->pushButton_PortSwitch->blockSignals(true);
            ui->pushButton_PortSwitch->setChecked(true);
            ui->pushButton_PortSwitch->blockSignals(false);
        }
    }
}



void MainWindow::on_pushButton_HoldOFF_toggled(bool checked)
{
    motorLens->setEnabled(!checked);
}


void MainWindow::on_verticalSlider_LensSpeed_valueChanged(int value)
{
    int speed=abs(value);
    int frequency=speed>4 ? pow(1.13,((speed-5)/1.53)): 0;//int frequency=speed>4 ? pow(1.1,((speed-5)/1.53)): 0;
    if (frequency>2500) frequency=2500;
    if (frequency) motorLens->setDirection(value>0 ? 1 : 0);
    ui->lcdNumber_LensFreq->display(frequency);
    motorLens->setEnabled(!ui->pushButton_HoldOFF->isChecked());
    //qDebug()<<frequency;
    motorLens->setFrequency(frequency);
}





void MainWindow::on_verticalSlider_LensSpeed_sliderReleased()
{
    ui->verticalSlider_LensSpeed->setValue(0);
}


void MainWindow::on_pushButton_AutoExpo_toggled(bool checked)
{
    ui->spinBox_ExposureTime->setReadOnly(checked);
    ui->spinBox_ExpoGain->setReadOnly(checked);
    camera->setAutoExposure(checked);
    if (!checked) {
        camera->put_ExpoTime(ui->spinBox_ExposureTime->value());
    }

}


void MainWindow::on_spinBox_ExposureTime_valueChanged(int arg1)
{
    camera->put_ExpoTime(arg1);
}


void MainWindow::on_pushButton_CameraSwitch_toggled(bool checked)
{
    if (checked) {
        // 打开相机
        if (camera->open()) {
            // 连接图像信号（如未连接过）
            connect(camera, &CameraManager::imageArrived, this, &MainWindow::onImageArrived);
            connect(camera, &CameraManager::imageReady, this, &MainWindow::onImageReady);
            connect(camera, &CameraManager::stillImageArrived, this, &MainWindow::onStillImageArrived);
            ui->pushButton_CameraSwitch->setText("关闭相机");
        } else {
            QMessageBox::warning(this, "错误", "相机打开失败！");
            ui->pushButton_CameraSwitch->setChecked(false); // 打开失败时重置按钮状态
        }
    } else {
        // 关闭相机
        camera->close();
        ui->pushButton_CameraSwitch->setText("打开相机");
    }

}


void MainWindow::on_spinBox_ExpoGain_valueChanged(int arg1)
{
    camera->put_ExpoGain(arg1);
}

void MainWindow::on_pushButton_Snap_clicked()
{
    QString ledStateStr;
    if (ringWidget) {
        QVector<QPair<bool, QString>> states = ringWidget->getLedStates();
        for (const auto &s : states) {
            ledStateStr += s.first ? "1" : "0";  // LED开=1，关=0
        }
    } else {
        ledStateStr = "UNKNOWN";
    }

    if (camera && camera->isOpen()) {
        camera->setLedStateString(ledStateStr);  // 把 LED 状态传给 CameraManager
        disconnect(camera, &CameraManager::stillImageArrived, nullptr, nullptr); // 先断开所有旧的连接，避免重复
        connect(camera, &CameraManager::stillImageArrived, this, [=]() {
            camera->fetchStillImageTif();
        });
        camera->snapImage();  // 发起静态图像拍摄
    } else {
        // -------- 无相机：模拟模式 --------
        qDebug() << "无相机连接，执行模拟拍照";

        // 1. 生成时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

        // 3. 构造文件名
        QString filename = QString("snapshot_%1_LED%2.tif")
                               .arg(timestamp)
                               .arg(ledStateStr);

        QString saveDir = "D:/snap_test"; // ← 替换为实际保存路径
        QDir().mkpath(saveDir);
        QString filePath = QDir(saveDir).filePath(filename);

        // 4. 输出文件名
        qDebug() << "模拟保存文件名:" << filePath;

        // 5. 生成模拟图像保存
        QImage fake(640, 480, QImage::Format_RGB888);
        fake.fill(Qt::gray);
        fake.save(filePath, "TIFF");

        // 6. 发送信号以便 UI 显示
        if (camera)
            emit camera->imageReady(fake);
    }

}

void MainWindow::on_pushButton_SnapScheduled_toggled(bool checked)
{
    ui->spinBox_SnapScheduledTime->setReadOnly(checked);
    if (checked) {
        ui->pushButton_Snap->click();
        snapshotTimer->start(ui->spinBox_SnapScheduledTime->value()*1000); // 每 5000 毫秒（5 秒）拍一次
    } else {
        snapshotTimer->stop();
    }
}



void MainWindow::on_toolButton_FilePath_clicked()
{
    QSettings settings("YourCompany", "YourApp");
    QString lastDir = settings.value("lastSaveDir", QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)).toString();
    QString dir = QFileDialog::getExistingDirectory(this, "选择保存文件夹", lastDir, QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        ui->lineEdit_FilePath->setText(dir); // 显示到 LineEdit 中
        settings.setValue("lastSaveDir", dir); // 保存新的目录
    }
}

void MainWindow::on_lineEdit_FilePath_textChanged(const QString &arg1)
{
    camera->setSaveDirectory(arg1);
}


void MainWindow::on_pushButton_Laser365_toggled(bool checked)
{
    fluorescence->sendFluorescenceCommand(ui->pushButton_Laser365->isChecked(),ui->pushButton_Laser488->isChecked(),ui->pushButton_Laser532->isChecked());

}


void MainWindow::on_pushButton_Laser488_toggled(bool checked)
{
    fluorescence->sendFluorescenceCommand(ui->pushButton_Laser365->isChecked(),ui->pushButton_Laser488->isChecked(),ui->pushButton_Laser532->isChecked());
}


void MainWindow::on_pushButton_Laser532_toggled(bool checked)
{
    fluorescence->sendFluorescenceCommand(ui->pushButton_Laser365->isChecked(),ui->pushButton_Laser488->isChecked(),ui->pushButton_Laser532->isChecked());
}


void MainWindow::on_horizontalSlider_FliterSpeed_sliderReleased()
{
    ui->horizontalSlider_FliterSpeed->setValue(0);
}


void MainWindow::on_horizontalSlider_FliterSpeed_valueChanged(int value)
{
    int speed=abs(value);
    int frequency=speed>4 ? pow(1.14,((speed-5)/1.537)): 0;//int frequency=speed>4 ? pow(1.1,((speed-5)/1.53)): 0;
    //if (frequency>2500) frequency=2500;
    if (frequency) fluorescence->setDirection(value>0 ? 1 : 0);
    ui->lcdNumber_FliterFreq->display(frequency);
    fluorescence->setEnabled(ui->pushButton_MotorFliter_ENA->isChecked());
    //qDebug()<<frequency;
    fluorescence->setFrequency(frequency);
}


void MainWindow::on_pushButton_MotorFliter_ENA_toggled(bool checked)
{
    fluorescence->setEnabled(checked);
}


void MainWindow::on_pushButton_AutoScan_toggled(bool checked)
{
    if (checked) {
        qDebug() << "自动扫描开始";

        if (!autoScanTimer) {
            autoScanTimer = new QTimer(this);
            connect(autoScanTimer, &QTimer::timeout, this, [=]() {
                int ledCount = ringWidget->ledStates.size(); // 使用 ringWidget

                if (currentLedIndex >= ledCount) {
                    // 扫描完毕
                    autoScanTimer->stop();
                    ui->pushButton_AutoScan->setChecked(false);
                    qDebug() << "自动扫描结束";
                    return;
                }

                // 1. 关闭所有灯
                ui->pushButton_All_Off->click();

                // 2. 打开当前灯
                 ringWidget->clickLed(currentLedIndex);

                // 3. 拍照
                ui->pushButton_Snap->click();

                // 4. 进入下一个灯
                currentLedIndex++;
            });
        }

        currentLedIndex = 0;
        autoScanTimer->start(3000); // 每1.5秒切换一个灯，可根据相机响应调整
    } else {
        qDebug() << "自动扫描停止";
        if (autoScanTimer)
            autoScanTimer->stop();
    }
}


void MainWindow::on_comboBox_LedColorMode_currentIndexChanged(int index)
{
    QString hex = ui->comboBox_LedColorMode->currentData().toString();

    // 设置所有 LED 为统一颜色
    for (int i = 0; i < ringWidget->ledStates.size(); i++) {
        ringWidget->ledStates[i].color = hex;
    }

    ringWidget->refreshLed(); // 刷新 LED 环显示并发送串口
}

