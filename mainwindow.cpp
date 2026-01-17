//miinwindows.cpp
#include "mainwindow.h"
#include "ui_mainwindow.h"
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
#include <QButtonGroup>




SerialManager *serialManager;

QTimer* autoScanTimer = nullptr;
int currentLedIndex = 0;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)

{
    ui->setupUi(this);

    // 创建互斥按钮组
    QButtonGroup* group = new QButtonGroup(this);
    group->addButton(ui->pushButton_ModeBright);
    group->addButton(ui->pushButton_Mode365);
    group->addButton(ui->pushButton_Mode488);
    group->addButton(ui->pushButton_Mode532);
    group->setExclusive(true);

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


    if (camera && camera->isOpen()) { // 把 LED 状态传给 CameraManager
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
                               .arg(timestamp);

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
    if (checked) {
        qDebug() << "Auto Scan Starting...";
        scanTasks.clear();

        // 采集 UI 上的 4 个通道数据
        // 通道 1: 明场 (Bright)
        if (ui->checkBox_ModeBright->isChecked()) {
            scanTasks.append({"Bright", ui->spinBox_ModeBrightPos->value(),
                              ui->spinBox_ExposureTime_ModeBright->value(),
                              ui->spinBox_ExpoGain_ModeBright->value(), true});
        }
        // 通道 2: 365nm
        if (ui->checkBox_Mode365->isChecked()) {
            scanTasks.append({"365", ui->spinBox_Mode365Pos->value(),
                              ui->spinBox_ExposureTime_Mode365->value(),
                              ui->spinBox_ExpoGain_Mode365->value(), true});
        }
        // ... 以此类推添加 488 和 532 的判断 ...

        if (scanTasks.isEmpty()) {
            QMessageBox::warning(this, "提醒", "请至少勾选一个启用的模式");
            ui->pushButton_SnapScheduled->setChecked(false);
            return;
        }

        currentTaskIndex = 0;
        // 起始动作：由于手动已经调至 0 位，建议第一个任务直接从 Capture 开始
        scanState = AutoScanState::Capture;

        if (!autoScanTimer) {
            autoScanTimer = new QTimer(this);
            connect(autoScanTimer, &QTimer::timeout, this, &MainWindow::autoScanStep);
        }
        autoScanTimer->start(50);
    } else {
        if (autoScanTimer) autoScanTimer->stop();
        stopMotion();
        scanState = AutoScanState::Idle;
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
    fluorescence->sendFluorescenceCommand(checked,ui->pushButton_Laser488->isChecked(),ui->pushButton_Laser532->isChecked());
}


void MainWindow::on_pushButton_Laser488_toggled(bool checked)
{
    fluorescence->sendFluorescenceCommand(ui->pushButton_Laser365->isChecked(),checked,ui->pushButton_Laser532->isChecked());
}


void MainWindow::on_pushButton_Laser532_toggled(bool checked)
{
    fluorescence->sendFluorescenceCommand(ui->pushButton_Laser365->isChecked(),ui->pushButton_Laser488->isChecked(),checked);
}

void MainWindow::on_pushButton_LightBright_toggled(bool checked)
{
    QByteArray data;
    data.append(0xA5);  // HEADER
    data.append(0x5A);
    data.append(0x01);
    data.append(17);

    uint8_t accumulator = 0;
    int bitPos = 0; // 当前 bit 写入位置（0~7）

    for (int i = 0; i < 43; i++) {

        uint8_t v = (checked << 2) | (checked << 1) | checked ;

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

void MainWindow::autoScanStep()
{
    if (currentTaskIndex >= scanTasks.size()) {
        scanState = AutoScanState::Finished;
    }

    switch (scanState) {
    case AutoScanState::Capture: {
        CaptureTask task = scanTasks[currentTaskIndex];
        qDebug() << "Executing Task:" << task.modeName;

        // 1. 设置相机参数
        camera->put_ExpoTime(task.exposure);
        camera->put_ExpoGain(task.gain);

        // 2. 触发 UI 按钮（利用 click() 自动触发之前的指令发送逻辑）
        if (task.modeName == "Bright") ui->pushButton_ModeBright->click();
        else if (task.modeName == "365") ui->pushButton_Mode365->click();
        else if (task.modeName == "488") ui->pushButton_Mode488->click();
        else if (task.modeName == "532") ui->pushButton_Mode532->click();

        // 3. 拍照并等待
        disconnect(camera, &CameraManager::stillImageArrived, nullptr, nullptr);
        connect(camera, &CameraManager::stillImageArrived, this, [=]() {
            camera->fetchStillImageTif();
            scanState = AutoScanState::NextPosition; // 拍照完进入下一阶段
        });
        camera->snapImage();
        scanState = AutoScanState::WaitCaptureDone;
        break;
    }

    case AutoScanState::NextPosition:
        currentTaskIndex++;
        if (currentTaskIndex < scanTasks.size()) {
            scanState = AutoScanState::MoveTurntable;
        } else {
            scanState = AutoScanState::Finished;
        }
        break;

    case AutoScanState::MoveTurntable:
        startMotion(turntableFrequency);
        scanState = AutoScanState::WaitTurntableStable;
        QTimer::singleShot(moveDurationMs, this, [=]() {
            stopMotion();
            QTimer::singleShot(settleDurationMs, this, [=]() {
                scanState = AutoScanState::Capture; // 转完停稳后，去拍照
            });
        });
        break;

    case AutoScanState::Finished:
        ui->pushButton_SnapScheduled->setChecked(false);
        // 结束时建议关闭所有光源
        fluorescence->sendFluorescenceCommand(false, false, false);
        break;

    default: break;
    }
}

void MainWindow::startMotion(int frequency)
{
    // 借用你已有的 motorLens 或 fluorescence 接口控制转盘电机
    // 假设转盘由 fluorescence 里的电机控制
    fluorescence->setDirection(1); // 固定正向旋转
    fluorescence->setFrequency(frequency);
    fluorescence->setEnabled(true);
}

void MainWindow::stopMotion()
{
    fluorescence->setEnabled(false);
    fluorescence->setFrequency(0);
}






