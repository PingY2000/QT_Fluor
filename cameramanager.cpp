//cameramanager.cpp
#include "cameramanager.h"
#include <QDebug>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QIODevice>
#include <QImageWriter>
#include <QDir>


void CameraManager::setModeName(const QString &Name) {
    modeName = Name;
}

CameraManager::CameraManager(QObject *parent) : QObject(parent)
{
}

CameraManager::~CameraManager()
{
    close();
}

bool CameraManager::isOpen() const {
    return hcam != nullptr;
}

bool CameraManager::open()
{
    ToupcamDeviceV2 arr[TOUPCAM_MAX] = {};
    unsigned cnt = Toupcam_EnumV2(arr);

    if (cnt == 0) {
        qWarning() << "No Toupcam device found.";
        return false;
    }

    hcam = Toupcam_Open(arr[0].id);
    if (!hcam) {
        qWarning() << "Failed to open camera.";
        return false;
    }

    HRESULT hr = Toupcam_StartPullModeWithCallback(hcam, EventCallback, this);
    if (FAILED(hr)) {
        qWarning() << "Failed to start preview.";
        Toupcam_Close(hcam);
        hcam = nullptr;
        return false;
    }

    return true;
}

void CameraManager::close()
{
    if (hcam) {
        Toupcam_Close(hcam);
        hcam = nullptr;
    }
}

void CameraManager::fetchImage() {
    if (!hcam) return;

    lastImageSource = ImageSource::Realtime;
    ToupcamFrameInfoV2 info = {};
    int imgSize = 0;

    // 查询是否有图像
    if (SUCCEEDED(Toupcam_PullImageV2(hcam, nullptr, 24, &info))) {
        //qDebug() << "实时图像分辨率：" << info.width << "x" << info.height;
        imgSize = info.width * info.height * 3;
        QByteArray buffer(imgSize, 0);
        if (SUCCEEDED(Toupcam_PullImageV2(hcam, buffer.data(), 24, &info))) {
            QImage img((uchar*)buffer.data(), info.width, info.height, QImage::Format_BGR888);
            emit imageReady(img.copy());  // 发送信号
        }
    }
}

void CameraManager::snapImage() {
    if (!hcam) return;
    lastImageSource = ImageSource::Snapshot;
    HRESULT hr = Toupcam_Snap(hcam, 0);  // 0 表示使用最高 still 分辨率
    if (FAILED(hr)) {
        qWarning() << "Snap failed";
    }
}

void CameraManager::fetchStillImage() {
    if (!hcam) return;

    ToupcamFrameInfoV2 info = {};
    if (SUCCEEDED(Toupcam_PullStillImageV2(hcam, nullptr, 24, &info))) {
        int imgSize = info.width * info.height * 3;
        QByteArray buffer(imgSize, 0);

        if (SUCCEEDED(Toupcam_PullStillImageV2(hcam, buffer.data(), 24, &info))) {
            QImage img((uchar*)buffer.data(), info.width, info.height, QImage::Format_BGR888);
            emit imageReady(img.copy());  // 可复用现有 imageReady 信号
        }
    }
}

void CameraManager::setSaveDirectory(const QString& dir) {
    saveDirectory = dir;
}

void CameraManager::fetchStillImageTif() {
    if (!hcam) return;

    ToupcamFrameInfoV2 info = {};
    lastImageSource = ImageSource::Snapshot;

    // 拉取处理后的彩色图像，24bit = RGB888
    if (SUCCEEDED(Toupcam_PullStillImageV2(hcam, nullptr, 24, &info))) {
        int rgbSize = info.width * info.height * 3;
        QByteArray rgbBuffer(rgbSize, 0);

        if (SUCCEEDED(Toupcam_PullStillImageV2(hcam, rgbBuffer.data(), 24, &info))) {
            QImage img(reinterpret_cast<const uchar*>(rgbBuffer.constData()),
                       info.width, info.height, QImage::Format_BGR888);

            // 获取曝光时间（微秒）和增益
            unsigned exposure_us = 0;
            unsigned short gain = 0;
            Toupcam_get_ExpoTime(hcam, &exposure_us);
            Toupcam_get_ExpoAGain(hcam, &gain);

            // 构造文件路径，带曝光信息
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
            QString filename = QString("snapshot_%1_%2_exp%3ms_gain%4.tif")
                                   .arg(timestamp, modeName, QString::number(exposure_us / 1000), QString::number(gain));
            QString filePath = QDir(saveDirectory).filePath(filename);

            if (img.save(filePath, "TIFF")) {
                qDebug() << "TIFF 彩色图像已保存到：" << filePath;
            } else {
                qWarning() << "TIFF 彩色图像保存失败";
            }

            emit imageReady(img.copy());
        }
    }
}


void CameraManager::setAutoExposure(bool enabled) {
    if (hcam) {
        Toupcam_put_AutoExpoEnable(hcam, enabled ? TRUE : FALSE);
    }
}

void CameraManager::put_ExpoTime(int Expotime){
    if (hcam) {
        Toupcam_put_ExpoTime(hcam, Expotime*1000);
    }
}

void CameraManager::put_ExpoGain(int ExpoGain){
    if (hcam) {
        Toupcam_put_ExpoAGain(hcam, (unsigned short)ExpoGain);
    }
}

bool CameraManager::getExposureTime(unsigned& time_us) {
    if (hcam) {
        return SUCCEEDED(Toupcam_get_ExpoTime(hcam, &time_us));
    }
    return false;
}

bool CameraManager::getGain(unsigned short &gain)
{
    if (hcam) {
        return SUCCEEDED(Toupcam_get_ExpoAGain(hcam, &gain));
    }
    return false;
}

void __stdcall CameraManager::EventCallback(unsigned nEvent, void* ctx) {
    auto mgr = static_cast<CameraManager*>(ctx);
    if (!mgr) return;

    if (nEvent == TOUPCAM_EVENT_IMAGE) {
        emit mgr->imageArrived();  // 视频图像
    } else if (nEvent == TOUPCAM_EVENT_STILLIMAGE) {
        emit mgr->stillImageArrived();  // 静态图像拍照完成
    }

    if (nEvent == TOUPCAM_EVENT_EXPOSURE) {
        emit mgr->exposureChanged();  // 新增信号
    }
}

