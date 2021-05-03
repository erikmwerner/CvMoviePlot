#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <opencv2/imgproc.hpp>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QStandardPaths>
#include <QFileDialog>
#include <QSettings>
#include <QTimer>
#include <QElapsedTimer>

#define CVPLOT_HEADER_ONLY
#include <CvPlot/cvplot.h>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow), m_fps_timer(new QElapsedTimer())
{
    ui->setupUi(this);
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTimerTimeout);
    m_scene = new QGraphicsScene(this);
    m_pixmap_frame = new QGraphicsPixmapItem();
    m_scene->addItem(m_pixmap_frame);
    ui->graphicsView->setScene(m_scene);
    ui->graphicsView->setBackgroundBrush(QBrush(QColor(Qt::black)));
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pushButtonLoadVideo_clicked()
{
    QSettings settings;
    QString dir = settings.value("lastFileOpenDir").toString();
    QString file_name = QFileDialog::getOpenFileName(this, "Open file",dir);
    if(file_name.isEmpty()) {
        return;
    }
    else {
        ui->lineEditFileName->setText(file_name);
        if(m_capture.open(file_name.toStdString())) {
            m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
            double total_frames = m_capture.get(cv::CAP_PROP_FRAME_COUNT);
            ui->spinBoxFrame->setSuffix(QString("/%1").arg(total_frames));

            double frame_w = m_capture.get(cv::CAP_PROP_FRAME_WIDTH);
            double frame_h = m_capture.get(cv::CAP_PROP_FRAME_HEIGHT);

            double fps = m_capture.get(cv::CAP_PROP_FPS);
            ui->doubleSpinBoxFpsSet->setSuffix(QString(" (%1 native)").arg(fps));
            ui->doubleSpinBoxFpsSet->setValue(fps);
            ui->doubleSpinBoxWriterFps->setValue(fps);

            ui->labelVideoFormat->setText(QString("Video format:%1x%2 px \n%3 FPS")
                                          .arg(frame_w).arg(frame_h).arg(fps));

            ui->graphicsView->setSceneRect(0,0,frame_w,frame_h);

            ui->spinBoxGraphY->setRange(0,frame_h);
            ui->spinBoxGraphX->setRange(0,frame_w);
            ui->spinBoxGraphH->setRange(0,frame_h);
            ui->spinBoxGraphW->setRange(0,frame_w);

            ui->spinBoxGraphX->setValue(0);
            ui->spinBoxGraphY->setValue(frame_h/2);
            ui->spinBoxGraphW->setValue(frame_w);
            ui->spinBoxGraphH->setValue(frame_h/2);

            m_capture.read(m_source);
            //m_processed = cv::Mat::zeros( m_source.size(), CV_8UC3 );
            handleFrame();
        }
        settings.setValue("lastFileOpenDir", file_name);
    }

}

void MainWindow::on_pushButtonPlay_clicked()
{
    if(m_capture.isOpened()) {
        double fps = m_capture.get(cv::CAP_PROP_FPS);
        int interval = 1000.0/fps;
        m_timer->start(interval);

        ui->pushButtonPlay->setEnabled(false);
        ui->pushButtonPause->setEnabled(true);
    }
}

void MainWindow::on_pushButtonPause_clicked()
{
    if(m_capture.isOpened()) {
        m_timer->stop();

        ui->pushButtonPlay->setEnabled(true);
        ui->pushButtonPause->setEnabled(false);
    }
}

