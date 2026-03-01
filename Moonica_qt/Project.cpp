#include "Project.h"


void Project::resetProject()
{
	_projectId.clear();
	_projectName.clear();
	_viewInfoHash.clear();
	_floorViewInfo = ViewInfo();
	_settings = ProjectSetting();
}

bool Project::loadProject(QString recipeName)
{
	resetProject();
	
	PathManager::setProject(recipeName);
	_projectId = recipeName;
	_projectName = recipeName;
	bool loadImageInfo = loadViewImages();
	qDebug() << "Load View Images:" << (loadImageInfo ? "Success" : "Failed");

	bool loadMatrix = loadHomographyMatrix();
	qDebug() << "Load Homography Matrix:" << (loadMatrix ? "Success" : "Failed");

	bool loadFloorInfo = loadFloorImages();
	qDebug() << "Load Floor Image:" << (loadFloorInfo ? "Success" : "Failed");

	bool loadPSettingInfo = loadProjectSetting();

	return loadImageInfo && loadMatrix && loadFloorInfo;
}

bool Project::saveProject()
{
	saveHomographyMatrix();
	saveViewImages();
	saveFloorImages();
	saveProjectSetting();

	return true;
}

bool Project::saveHomographyMatrix()
{
	if (_viewInfoHash.isEmpty()) {
		qWarning() << "No View Info hamography to save.";
		return false;
	}

	QDir dir(PathManager::_camViewsMatrixDir);
	if (!dir.exists()) {
		dir.mkpath(".");
	}

	for (auto it = _viewInfoHash.constBegin(); it != _viewInfoHash.constEnd(); ++it)
	{
		QString viewName = it.key();
		ViewInfo vInfo = it.value();

		// Create filename: key.yaml
		QString filePath = dir.filePath(viewName + ".yaml");

		// Use OpenCV FileStorage to write the matrix
		cv::FileStorage fs(filePath.toStdString(), cv::FileStorage::WRITE);
		if (!fs.isOpened()) {
			qWarning() << "Cannot open file for writing:" << filePath;
			return false;
		}

		fs << "H" << vInfo.homographyMatrix;
		fs.release();
	}

	return true;
}

bool Project::loadHomographyMatrix()
{
	QDir dir(PathManager::_camViewsMatrixDir);
	if (!dir.exists()) {
		qWarning() << "Directory does not exist:" << PathManager::_camViewsMatrixDir;
		return false;
	}

	QStringList yamlFiles = dir.entryList(QStringList() << "*.yaml", QDir::Files);
	if (yamlFiles.isEmpty()) {
		qWarning() << "No YAML files found in directory:" << PathManager::_camViewsMatrixDir;
		return false;
	}



	for (const QString& fileName : yamlFiles) {
		QString viewName = QFileInfo(fileName).baseName();  // filename without extension
		QString fullPath = dir.filePath(fileName);

		cv::FileStorage fs(fullPath.toStdString(), cv::FileStorage::READ);
		if (!fs.isOpened()) {
			qWarning() << "Cannot open file:" << fullPath;
			continue;
		}

		cv::Mat H;
		fs["H"] >> H;
		fs.release();

		if (!H.empty())
		{
			_viewInfoHash[viewName].homographyMatrix = H;
		}

	}


	return !_viewInfoHash.isEmpty();
}

