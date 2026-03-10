#pragma once

#include <QObject>
#include <QDir> 
#include <QDebug> 
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QJsonArray>
class PathManager
{
public:

	static void makeDir(QString& dir);
	static void setAllPath();
	static void setProject(QString recipe);
	static bool clearDirectory(const QString& path);


	static QStringList _imageExtensions;
	static QString _curProject;

	static QString _rootPath;
	static QString _projectListDir;
	static QString _projectRootDir;
	static QString _appSettingPath;

	static QString _humanTrackingModelDir;
	static QString _objectDetectionModelDir;
	static QString _featureExtractionModelDir;

	// project workspace
	static QString _camViewsDir;
	static QString _camViewsInfoDir;
	static QString _camViewsMatrixDir;
	static QString _floorViewsDir;
	static QString _recordedVideoDir;


	// report
	static QString _cleaningReportDir;

	static QString _dbUserAccountPath;



};