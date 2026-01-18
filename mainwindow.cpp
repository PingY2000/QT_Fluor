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

    m_periodTimer = nullptr;
    autoScanTimer = nullptr;
    m_completedCycles = 0;
    m_currentPeriodSeconds = 0;

    // 创建互斥按钮组
    QButtonGroup* group = new QButtonGroup(this);
    group->addButton(ui->pushButton_ModeBright, 0);
    group->addButton(ui->pushButton_Mode365, 1);
    group->addButton(ui->pushButton_Mode488, 2);
    group->addButton(ui->pushButton_Mode532, 3);
    group->setExclusive(true);

    ui->pushButton_ModeBright->setEnabled(false);
    ui->pushButton_Mode365->setEnabled(false);
    ui->pushButton_Mode488->setEnabled(false);
    ui->pushButton_Mode532->setEnabled(false);

    // 2. 连接信号：当组内按钮被点击时触发转盘移动
    connect(group, &QButtonGroup::idClicked, this, [=](int id) {
        handleTurntableSwitch(id);
    });

    ui->pushButton_SnapScheduled->setEnabled(false);



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
    connect(camera, &CameraManager::infoLogRequested, this, &MainWindow::logStatus);

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
    //ui->textEdit_SerialMonitor->moveCursor(QTextCursor::End);
    //ui->textEdit_SerialMonitor->insertPlainText(QString::fromUtf8(data));
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
        ui->spinBox_ExposureTime->setValue(exp / 1000);
    }
    if (camera->getGain(gain)) {
        ui->spinBox_ExpoGain->setValue(gain);
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
                setModeButtonsEnabled(true);
            }
            else
            {
                ui->pushButton_PortSwitch->blockSignals(true);
                ui->pushButton_PortSwitch->setChecked(false);
                ui->pushButton_PortSwitch->blockSignals(false);
                setModeButtonsEnabled(false);
            }
        }
    } else {
        serialManager->closePort();
        if (!serialManager->isOpen()){
            ui->pushButton_PortSwitch->setText("打开");
            setModeButtonsEnabled(false);
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
            ui->pushButton_SnapScheduled->setEnabled(true);
        } else {
            QMessageBox::warning(this, "错误", "相机打开失败！");
            ui->pushButton_CameraSwitch->setChecked(false); // 打开失败时重置按钮状态
            ui->pushButton_SnapScheduled->setEnabled(false);
        }
    } else {
        // 关闭相机
        camera->close();
        ui->pushButton_CameraSwitch->setText("打开相机");
        ui->pushButton_SnapScheduled->setEnabled(false);
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



void MainWindow::on_pushButton_SnapScheduled_toggled(bool checked)
{
    if (checked) {
        ui->pushButton_AutoExpo->setChecked(false);
        // 1. 初始化计数器
        m_RepeatTimes = ui->spinBox_SnapScheduledTimes->value();
        m_completedCycles = 0;
        m_currentPeriodSeconds = 0;

        // 更新 UI 显示
        ui->spinBox_SnapScheduledTimes_Show->setValue(0);
        ui->spinBox_SnapScheduledPeriod_Show->setValue(0);

        if (m_completedCycles >= m_RepeatTimes) {
            ui->pushButton_SnapScheduled->setChecked(false);
            return;
        }
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
        // 通道 3: 488nm
        if (ui->checkBox_Mode488->isChecked()) {
            scanTasks.append({"488", ui->spinBox_Mode488Pos->value(),
                              ui->spinBox_ExposureTime_Mode488->value(),
                              ui->spinBox_ExpoGain_Mode488->value(), true});
        }

        // 通道 4: 532nm
        if (ui->checkBox_Mode532->isChecked()) {
            scanTasks.append({"532", ui->spinBox_Mode532Pos->value(),
                              ui->spinBox_ExposureTime_Mode532->value(),
                              ui->spinBox_ExpoGain_Mode532->value(), true});
        }

        if (scanTasks.isEmpty()) {
            QMessageBox::warning(this, "提醒", "请至少勾选一个启用的模式");
            ui->pushButton_SnapScheduled->setChecked(false);
            return;
        }

        // 3. 启动秒表定时器 (每秒触发一次)
        if (!m_periodTimer) {
            m_periodTimer = new QTimer(this);
            connect(m_periodTimer, &QTimer::timeout, this, [this]() {
                m_currentPeriodSeconds++;
                ui->spinBox_SnapScheduledPeriod_Show->setValue(m_currentPeriodSeconds);

                // 如果到达设定周期
                if (m_currentPeriodSeconds >= ui->spinBox_SnapScheduledPeriod->value()) {
                    m_currentPeriodSeconds = 0;
                    ui->spinBox_SnapScheduledPeriod_Show->setValue(0);

                    // 如果任务还没结束，开启新一轮扫描
                    if (scanState == AutoScanState::Idle || scanState == AutoScanState::Finished) {
                        startNewScanCycle();
                    }
                }
            });
        }
        m_periodTimer->start(1000);



        if (!autoScanTimer) {
            autoScanTimer = new QTimer(this);
            connect(autoScanTimer, &QTimer::timeout, this, &MainWindow::autoScanStep);
        }
        autoScanTimer->start(50);

        startNewScanCycle();
    } else {
        if (autoScanTimer) autoScanTimer->stop();
        if (m_periodTimer) m_periodTimer->stop();
        stopMotion();
        scanState = AutoScanState::Idle;
    }
}

void MainWindow::autoScanStep()
{
    if (currentTaskIndex >= scanTasks.size()) {
        scanState = AutoScanState::Finished;
    }

    switch (scanState) {

    case AutoScanState::MoveTurntable: {
        autoScanTimer->stop(); // 停止轮询，等待移动完成
        CaptureTask task = scanTasks[currentTaskIndex];

        qDebug() << ">>> Step 1: Moving to" << task.modeName << "at position" << task.position;
        logStatus(QString("Moving to %1 at position %2").arg(task.modeName).arg(task.position));

        // 这里的 handleTurntableSwitch 内部会处理电机转动和 settleDurationMs 静止时间
        handleTurntableSwitch(task.position, [=]() {
            // 同步更新 UI 上的模式按钮状态（让用户看到转盘切到了哪）
            if(task.position == 0) ui->pushButton_ModeBright->setChecked(true);
            else if(task.position == 1) ui->pushButton_Mode365->setChecked(true);
            else if(task.position == 2) ui->pushButton_Mode488->setChecked(true);
            else if(task.position == 3) ui->pushButton_Mode532->setChecked(true);

            scanState = AutoScanState::Capture; // 移动稳了，去拍照
            autoScanTimer->start(50);
        });
        break;
    }

    case AutoScanState::Capture: {
        autoScanTimer->stop(); // 进入异步拍照流程，停止轮询
        CaptureTask task = scanTasks[currentTaskIndex];

        qDebug() << ">>> Step 2: Lighting & Capturing" << task.modeName;
        logStatus(QString("Lighting & Capturing :%1").arg(task.modeName));

        // 1. 关闭所有灯 (500ms 消隐)
        ui->pushButton_LightBright->setChecked(false);
        ui->pushButton_Laser365->setChecked(false);
        ui->pushButton_Laser488->setChecked(false);
        ui->pushButton_Laser532->setChecked(false);

        QTimer::singleShot(500, this, [=]() {
            // 2. 开启目标灯光
            if (task.modeName == "Bright") ui->pushButton_LightBright->setChecked(true);
            else if (task.modeName == "365") ui->pushButton_Laser365->setChecked(true);
            else if (task.modeName == "488") ui->pushButton_Laser488->setChecked(true);
            else if (task.modeName == "532") ui->pushButton_Laser532->setChecked(true);

            // 3. 设置参数
            camera->setModeName(task.modeName); // 确保文件名正确
            camera->put_ExpoTime(task.exposure);
            camera->put_ExpoGain(task.gain);

            // 4. 等待曝光稳定 (给相机缓冲区清空的时间)
            int exposureWaitTime = (task.exposure * 2) + 1000;
            QTimer::singleShot(exposureWaitTime, this, [=]() {

                // 5. 连接拍照完成回调
                disconnect(camera, &CameraManager::stillImageArrived, nullptr, nullptr);
                connect(camera, &CameraManager::stillImageArrived, this, [=]() {
                    camera->fetchStillImageTif();

                    // 6. 拍完关灯
                    ui->pushButton_LightBright->setChecked(false);
                    ui->pushButton_Laser365->setChecked(false);
                    ui->pushButton_Laser488->setChecked(false);
                    ui->pushButton_Laser532->setChecked(false);

                    // 7. 进入下一环节
                    scanState = AutoScanState::NextPosition;
                    autoScanTimer->start(50);
                });

                camera->snapImage();
            });
        });
        break;
    }

    case AutoScanState::NextPosition:
        currentTaskIndex++;
        if (currentTaskIndex < scanTasks.size()) {
            scanState = AutoScanState::MoveTurntable; // 还有任务，继续转动
        } else {
            scanState = AutoScanState::Finished;
        }
        break;

    case AutoScanState::Finished:
        autoScanTimer->stop();
        qDebug() << ">>> Cycle complete. Resetting to Home.";
        logStatus("Cycle complete. Resetting to Home.");
        handleTurntableSwitch(0, [=]() {
            ui->pushButton_ModeBright->setChecked(true);
            if (m_completedCycles >= m_RepeatTimes) {
                ui->pushButton_SnapScheduled->setChecked(false);
                m_periodTimer->stop();
                ui->spinBox_SnapScheduledPeriod_Show->setValue(0);
            }
            scanState = AutoScanState::Idle;
        });
        break;

    default: break;
    }
}

void MainWindow::startMotion(int frequency)
{
    fluorescence->setDirection(1); // 固定正向旋转
    fluorescence->setFrequency(frequency);
    fluorescence->setEnabled(true);
}

void MainWindow::stopMotion()
{
    //fluorescence->setEnabled(false);
    fluorescence->setFrequency(0);
}

void MainWindow::handleTurntableSwitch(int targetIndex, std::function<void()> onFinished)
{
    if (targetIndex == m_currentPositionIndex) {
        qDebug() << "Already at position" << targetIndex;
        if (onFinished) onFinished(); // 必须执行回调！
        return;
    }

    // 1. 计算位置差 (假设转盘只能单向正向旋转)
    int steps = targetIndex - m_currentPositionIndex;
    if (steps < 0) steps += 4; // 处理从位置3回到位置0的情况

    // 2. 禁用 UI 交互，防止运动中再次点击
    ui->groupBox_Image->setEnabled(false);
    qDebug() << "Moving Turntable from" << m_currentPositionIndex << "to" << targetIndex;

    // 3. 计算运动总时间
    // 旋转 90 度的时间是 moveDurationMs，旋转 steps * 90 度的时间：
    int totalMoveTime = steps * moveDurationMs;

    // 4. 启动电机
    startMotion(turntableFrequency);

    // 5. 使用定时器在运动完成后停止电机并恢复 UI
    QTimer::singleShot(totalMoveTime, this, [=]() {
        stopMotion();

        // 停稳后的处理
        QTimer::singleShot(settleDurationMs, this, [=]() {
            m_currentPositionIndex = targetIndex; // 更新当前位置记录
            ui->groupBox_Image->setEnabled(true); // 恢复 UI
            qDebug() << "Turntable reached position:" << targetIndex;
            if (onFinished) onFinished();
            // 可以在这里根据当前选中的 ID 自动打开对应的激光
            // 例如：if(targetIndex == 1) ui->pushButton_Laser365->click();
        });
    });
}

void MainWindow::setModeButtonsEnabled(bool enabled)
{
    ui->pushButton_ModeBright->setEnabled(enabled);
    ui->pushButton_Mode365->setEnabled(enabled);
    ui->pushButton_Mode488->setEnabled(enabled);
    ui->pushButton_Mode532->setEnabled(enabled);
}

void MainWindow::startNewScanCycle()
{
    if (m_completedCycles<m_RepeatTimes ) {
        currentTaskIndex = 0;
        scanState = AutoScanState::Capture;
        m_completedCycles++;
        // 更新剩余次数显示
        ui->spinBox_SnapScheduledTimes_Show->setValue(m_completedCycles);

        if (!autoScanTimer) {
            autoScanTimer = new QTimer(this);
            connect(autoScanTimer, &QTimer::timeout, this, &MainWindow::autoScanStep);
        }
        autoScanTimer->start(50);


    } else {
        // 全部次数完成
        ui->pushButton_SnapScheduled->setChecked(false);
    }
}





void MainWindow::on_spinBox_SnapScheduledTimes_valueChanged(int arg1)
{
    m_RepeatTimes=arg1;
}


void MainWindow::logStatus(const QString &message)
{
    // 1. 构造带时间戳的信息
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString logLine = QString("%1   %2").arg(timestamp, message);

    // 2. 依然保留控制台打印，方便调试
    qDebug() << logLine;

    // 3. 追加到 UI (确保在主线程调用)
    ui->textEdit_StateShow->append(logLine);

    // 4. 自动滚动到底部
    ui->textEdit_StateShow->moveCursor(QTextCursor::End);
}
