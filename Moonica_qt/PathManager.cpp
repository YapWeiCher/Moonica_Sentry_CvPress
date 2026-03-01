#include "PathManager.h"

QStringList PathManager::_imageExtensions = { "*.png", "*.jpg", "*.jpeg", "*.bmp" };
QString PathManager::_curProject = "";

QString PathManager::_rootPath = "";
QString PathManager::_projectListDir = "";
QString PathManager::_projectRootDir = "";
QString PathManager::_appSettingPath = "";
QString PathManager::_humanTrackingModelDir = "";
QString PathManager::_objectDetectionModelDir = "";
QString PathManager::_featureExtractionModelDir = "";

QString PathManager::_camViewsDir = "";
QString PathManager::_camViewsInfoDir = "";
QString PathManager::_camViewsMatrixDir = "";
QString PathManager::_floorViewsDir = "";
QString PathManager::_recordedVideoDir = "";

QString PathManager::_dbUserAccountPath = "";

void PathManager::setProject(QString project)
{
	_curProject = project;
	setAllPath();
}


void PathManager::setAllPath()
{
	_rootPath = "C:/Moonica/";
	_appSettingPath = _rootPath+ "appSetting.json";
    _dbUserAccountPath = _rootPath + "userDatabase.db";
	_projectListDir = _rootPath + "Projects/";

	if (_curProject.isEmpty()) return;
	_projectRootDir = _projectListDir + _curProject + "/";

	_camViewsDir = _projectRootDir + "CamViews/";
	_camViewsInfoDir = _projectRootDir + "CamViewsInfo/";
	_camViewsMatrixDir = _projectRootDir + "CamViewsMatrix/";
	_floorViewsDir = _projectRootDir + "FloorView/";
    _recordedVideoDir = _projectRootDir + "RecordedVideos/";

    _humanTrackingModelDir = _rootPath + "/AiModels/HumanTracking/";
    _objectDetectionModelDir = _rootPath + "/AiModels/ObjectDetection/";
    _featureExtractionModelDir = _rootPath + "/AiModels/FeatureExtraction/";
	
	makeDir(_camViewsDir);
	makeDir(_camViewsInfoDir);
	makeDir(_camViewsMatrixDir);
	makeDir(_floorViewsDir);
    makeDir(_recordedVideoDir);

    makeDir(_humanTrackingModelDir);
    makeDir(_objectDetectionModelDir);
    makeDir(_featureExtractionModelDir);
}

void PathManager::makeDir(QString& dirPath)
{
	QDir dir(dirPath);
	if (!dir.exists())
		dir.mkpath(dirPath);
}

bool PathManager::clearDirectory(const QString& path)
{
    QDir dir(path);

    if (!dir.exists()) {
        qDebug() << "Directory does not exist:" << path;
        return false;
    }

    // Remove all files
    for (const QFileInfo& fileInfo : dir.entryInfoList(QDir::Files)) {
        if (!QFile::remove(fileInfo.absoluteFilePath())) {
            qDebug() << "Failed to remove file:" << fileInfo.absoluteFilePath();
            return false;
        }
    }

    // Remove all subdirectories recursively
    for (const QFileInfo& subDirInfo : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QDir subDir(subDirInfo.absoluteFilePath());
        if (!subDir.removeRecursively()) {
            qDebug() << "Failed to remove folder:" << subDirInfo.absoluteFilePath();
            return false;
        }
    }

    return true;
}