bool Project::loadViewImages()
{
	_viewInfoHash.clear();
	_viewList.clear();

	
	QString infoDir = PathManager::_camViewsInfoDir;
	QDir dir(infoDir);
	if (!dir.exists())
	{
		qWarning() << "View info directory does not exist:" << infoDir;
		return true;
	}


	//sort addded cctv by order
	QFileInfoList infoFiles = dir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Time);

	for (const QFileInfo& fileInfo : infoFiles)
	{
		QFile jsonFile(fileInfo.absoluteFilePath());

		if (!jsonFile.open(QIODevice::ReadOnly)) //check readable or not 
		{
			continue;
		};

		QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll());
		jsonFile.close();

		if (!doc.isObject())
		{
			qWarning() << "Invalid JSON in file:" << fileInfo.fileName();
			continue;
		}
		QJsonObject obj = doc.object();
		ViewInfo vInfo;
	
		// Populate the ViewInfo object from the JSON data
		vInfo.viewName = obj.value("viewName").toString();
		vInfo.cctvUrl = obj.value("cctvUrl").toString();
		vInfo.isCalibrated = obj.value("isCalibrated").toBool();
		vInfo.viewWidth = obj.value("viewWidth").toDouble();
		vInfo.viewHeight = obj.value("viewHeight").toDouble();
		vInfo.extension = obj.value("extension").toString();
		if (vInfo.extension.isEmpty())vInfo.extension = "png";
		vInfo.viewPath = PathManager::_camViewsDir + "/" + vInfo.viewName + "."+ vInfo.extension;
		qDebug() << "vInfo.viewPath: " << vInfo.viewPath;
		if (obj.contains("imagePoints") && obj["imagePoints"].isArray())
		{
			QJsonArray pts = obj["imagePoints"].toArray();
			for (const QJsonValue& val : pts)
			{
				if (val.isArray())
				{
					QJsonArray point = val.toArray();
					if (point.size() == 2)
						vInfo.imagePointVec.append(QPointF(point[0].toDouble(), point[1].toDouble()));
				}
			}
		}

		if (obj.contains("floorPoints") && obj["floorPoints"].isArray())
		{
			QJsonArray pts = obj["floorPoints"].toArray();
			for (const QJsonValue& val : pts)
			{
				if (val.isArray()) {
					QJsonArray point = val.toArray();
					if (point.size() == 2)
						vInfo.floorPointVec.append(QPointF(point[0].toDouble(), point[1].toDouble()));
				}
			}
		}

		if (obj.contains("doorPoints") && obj["doorPoints"].isArray())
		{
			QJsonArray pts = obj["doorPoints"].toArray();
			for (const QJsonValue& val : pts)
			{
				if (val.isArray()) {
					QJsonArray point = val.toArray();
					if (point.size() == 2)
						vInfo.doorPointVec.append(QPointF(point[0].toDouble(), point[1].toDouble()));
				}
			}
		}

		if (obj.contains("perspectivePoints") && obj["perspectivePoints"].isArray())
		{
			QJsonArray pts = obj["perspectivePoints"].toArray();
			for (const QJsonValue& val : pts)
			{
				if (val.isArray()) {
					QJsonArray point = val.toArray();
					if (point.size() == 2)
						vInfo.perspectivePointVec.append(QPointF(point[0].toDouble(), point[1].toDouble()));
				}
			}
		}

		//add fully loaded camera data to BOTH internal lists.
		if (!vInfo.viewName.isEmpty())
		{
			_viewInfoHash.insert(vInfo.viewName, vInfo); // Used by the Workspace tab.
			_viewList.append(vInfo);                   // Used by the Cam Test tab in the correct order.
		}
	}
	return true;
}
	


