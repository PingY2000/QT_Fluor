//serialmanager.h
#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>

class SerialManager : public QObject
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    bool openPort(const QString &portName, qint32 baudRate = QSerialPort::Baud115200);
    void closePort();
    bool isOpen() const;


    void sendData(const QByteArray &data);
    void sendDatawithCRC(const QByteArray &data);

signals:
    void dataReceived(const QByteArray &data);
    void errorOccurred(const QString &errorString);

private slots:
    void handleReadyRead();

private:
    QSerialPort *serial;
    quint8 calculateCRC8(const QByteArray &data);
};

#endif // SERIALMANAGER_H