void MainWindow::handleFrame()
{
    if(m_source.empty()) {
        return;
    }

    double rotation = ui->doubleSpinBoxRotate->value();
    if(!qFuzzyIsNull(rotation)) {
        cv::Point2f center(m_source.cols/2., m_source.rows/2.);          //point from where to rotate
        double scale = ui->doubleSpinBoxScale->value();
        cv::Mat r = getRotationMatrix2D(center, rotation, scale);      //Mat object for storing after rotation
        warpAffine(m_source, m_source, r, cv::Size(m_source.cols, m_source.rows));  ///applie an affine transforation to image.
    }

    //qDebug()<<"transformed source dim"<<m_source.cols<<m_source.rows;

    // get the frame rectangle from the graphics view
    QRect frame_rect = ui->graphicsView->sceneRect().toRect();

    // create an axes object over the source image
    //auto axes = CvPlot::plotImage(m_source, cv::Rect2d(0,0,frame_rect.width(), frame_rect.height()));
    double frame = ui->spinBoxFrame->value();

    // calculate data rate relatie to video frame rate
    double coeff = static_cast<double>(ui->spinBoxXRate->value())/ui->doubleSpinBoxFpsSet->value();


    int offset = ui->spinBoxXOffset->value() + frame*coeff;
    int window = ui->spinBoxXWindow->value();
    QVector<double> frame_keys = m_graph_keys.mid(offset, window);
    QVector<double> frame_values = m_graph_values.mid(offset, window);
    //qDebug()<<"coeff:"<<coeff<<"offset:"<<offset<<"n pts:"<<frame_keys.length();

    //auto axes = CvPlot::plot(std::vector<double>(frame_values.begin(), frame_values.end()), "-b");

    auto axes = CvPlot::makePlotAxes();

    axes.create<CvPlot::Series>(std::vector<double>(frame_keys.begin(), frame_keys.end()),std::vector<double>(frame_values.begin(), frame_values.end()), "-b").setLineWidth(ui->spinBoxLineWeight->value());

    axes.setMargins(ui->spinBoxMarginLeft->value(),ui->spinBoxMarginRight->value(),ui->spinBoxMarginTop->value(),ui->spinBoxMarginBottom->value());

    axes.enableHorizontalGrid();
    int x = ui->spinBoxGraphX->value();
    int y = ui->spinBoxGraphY->value();
    int w = ui->spinBoxGraphW->value();
    int h = ui->spinBoxGraphH->value();

    cv::Rect rect(x, y, w ,h);
    cv::Mat roi = m_source(rect);
    //qDebug()<<"frame rect:"<<frame_rect;
    // axes.setYLimAuto(false);
    axes.xLabel(ui->lineEditXLabel->text().toStdString());
    axes.yLabel(ui->lineEditYLabel->text().toStdString());

    // check tightness
    if(ui->checkBoxTightenX->isChecked()) {
        axes.setXTight(true);
    }
    if(ui->checkBoxTightenY->isChecked()) {
        axes.setYTight(true);
    }

    // check axes scaling
    if(ui->checkBoxAutoScaleX->isChecked()){
        axes.setXLimAuto(true);
    }
    else {
        axes.setXLimAuto(false);
        double min = ui->doubleSpinBoxXMin->value();
        double max = ui->doubleSpinBoxXMax->value();
        axes.setXLim(std::pair<double,double>(min, max));
    }
    if(ui->checkBoxAutoScaleY->isChecked()){
        axes.setYLimAuto(true);
    }
    else {
        axes.setYLimAuto(false);
        double min = ui->doubleSpinBoxYMin->value();
        double max = ui->doubleSpinBoxYMax->value();
        axes.setYLim(std::pair<double,double>(min, max));
    }

    cv::Mat graph_mat = axes.render(rect.height,rect.width);

    double alpha = ui->doubleSpinBoxGraphAlpha->value();
    double beta = ( 1.0 - alpha );
    addWeighted( graph_mat, alpha, roi, beta, 0.0, roi);

    roi.copyTo(m_source(rect));



    m_pixmap_frame->setPixmap(cvMatToQPixmap(m_source));


    /*  double avg = cv::mean(m_processed).val[0];

    if(ui->checkBoxCapture->isChecked()){
        QString data = QString("%1,%2").arg(frame).arg(avg);
        ui->textEdit->append(data);
    }

    ui->labelSource->setPixmap(cvMatToQPixmap(m_source));
    ui->labelProcessed->setPixmap(cvMatToQPixmap(m_processed));*/

    QSignalBlocker blocker(ui->spinBoxFrame);

    frame++;
    ui->spinBoxFrame->setValue(frame);

    ui->labelCurrentFps->setText(QString("FPS: %1").arg(QString::number(1000.0/m_fps_timer->elapsed(),'f',2)));

    if(m_writer.isOpened()) {
        writeFrame(m_source);
    }

    m_fps_timer->restart();
}

void MainWindow::onTimerTimeout()
{
    if(m_capture.read(m_source)) {
        handleFrame();
    }
    else {
        if(ui->checkBoxLoop->isChecked()) {
            if(m_capture.isOpened()) {
                m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);
                ui->spinBoxFrame->setValue(0);
                if(m_capture.read(m_source)) {
                    handleFrame();
                    return;
                }
            }
            on_pushButtonPause_clicked();
        }
        else {
            on_pushButtonPause_clicked();
        }
    }
}

