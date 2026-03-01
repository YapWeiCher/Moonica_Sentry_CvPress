#ifndef VIDEOTEST_H
#define VIDEOTEST_H
#pragma once
#include <QObject>
#include <QThread>
#include <QImage>
#include <atomic>
#include <QDebug>
#include<opencv2/opencv.hpp>


class VideoTester : public QObject
{
	Q_OBJECT

public:
	VideoTester(QObject* parent = nullptr);
	~VideoTester();

public slots:
	void startGrabbing(const QString& url);
	void stopGrabbing();

signals:
	void newFrameReady(const QImage& image);
	void finished();

private:
	cv::VideoCapture _cap;
	std::atomic<bool> _isRunning;
};

#endif

