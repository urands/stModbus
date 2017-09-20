#-------------------------------------------------
#
# Project created by QtCreator 2017-09-13T14:56:41
#
#-------------------------------------------------

QT       -= core gui qt

TARGET = stModbus
TEMPLATE = lib
CONFIG += static

QMAKE_LFLAGS+= -static

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../../include


SOURCES +=  \
    ../../src/modbus.c \
    ../../src/rtu/mbutils.c \



HEADERS +=  \
    ../../include/modbus.h \
    ../../include/modbus_conf.h \
    ../../include/mbutils.h \


CONFIG(release, debug|release) {
    BUILD_TYPE = release

}
CONFIG(debug, debug|release) {
    BUILD_TYPE = debug
    CONFIG +=  debug
    CONFIG -=  qml_debug
}

LICENSE_TEMPLATE = $$PWD/../../LICENSE.md

OBJECTS_DIR = $$PWD/../../build/$${TARGET}_$${BUILD_TYPE}/obj

DESTDIR = $$PWD/../../lib/