bool Project::saveViewImages()
{
	QDir().mkpath(PathManager::_camViewsInfoDir); // Ensure the directory exists

	for (const auto& viewName : _viewInfoHash.keys()) {
		const ViewInfo& vInfo = _viewInfoHash[viewName];

		QJsonObject rootObj;

		// Save simple members
		rootObj["isCalibrated"] = vInfo.isCalibrated;
		rootObj["viewName"] = vInfo.viewName;
		rootObj["viewPath"] = vInfo.viewPath;
		rootObj["cctvUrl"] = vInfo.cctvUrl;  //save cctv url for settings
		rootObj["viewWidth"] = vInfo.viewWidth;
		rootObj["viewHeight"] = vInfo.viewHeight;
		rootObj["extension"] = vInfo.extension;
		// Save imagePointVec as array of [x, y] pairs
		QJsonArray pointsArray;
		for (const QPointF& pt : vInfo.imagePointVec) {
			QJsonArray ptArray;
			ptArray.append(pt.x());
			ptArray.append(pt.y());
			pointsArray.append(ptArray);
		}
		rootObj["imagePoints"] = pointsArray;

		// Save imagePointVec as array of [x, y] pairs
		QJsonArray floorPointsArray;
		for (const QPointF& pt : vInfo.floorPointVec) {
			QJsonArray ptArray;
			ptArray.append(pt.x());
			ptArray.append(pt.y());
			floorPointsArray.append(ptArray);
		}
		rootObj["floorPoints"] = floorPointsArray;

		QJsonArray doorPointsArray;
		for (const QPointF& pt : vInfo.doorPointVec) {
			QJsonArray ptArray;
			ptArray.append(pt.x());
			ptArray.append(pt.y());
			doorPointsArray.append(ptArray);
		}
		rootObj["doorPoints"] = doorPointsArray;

		QJsonArray perspectivePointsArray;
		for (const QPointF& pt : vInfo.perspectivePointVec) {
			QJsonArray ptArray;
			ptArray.append(pt.x());
			ptArray.append(pt.y());
			perspectivePointsArray.append(ptArray);
		}
		rootObj["perspectivePoints"] = perspectivePointsArray;


		// Convert to JSON and write to file
		QJsonDocument doc(rootObj);
		QString jsonPath = PathManager::_camViewsInfoDir + "/" + viewName + ".json";
		QFile jsonFile(jsonPath);

		if (!jsonFile.open(QIODevice::WriteOnly)) {
			qWarning() << "Failed to save:" << jsonPath;
			continue;
		}

		jsonFile.write(doc.toJson(QJsonDocument::Indented));
		jsonFile.close();
	}

	return true;
}

bool Project::loadFloorImages()
{

	QString imagesDir = PathManager::_floorViewsDir;

	QDir dir(imagesDir);
	QStringList nameFilters = { "*.png", "*.jpg", "*.jpeg", "*.bmp", "*.tiff" };
	QStringList imageFiles = dir.entryList(nameFilters, QDir::Files);

	for (const QString& fileName : imageFiles) {
		QString fullPath = dir.absoluteFilePath(fileName);
		QString baseName = QFileInfo(fileName).completeBaseName();
		QString viewInfoPath = PathManager::_floorViewsDir + "/" + baseName + ".json";

		ViewInfo vInfo;
		vInfo.viewPath = fullPath;
		vInfo.viewName = baseName;
		vInfo.isCalibrated = false; // Default value

		QFile jsonFile(viewInfoPath);
		if (jsonFile.exists() && jsonFile.open(QIODevice::ReadOnly)) {
			QByteArray data = jsonFile.readAll();
			QJsonDocument doc = QJsonDocument::fromJson(data);

			if (doc.isObject()) {
				QJsonObject obj = doc.object();

				if (obj.contains("isCalibrated"))  vInfo.isCalibrated = obj["isCalibrated"].toBool();
				if (obj.contains("viewName"))  vInfo.viewName = obj["viewName"].toString();
				if (obj.contains("extension"))  vInfo.extension = obj["extension"].toString();
				if (vInfo.extension.isEmpty()) vInfo.extension = "png";
				//if (obj.contains("viewPath"))  vInfo.viewPath = obj["viewPath"].toString();
				vInfo.viewPath = PathManager::_floorViewsDir + "/" + vInfo.viewName + "." + vInfo.extension;



			}

			jsonFile.close();
		}
		_floorViewInfo = vInfo;

	}

	return true;
}


