#pragma once

#include <QObject>
#include <QUuid>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QImage>
#include <QRegularExpression>
#include <opencv2/opencv.hpp>
#include "PathManager.h"

#include "Project.h"
#include "MessageQue.h"
#include "QJsonHelper.h"
#include "CAM_IRayple.h"
#include "Logger.h"

#include <QMetaType>


enum CAMERA_TYPE
{
	INDUSTRIAL_CAM,
	IP_CAM
};



extern TMessageQue<FrameInfo> g_imageQueue;

class FrameGrabberThread : public QThread
{
	Q_OBJECT
public:
	FrameGrabberThread(QString camId, QString url);
	~FrameGrabberThread();

	void run();
	void setSetting(QString url);


	void startFrameGrabbing();
	void stopFrameGrabbing();

	

	QString getCamId();


	void setCurrentSecond(double currentSecond);
	void setVideoPath(QString videoPath);
	void setVideoMode(bool _isVideoMode);

private:

	QString _recordDir;
	bool _record = false;

	cv::VideoCapture _cap;
	QString _url;
	QString _videoPath;
	bool _is_running_frame;
	QString _camId;

	bool _isVideoMode = false;
	double _currentSecond;

	CAMERA_TYPE _camType = CAMERA_TYPE::INDUSTRIAL_CAM;

	CAM_IRayple* _iCam;
	// irayple
	void iCam_iniIndustrialCamera();
	bool iCam_loadCameraSettingConfig(QString path);
	bool iCam_verifyKey(QString sn, int key);
	bool iCam_create(QString api);
	bool iCam_connect(QString sn);
	bool iCam_disconnect();
	bool iCam_startGrab();
	bool iCam_stopGrab();
	bool iCam_valid();


signals:
	void frameReady(const cv::Mat&, QString camId, double currentSec);

public slots:
	void iCam_frameReady(FrameInfo fInfo);
	
};