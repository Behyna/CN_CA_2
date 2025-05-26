QT       += core network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SCHIFRA_DIR  = $$PWD/schifra          # tweak if your layout is different
INCLUDEPATH += $$SCHIFRA_DIR          # lets the compiler find the headers
DEPENDPATH  += $$SCHIFRA_DIR          # lets qmake know they exist

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    chunkserver.cpp \
    cli.cpp \
    client.cpp \
    errorcorrection.cpp \
    filemanager.cpp \
    main.cpp \
    manager.cpp \
    node.cpp \
    reedsolomon.cpp \
    schifrawrapper.cpp \
    tcpmessage.cpp \
    tcpsocket.cpp \
    udpsocket.cpp

HEADERS += \
    chunkserver.h \
    cli.h \
    client.h \
    errorcorrection.h \
    filemanager.h \
    manager.h \
    node.h \
    reedsolomon.h \
    schifrawrapper.h \
    tcpmessage.h \
    tcpsocket.h \
    udpsocket.h \

FORMS +=

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
