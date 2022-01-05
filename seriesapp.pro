#-------------------------------------------------
#
# Project created by QtCreator 2011-08-02T10:35:46
#
#-------------------------------------------------

CONFIG += qt
QT     += core gui widgets network

TARGET = seriesapp
TEMPLATE = app

HEADERS  += \
    mainwindow.h

SOURCES += main.cpp \
    mainwindow.cpp

FORMS    += \
    mainwindow.ui

RESOURCES += icons.qrc

# For Windows exe icon
win32:RC_FILE += windowsicon.rc
