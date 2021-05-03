#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <QDebug>

class SizeGripItem;
class QGraphicsScene;
class QGraphicsPixmapItem;
class QLabel;
class QElapsedTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void onTimerTimeout();

private slots:
    void on_pushButtonLoadVideo_clicked();

    void on_pushButtonPlay_clicked();
    void on_pushButtonPause_clicked();
    void on_pushButtonFrameBack_clicked();
    void on_pushButtonFrameForward_clicked();

    void on_spinBoxFrame_valueChanged(int arg1);

    void on_spinBoxFrame_editingFinished();

    void on_pushButtonLoadGraph_clicked();

    void on_pushButtonZoomOut_clicked();

    void on_pushButtonZoomIn_clicked();

    void on_doubleSpinBoxFpsSet_valueChanged(double arg1);

    void on_pushButtonOpenWriter_clicked();

    void on_pushButtonCloseWriter_clicked();

    void on_checkBoxHasHeaders_clicked();

    void on_checkBoxAutoScaleX_toggled(bool checked);

    void on_checkBoxAutoScaleY_toggled(bool checked);

    void on_spinBoxGraphX_valueChanged(int arg1);

    void on_spinBoxGraphY_valueChanged(int arg1);

    void on_spinBoxGraphW_valueChanged(int arg1);

    void on_spinBoxGraphH_valueChanged(int arg1);

private:
    Ui::MainWindow *ui;
    QGraphicsScene* m_scene;
    QGraphicsPixmapItem* m_pixmap_frame;
    QStringList m_graph_data;
    QVector<double> m_graph_keys;
    QVector<double> m_graph_values;

    cv::VideoCapture m_capture;
    cv::Mat m_source;
    cv::VideoWriter m_writer;
    QElapsedTimer* m_fps_timer;

    QTimer* m_timer;
    void handleFrame();

    void startVideoRecording(const QString &file_name, const QString& format, int w, int h, double fps);
    void stopVideoRecording();
    void writeFrame(const cv::Mat &frame);
};


static inline QImage  cvMatToQImage( const cv::Mat &inMat )
{
    switch ( inMat.type() )
    {
    // 8-bit, 4 channel
    case CV_8UC4:
    {
        QImage image( inMat.data, inMat.cols, inMat.rows, inMat.step, QImage::Format_RGB32 );

        return image;
    }

        // 8-bit, 3 channel
    case CV_8UC3:
    {
        QImage image( inMat.data, inMat.cols, inMat.rows, inMat.step, QImage::Format_RGB888 );

        return image.rgbSwapped();
    }

        // 8-bit, 1 channel
    case CV_8UC1:
    {
        static QVector<QRgb>  sColorTable;

        // only create our color table once
        if ( sColorTable.isEmpty() )
        {
            for ( int i = 0; i < 256; ++i )
                sColorTable.push_back( qRgb( i, i, i ) );
        }

        QImage image( inMat.data, inMat.cols, inMat.rows, inMat.step, QImage::Format_Indexed8 );

        image.setColorTable( sColorTable );

        return image;
    }

    default:
        qWarning() << "ASM::cvMatToQImage() - cv::Mat image type not handled in switch:" << inMat.type();
        break;
    }

    return QImage();
}

static inline QPixmap cvMatToQPixmap( const cv::Mat &inMat )
{
    return QPixmap::fromImage( cvMatToQImage( inMat ) );
}

static inline cv::Mat QImageToCvMat( const QImage &inImage, bool inCloneImageData = true )
{
    switch ( inImage.format() )
    {
    // 8-bit, 4 channel
    case QImage::Format_RGB32:
    {
        cv::Mat  mat( inImage.height(), inImage.width(), CV_8UC4, const_cast<uchar*>(inImage.bits()), inImage.bytesPerLine() );

        return (inCloneImageData ? mat.clone() : mat);
    }

        // 8-bit, 3 channel
    case QImage::Format_RGB888:
    {
        if ( !inCloneImageData )
            qWarning() << "ASM::QImageToCvMat() - Conversion requires cloning since we use a temporary QImage";

        QImage   swapped = inImage.rgbSwapped();

        return cv::Mat( swapped.height(), swapped.width(), CV_8UC3, const_cast<uchar*>(swapped.bits()), swapped.bytesPerLine() ).clone();
    }

        // 8-bit, 1 channel
    case QImage::Format_Indexed8:
    {
        cv::Mat  mat( inImage.height(), inImage.width(), CV_8UC1, const_cast<uchar*>(inImage.bits()), inImage.bytesPerLine() );

        return (inCloneImageData ? mat.clone() : mat);
    }

    default:
        qWarning() << "ASM::QImageToCvMat() - QImage format not handled in switch:" << inImage.format();
        break;
    }

    return cv::Mat();
}

// If inPixmap exists for the lifetime of the resulting cv::Mat, pass false to inCloneImageData to share inPixmap's data
// with the cv::Mat directly
//    NOTE: Format_RGB888 is an exception since we need to use a local QImage and thus must clone the data regardless
static inline cv::Mat QPixmapToCvMat( const QPixmap &inPixmap, bool inCloneImageData = true )
{
    return QImageToCvMat( inPixmap.toImage(), inCloneImageData );
}

#endif // MAINWINDOW_H
