#include "VideoTest.h"
VideoTester::VideoTester(QObject* parent) : QObject(parent), _isRunning(false) {}
VideoTester::~VideoTester()
{
	stopGrabbing();
}
void VideoTester::stopGrabbing()
{
	_isRunning = false;
}
void VideoTester::startGrabbing(const QString& url)
{
	_isRunning = true;
	if (!_cap.open(url.toStdString(), cv::CAP_FFMPEG))
	{
		qDebug() << "Cant find cctv: "<< url;
		emit finished();
		return;
	}

	cv::Mat frame;
	while (_isRunning)
	{
		if (_cap.read(frame) && !frame.empty())
		{
			cv::Mat bgrFrame;
			cv::cvtColor(frame, bgrFrame, cv::COLOR_BGR2RGB);
			QImage img(bgrFrame.data, bgrFrame.cols, bgrFrame.rows, bgrFrame.step, QImage::Format_RGB888);
			emit newFrameReady(img.copy());
		}
		//QThread::msleep(33);
	}

	_cap.release();
	qDebug() << "VideoTester had finished";
	emit finished();
}