void MainWindow::on_pushButtonFrameBack_clicked()
{
    if(m_capture.isOpened()) {
        double frame = m_capture.get(cv::CAP_PROP_POS_FRAMES);
        m_capture.set(cv::CAP_PROP_POS_FRAMES, frame - 2);
        if(m_capture.read(m_source))
            handleFrame();
        QSignalBlocker blocker(ui->spinBoxFrame);
        ui->spinBoxFrame->setValue(frame - 2);
    }
}

void MainWindow::on_pushButtonFrameForward_clicked()
{
    if(m_capture.isOpened()) {
        double frame = m_capture.get(cv::CAP_PROP_POS_FRAMES);
        m_capture.set(cv::CAP_PROP_POS_FRAMES, frame);
        if(m_capture.read(m_source))
            handleFrame();
        QSignalBlocker blocker(ui->spinBoxFrame);
        ui->spinBoxFrame->setValue(frame);
    }
}

void MainWindow::on_spinBoxFrame_valueChanged(int arg1)
{
    if(m_capture.isOpened()) {
        m_capture.set(cv::CAP_PROP_POS_FRAMES, arg1);
        if(m_capture.read(m_source))
            handleFrame();
        QSignalBlocker blocker(ui->spinBoxFrame);
        ui->spinBoxFrame->setValue(arg1);
    }
}


void MainWindow::on_spinBoxFrame_editingFinished()
{
    if(m_capture.isOpened()) {
        //double frame = m_capture.get(cv::CAP_PROP_POS_FRAMES);
        double frame = ui->spinBoxFrame->value();
        m_capture.set(cv::CAP_PROP_POS_FRAMES, frame - 1);
        if(m_capture.read(m_source))
            handleFrame();
        QSignalBlocker blocker(ui->spinBoxFrame);
        ui->spinBoxFrame->setValue(frame - 1);
    }
}

void MainWindow::on_pushButtonLoadGraph_clicked()
{
    QSettings settings;
    QString last_dir = settings.value("lastLoadGraphName").toString();

    QString file_name = QFileDialog::getOpenFileName(
                this, tr("Load Graph Data"), last_dir, tr("CSV (*.csv)"));
    if(file_name.isEmpty()) {
        return;
    }
    QFile file(file_name);
    if( file.open(QFile::ReadOnly) ){
        QString data(file.readAll());
        m_graph_data = data.split("\n");

        // handle headers
        on_checkBoxHasHeaders_clicked();
    }

    settings.setValue("lastLoadGraphName",file_name);
    qDebug()<<"loaded"<<m_graph_values.length()<<"graph values...";
}
/////

void MainWindow::on_pushButtonZoomOut_clicked()
{
    QMatrix matrix = ui->graphicsView->matrix();
    matrix.scale(1.0/1.5, 1.0/1.5);
    ui->graphicsView->setMatrix(matrix);
}

void MainWindow::on_pushButtonZoomIn_clicked()
{
    QMatrix matrix = ui->graphicsView->matrix();
    matrix.scale(1.5, 1.5);
    ui->graphicsView->setMatrix(matrix);
}

void MainWindow::on_doubleSpinBoxFpsSet_valueChanged(double arg1)
{
    qDebug()<<"try set fps";
    if(m_capture.isOpened()) {
        m_capture.set(cv::CAP_PROP_FPS, arg1);
        qDebug()<<"set capture fps:"<<arg1;
        m_timer->setInterval(1000.0/arg1);
    }
}

void MainWindow::on_pushButtonOpenWriter_clicked()
{
    QSettings settings;
    QString last_dir = settings.value("lastVideoSaveFileName").toString();

    QString file_name = QFileDialog::getSaveFileName(
                this, tr("Write Video"), last_dir, tr("MP4 (*.mp4)"));

    if(!file_name.isEmpty()) {
        ui->lineEditFileNameOut->setText(file_name);

        settings.setValue("lastVideoSaveFileName",file_name);

        QRect frame_rect = ui->graphicsView->sceneRect().toRect();
        double fps = ui->doubleSpinBoxFpsSet->value();

        startVideoRecording(ui->lineEditFileNameOut->text(), "mp4", frame_rect.width(), frame_rect.height(), ui->doubleSpinBoxWriterFps->value());
    }


}

void MainWindow::on_pushButtonCloseWriter_clicked()
{
    stopVideoRecording();
}


