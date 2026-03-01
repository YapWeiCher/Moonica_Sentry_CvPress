#pragma once

#include <QObject>
#include <QDir> 
#include <QDebug> 
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QJsonArray>
#include <opencv2/opencv.hpp>
#include <QPoint>
#include <QSize>
#include "PathManager.h"


class ViewInfo
{
public:

	QString viewName;
	QString viewPath;
	
	//settings
	QString cctvUrl;
	QString extension;
	bool isCalibrated;

	cv::Mat homographyMatrix;
	QVector<QPointF> imagePointVec;
	QVector<QPointF> floorPointVec;

	bool isDoorCam = false;
	QString doorPosition; // Top, Left, Right, Bottom  

	QVector<QPointF> doorPointVec;
	QVector<QPointF> perspectivePointVec;

	double viewWidth;
	double viewHeight;

};