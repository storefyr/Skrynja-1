TARGET = harbour-skrynja-1

CONFIG += sailfishapp

QT += qml quick

isEmpty(VERSION) {
    VERSION = 0.1.0
}
DEFINES += APP_VERSION=\\\\\\"$$VERSION\\\\\\"

SOURCES += src/main.cpp \
    src/acpframereader.cpp \
    src/acpclient.cpp

HEADERS += src/acpframereader.h \
    src/acpclient.h

DISTFILES += qml/harbour-skrynja-1.qml \
    qml/pages/StartSide.qml \
    rpm/harbour-skrynja-1.spec \
    harbour-skrynja-1.desktop

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
