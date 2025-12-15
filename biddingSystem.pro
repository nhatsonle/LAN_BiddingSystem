QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    src/models/auctionitem.cpp \
    src/models/bid.cpp \
    src/models/auctionrepository.cpp \
    src/models/itemlistmodel.cpp \
    src/views/itemlistview.cpp \
    src/views/itemdetailview.cpp \
    src/views/bidpanel.cpp

HEADERS += \
    mainwindow.h \
    src/models/auctionitem.h \
    src/models/bid.h \
    src/models/auctionrepository.h \
    src/models/itemlistmodel.h \
    src/views/itemlistview.h \
    src/views/itemdetailview.h \
    src/views/bidpanel.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
