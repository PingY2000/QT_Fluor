//serialmanager.cpp
#include "serialmanager.h"
#include <QString>
#include <QDebug>

SerialManager::SerialManager(QObject *parent) : QObject(parent)
{
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &SerialManager::handleReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError error){
        if (error != QSerialPort::NoError)
            emit errorOccurred(serial->errorString());
    });
}

SerialManager::~SerialManager()
{
    closePort();
}

bool SerialManager::openPort(const QString &portName, qint32 baudRate)
{
    if (serial->isOpen()) serial->close();
    serial->setPortName(portName);
    serial->setBaudRate(baudRate);
    serial->setDataBits(QSerialPort::Data8);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setParity(QSerialPort::NoParity);
    serial->setFlowControl(QSerialPort::NoFlowControl);
    return serial->open(QIODevice::ReadWrite);
}

void SerialManager::closePort()
{
    if (serial->isOpen()) serial->close();
}

bool SerialManager::isOpen() const
{
    return serial->isOpen();
}

quint8 SerialManager::calculateCRC8(const QByteArray &data) {
    quint8 crc = 0x00;
    for (quint8 byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x07;
            else
                crc <<= 1;
        }
    }
    return crc;
}

void SerialManager::sendData(const QByteArray &data)
{
    if (serial->isOpen()){
        qDebug() << "Sending:" << data.toHex(' ').toUpper();
        serial->write(data);
    }
}

void SerialManager::sendDatawithCRC(const QByteArray &data)
{
    if (serial->isOpen()){
        quint8 crc = calculateCRC8(data);
        QByteArray toSend = data;
        toSend.append(crc);
        qDebug() << "Sending:" << toSend.toHex(' ').toUpper();
        serial->write(toSend);
    }
}

void SerialManager::handleReadyRead()
{
    emit dataReceived(serial->readAll());
}
