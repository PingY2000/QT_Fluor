QT       += core gui
QT       += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    cameramanager.cpp \
    fluorescence.cpp \
    led_ring_widget.cpp \
    main.cpp \
    mainwindow.cpp \
    motor_lens.cpp \
    serialmanager.cpp

HEADERS += \
    cameramanager.h \
    fluorescence.h \
    led_ring_widget.h \
    mainwindow.h \
    motor_lens.h \
    serialmanager.h \
    toupcam.h

FORMS += \
    mainwindow.ui

LIBS += -L$$PWD -ltoupcam

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