bool Project::saveFloorImages()
{
	QDir().mkpath(PathManager::_floorViewsDir); // Ensure the directory exists


	const ViewInfo& vInfo = _floorViewInfo;

	QJsonObject rootObj;

	// Save simple members
	rootObj["isCalibrated"] = vInfo.isCalibrated;
	rootObj["viewName"] = vInfo.viewName;
	rootObj["viewPath"] = vInfo.viewPath;
	rootObj["extension"] = vInfo.extension;


	// Convert to JSON and write to file
	QJsonDocument doc(rootObj);
	QString jsonPath = PathManager::_floorViewsDir + "/FloorPlan.json";
	QFile jsonFile(jsonPath);

	if (!jsonFile.open(QIODevice::WriteOnly)) {
		qWarning() << "Failed to save:" << jsonPath;

	}

	jsonFile.write(doc.toJson(QJsonDocument::Indented));
	jsonFile.close();


	return true;
}


bool Project::saveProjectSetting()
{
	QJsonObject rootObj;
	
	// Save simple members
	rootObj["humanTracking_modelName"] = _settings.humanTracking_modelName;
	rootObj["humanTracking_accuracy"] = _settings.humanTracking_accuracy;
	rootObj["humanTracking_enableFeatureExtraction"] = _settings.humanTracking_enableFeatureExtraction;
	rootObj["objectDetection_modelName"] = _settings.objectDetection_modelName;
	rootObj["objectDetection_accuracy"] = _settings.objectDetection_accuracy;
	rootObj["is_global_view_checking"] = _settings.isGlobalViewChecking;

	rootObj["selected_camera_list"] =
		QJsonArray::fromStringList(_settings.viewSequenceList);

	rootObj["view_row"] = _settings.viewRow;
	rootObj["view_col"] = _settings.viewCol;
	
	
	// Convert to JSON and write to file
	QJsonDocument doc(rootObj);
	QString jsonPath = PathManager::_projectRootDir + "/ProjectSettings.json";
	QFile jsonFile(jsonPath);

	if (!jsonFile.open(QIODevice::WriteOnly)) {
		qWarning() << "Failed to save:" << jsonPath;

	}
	
	jsonFile.write(doc.toJson(QJsonDocument::Indented));
	jsonFile.close();


	qDebug() << "saveProjectSetting: " << jsonPath;

	return true;
}

bool Project::loadProjectSetting()
{
	_settings.viewSequenceList.clear();

	QString jsonPath = PathManager::_projectRootDir + "/ProjectSettings.json";
	QFile jsonFile(jsonPath);

	if (!jsonFile.open(QIODevice::ReadOnly))
	{
		qWarning() << "Failed to load:" << jsonPath;
		return false;
	}

	QByteArray jsonData = jsonFile.readAll();
	jsonFile.close();

	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

	if (parseError.error != QJsonParseError::NoError)
	{
		qWarning() << "JSON parse error:" << parseError.errorString();
		return false;
	}

	if (!doc.isObject())
		return false;

	QJsonObject rootObj = doc.object();

	// ---- Load AI settings (with defaults for safety) ----
	_settings.humanTracking_modelName =
		rootObj.value("humanTracking_modelName").toString();

	_settings.humanTracking_accuracy =
		rootObj.value("humanTracking_accuracy").toDouble();

	_settings.humanTracking_enableFeatureExtraction =
		rootObj.value("humanTracking_enableFeatureExtraction").toBool();

	_settings.objectDetection_modelName =
		rootObj.value("objectDetection_modelName").toString();

	_settings.objectDetection_accuracy =
		rootObj.value("objectDetection_accuracy").toDouble();

	_settings.isGlobalViewChecking =
		rootObj.value("is_global_view_checking").toBool();

	_settings.viewRow =
		rootObj.value("view_row").toInt(1);

	_settings.viewCol =
		rootObj.value("view_col").toInt(1);

	_settings.viewSequenceList =
		rootObj["selected_camera_list"].toVariant().toStringList();


	if (_settings.viewCol < 1)_settings.viewCol = 1;
	if (_settings.viewRow < 1)_settings.viewRow = 1;
	if (_settings.viewSequenceList.isEmpty())
	{
		for (auto& vH : _viewInfoHash)
		{
			_settings.viewSequenceList.append(vH.viewName);
		}
	}

	return true;
}