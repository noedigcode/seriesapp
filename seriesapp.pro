#-------------------------------------------------
#
# Project created by QtCreator 2011-08-02T10:35:46
#
#-------------------------------------------------

CONFIG += qt
QT     += core gui widgets network

TARGET = seriesapp
TEMPLATE = app

INCLUDEPATH += src

HEADERS  += \
    src/mainwindow.h

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp

FORMS    += \
    src/mainwindow.ui

RESOURCES += res/icons.qrc

# For Windows exe icon
win32:RC_FILE += res/windowsicon.rc