void MainWindow::on_checkBoxHasHeaders_clicked()
{
    m_graph_values.clear();
    m_graph_keys.clear();

    if(ui->checkBoxHasHeaders->isChecked()) {
        // set ui labels with headers
        QString headers = m_graph_data.first();
        QStringList headers_split = headers.split(",");
        if(headers_split.length() >= 2) {
            // m_graph_values.pop_front(); // remove header entries
            ui->lineEditXLabel->setText(headers_split.at(0).trimmed());
            ui->lineEditYLabel->setText(headers_split.at(1).trimmed());
        }

        m_graph_values.reserve(m_graph_data.length() - 1);
        m_graph_keys.reserve(m_graph_data.length() - 1);
        foreach(QString value, m_graph_data.mid(1)) {
            QStringList data_split = value.split(",");
            if(data_split.length() >= 2) {
                m_graph_keys << data_split.at(0).toDouble();
                m_graph_values << data_split.at(1).toDouble();
            }
        }
    }
    else {
        m_graph_values.reserve(m_graph_data.length());
        m_graph_keys.reserve(m_graph_data.length());
        foreach(QString value, m_graph_data) {
            QStringList data_split = value.split(",");
            if(data_split.length() >= 2) {
                m_graph_keys << data_split.at(0).toDouble();
                m_graph_values << data_split.at(1).toDouble();
            }
        }
    }
}


void MainWindow::startVideoRecording(const QString &file_name, const QString &format, int w, int h, double fps)
{
    // the writer is automatically closed if it was already open
    int fcc = -1;
    if(format == "mp4") {
        fcc = cv::VideoWriter::fourcc('m','p','4','v');
    }
    if(format == "avi") {
        fcc = cv::VideoWriter::fourcc('M','J','P','G');
    }
    if(format == "h264"){
        fcc = cv::VideoWriter::fourcc('h','2','6','4');
    }

    //int fcc = cv::VideoWriter::fourcc('H','2','6','4');
    //int fcc = cv::VideoWriter::fourcc('m','p','4','v');
    cv::Size s(w, h);
    if(m_writer.open(file_name.toStdString(), fcc, fps, s)){
        qDebug()<< QString("Recording video (%1 x %2, %3 FPS) to:" + file_name).arg(w).arg(h).arg(fps);
    }
    else {
        qDebug()<< "Unable to record video to:" << file_name;
    }
}

void MainWindow::stopVideoRecording()
{
    m_writer.release();
    qDebug()<<"Video recording stopped";
}

void MainWindow::writeFrame(const cv::Mat &frame)
{
    if(m_writer.isOpened()) {
        if(frame.empty()) {
            qDebug()<<"empty frame";
        }
        m_writer.write(frame);
        //qDebug()<<"wrote frame"<<frame.cols<<frame.ows<<frame.channels();
    }
    else {
        qDebug()<<"writer closed";
    }
}

void MainWindow::on_checkBoxAutoScaleX_toggled(bool checked)
{
    ui->doubleSpinBoxXMin->setEnabled(!checked);
    ui->doubleSpinBoxXMax->setEnabled(!checked);
}

void MainWindow::on_checkBoxAutoScaleY_toggled(bool checked)
{
    ui->doubleSpinBoxYMin->setEnabled(!checked);
    ui->doubleSpinBoxYMax->setEnabled(!checked);
}

// check limits

void MainWindow::on_spinBoxGraphX_valueChanged(int arg1)
{
    QRect output_rect = ui->graphicsView->sceneRect().toRect();
    int w = ui->spinBoxGraphW->value();
    if(arg1 + w > output_rect.width()) {
        ui->spinBoxGraphW->setValue(output_rect.width() - arg1);
    }
}

void MainWindow::on_spinBoxGraphY_valueChanged(int arg1)
{
   QRect output_rect = ui->graphicsView->sceneRect().toRect();
    int h = ui->spinBoxGraphH->value();
    if(arg1 + h > output_rect.height()) {
        ui->spinBoxGraphH->setValue(output_rect.height() - arg1);
    }
}

void MainWindow::on_spinBoxGraphW_valueChanged(int arg1)
{
    QRect output_rect = ui->graphicsView->sceneRect().toRect();
    int x = ui->spinBoxGraphX->value();
    if(arg1 + x > output_rect.width()) {
        ui->spinBoxGraphX->setValue(output_rect.width() - arg1);
    }
}

void MainWindow::on_spinBoxGraphH_valueChanged(int arg1)
{
    QRect output_rect = ui->graphicsView->sceneRect().toRect();
    int y = ui->spinBoxGraphY->value();
    if(arg1 + y > output_rect.height()) {
        ui->spinBoxGraphY->setValue(output_rect.height() - arg1);
    }
}
