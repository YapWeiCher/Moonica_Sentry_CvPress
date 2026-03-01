#pragma once

#include <QObject>
#include <QDir> 
#include <QDebug> 
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <opencv2/opencv.hpp>

#include "PathManager.h"
#include "ViewInfo.h"

struct ProjectSetting
{
	QString humanTracking_modelName;
	double humanTracking_accuracy;
	bool humanTracking_enableFeatureExtraction;

	QString objectDetection_modelName;
	double objectDetection_accuracy;

	bool isGlobalViewChecking = false;



	int viewRow;
	int viewCol;

	QStringList viewSequenceList;
};

class Project
{
public:

	QString _projectId;
	QString _projectName;
	QList<ViewInfo> _viewList; //order of cctv added

	QHash<QString, ViewInfo> _viewInfoHash; // key : cctv name
	ViewInfo _floorViewInfo;

	ProjectSetting _settings;

	void resetProject();

	bool loadProject(QString recipeName);
	bool saveProject();

	bool saveHomographyMatrix();
	bool loadHomographyMatrix();

	bool saveViewImages();
	bool loadViewImages();

	bool saveFloorImages();
	bool loadFloorImages();

	bool saveProjectSetting();
	bool loadProjectSetting();
};