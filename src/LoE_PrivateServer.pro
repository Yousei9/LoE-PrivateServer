#-------------------------------------------------
#
# Project created by QtCreator 2013-06-21T21:58:54
#
#-------------------------------------------------

TARGET = LoE_PrivateServer
VERSION = 0.5.9

QT       += core network
QT       += xml
TEMPLATE = app

# build as a console application
console_only {
    QT       -= gui
    CONFIG   -= app_bundle
    CONFIG   += console
    DEFINES  += USE_CONSOLE
}
# build with a gui
else {
    QT       += gui widgets
    FORMS    += app.ui
    RESOURCES += app.qrc
    QTPLUGIN += qpng
    DEFINES  += USE_GUI
}


DEFINES += APP_NAME=\\\"$${TARGET}\\\" \
    APP_VERSION=\\\"$${VERSION}\\\"

SOURCES += main.cpp \
    tcp.cpp \
    udp.cpp \
    messages.cpp \
    utils.cpp \
    pingTimeout.cpp \
    scene.cpp \
    dataType.cpp \
    sync.cpp \
    receiveMessage.cpp \
    sendMessage.cpp \
    serverCommands.cpp \
    quest.cpp \
    serialize.cpp \
    items.cpp \
    receiveAck.cpp \
    receiveChatMessage.cpp \
    mobsParser.cpp \
    mob.cpp \
    mobStats.cpp \
    skill.cpp \
    skillparser.cpp \
    animationparser.cpp \
    animation.cpp \
    log.cpp \
    player.cpp \
    playerSerialization.cpp \
    sceneEntity.cpp \
    settings.cpp \
    app.cpp \
    appStartStopServer.cpp

HEADERS  += \
    build.h \
    message.h \
    utils.h \
    scene.h \
    dataType.h \
    sync.h \
    quest.h \
    serialize.h \
    items.h \
    sendMessage.h \
    receiveAck.h \
    receiveChatMessage.h \
    mobzone.h \
    sceneEntity.h \
    mobsParser.h \
    mob.h \
    mobsStats.h \
    packetloss.h \
    skill.h \
    skillparser.h \
    statsComponent.h \
    animationparser.h \
    animation.h \
    log.h \
    player.h \
    settings.h \
    udp.h \
    app.h

TRANSLATIONS = ../translations/fr.ts \
    ../translations/ru.ts

# include coreservices (required for timestamps) only on mac
macx {
    QMAKE_LFLAGS += -F /System/Library/Frameworks/CoreServices.framework/
    LIBS += -framework CoreServices
}

CONFIG += c++11
