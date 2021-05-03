QT       += core gui
TARGET = CvMoviePlot

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

#OPENCV_PATH = /usr/local/Cellar/opencv/4.3.0_5
OPENCV_PATH = $$files(/usr/local/Cellar/opencv/*)
message(Using OpenCV at: $$OPENCV_PATH)

LIBS += -L/$$OPENCV_PATH/lib/ \
        -lopencv_core \
        -lopencv_imgproc \
        -lopencv_videoio

INCLUDEPATH += $$OPENCV_PATH/include/opencv4
DEPENDPATH += $$OPENCV_PATH/include

INCLUDEPATH += /Users/erik/Developer/CvPlot/CvPlot/inc
DEPENDPATH += /Users/erik/Developer/CvPlot/CvPlot/inc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
