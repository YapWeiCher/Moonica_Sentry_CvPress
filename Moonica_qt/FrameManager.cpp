#include "FrameManager.h"


FrameManager::~FrameManager()
{
	clearAllCameraStreams();
}

void FrameManager::clearAllCameraStreams()
{
	// ---- CameraStream ----
	for (auto it = _cameraStreamHash.begin(); it != _cameraStreamHash.end(); ++it)
	{
		delete it.value();
	}
	_cameraStreamHash.clear();

	// ---- Pose / Tracking ONNX ----
	for (auto it = _onnxInferenceHash.begin(); it != _onnxInferenceHash.end(); ++it)
	{
		delete it.value();
	}
	_onnxInferenceHash.clear();

	// ---- OD ONNX ----
	for (auto it = _od_onnxInferenceHash.begin(); it != _od_onnxInferenceHash.end(); ++it)
	{
		delete it.value();
	}
	_od_onnxInferenceHash.clear();
}
void FrameManager::run()
{

}

void FrameManager::setAiSetting(ProjectSetting aiSetting)
{
	_aiSetting = aiSetting;
}

FrameManager::FrameManager()
{
	_processTimer = new QTimer(this);
	connect(_processTimer, &QTimer::timeout, this, &FrameManager::processLatestFrames);
	_processTimer->start(33); // ~30fps (or 66 for 15fps)
}

void FrameManager::onFrameReady(const cv::Mat& frame, QString camId, double currentSec)
{
	//qDebug() << "onFrameReady";
	//QMutexLocker locker(&_frameQueueMutex);
	////qDebug() << "Cam: " << camId << " Frame Ready";
	//_latestFrames[camId] = frame.clone();  // overwrite old frame

	//if (_latestFrames.size() == _activeCameraNum && !_isProcessingQueue)
	//{
	//	processFrameQueue();
	//	_currentSec = currentSec;
	//}
	// ---------
	{
		QMutexLocker locker(&_frameQueueMutex);
		_latestFrames[camId] = std::move(frame);
		_currentSec = currentSec;
	}
}
//

void FrameManager::processLatestFrames()
{
	QHash<QString, cv::Mat> framesCopy;
	double secCopy = 0.0;

	{
		QMutexLocker locker(&_frameQueueMutex);

		if (_latestFrames.size() != _activeCameraNum) return;

		// copy pointers/headers quickly; clone only if needed
		framesCopy = _latestFrames;
		secCopy = _currentSec;
	}

	// heavy work OUTSIDE mutex
	// render / record / ai inference etc using framesCopy
	processFrameQueue(framesCopy, secCopy);
}
void FrameManager::processFrameQueue(QHash<QString, cv::Mat> localFrames,double curSecond ) {
	
	_isProcessingQueue = true;
	//QHash<QString, cv::Mat> localFrames;

	//{
	//	// QMutexLocker locker(&_frameQueueMutex); // Uncomment if _latestFrames access needs locking
	//	if (_latestFrames.isEmpty())
	//		return;
	//	localFrames = _latestFrames;
	//	_latestFrames.clear();
	//}

	// Create a QFutureSynchronizer to wait for all concurrent tasks to finish
	QFutureSynchronizer<void> synchronizer;

	for (auto it = localFrames.begin(); it != localFrames.end(); ++it) {
		QString camId = it.key();
		cv::Mat frame = it.value().clone();

		// Add the future to the synchronizer
		synchronizer.addFuture(QtConcurrent::run([=, frame = std::move(frame)]() mutable { // Pass frame by value to ensure it's copied for each thread
			// This code block runs in parallel for each camera.
			std::vector<OnnxResult> trackingResult;
			std::vector<OnnxResult> odResult;

			std::vector<int> classIdsToTrack;
			classIdsToTrack.push_back(0);
			
			if (_hasPoseModel)
			{
				trackingResult = _onnxInferenceHash[camId]->ct_runModelTracking(frame, _aiSetting.humanTracking_accuracy, 0.3, classIdsToTrack);
				cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);
			}
			
			if (_hasDetectionModel)
			{
				odResult = _od_onnxInferenceHash[camId]->ct_runModel(frame, _aiSetting.objectDetection_accuracy, 0.5);
			}
			
			

			QImage img(frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);

			// This signal will be automatically and safely queued to the main thread
			// if the receiver lives there.
			emit updateCameraGraphicView(camId, img.copy(), trackingResult, odResult, _currentSec); // Use img.copy() to ensure data is copied and not tied to rgbFrame's lifetime
			emit frameReadyRecord(it.value().clone(), camId);
			emit frameReadyTracking(camId, it.value().clone(), trackingResult, odResult);
		}));
	}


	_isProcessingQueue = false;
}



void FrameManager::addCameraStream(QString cameraId, QString url)
{
	if (!_cameraStreamHash.contains(cameraId))
	{
		CameraStream* camStream = new CameraStream();
		camStream->camId = cameraId;
		camStream->url = url;

		_cameraStreamHash.insert(cameraId, camStream);

		QString modelPath = PathManager::_humanTrackingModelDir + _aiSetting.humanTracking_modelName;
		QFileInfo fileInfo(modelPath);

		if (!_aiSetting.humanTracking_modelName.isEmpty() &&
			fileInfo.exists() &&
			fileInfo.isFile())
		{
			_hasPoseModel = true;
			Onnx::InferenceEngine* onnxInfer = new Onnx::InferenceEngine(true);
			onnxInfer->ct_loadModel(modelPath.toStdString(), modelPath.toStdString());

			QString osNetPath = PathManager::_featureExtractionModelDir + "osnet_x1_0.onnx";

			qDebug() << "Feature Extraction Model Path: " << osNetPath;
			if (_aiSetting.humanTracking_enableFeatureExtraction)
			{
				//onnxInfer->ct_loadModel_osnet(osNetPath.toStdString(), osNetPath.toStdString(), 0.3);
			}

			onnxInfer->setTrackingSettings(
				30,
				30,
				0.5,
				0.6,
				0.8
			);

			_onnxInferenceHash.insert(cameraId, onnxInfer);
		}
		else
		{
			_hasPoseModel = false;
		}

		QString modelPath_od = PathManager::_objectDetectionModelDir + _aiSetting.objectDetection_modelName;
		
		QFileInfo fileInfo1(modelPath_od);

		if (!_aiSetting.objectDetection_modelName.isEmpty() &&
			fileInfo1.exists() &&
			fileInfo1.isFile())
		{
			_hasDetectionModel = true;

	
			Onnx::InferenceEngine* onnxInfer_od = new Onnx::InferenceEngine(true);
			onnxInfer_od->ct_loadModel(modelPath_od.toStdString(), modelPath_od.toStdString());
			
			_od_onnxInferenceHash.insert(cameraId, onnxInfer_od);
		
		}
		else
		{
			_hasDetectionModel = false;		
		}
	


		

		
		

		
		
	}
}

void FrameManager::activateCamera(QString cameraId)
{
	if (_cameraStreamHash.contains(cameraId))
	{
		_cameraStreamHash[cameraId]->isActive = true;
	}

	int activeNum = 0;
	for (auto& c : _cameraStreamHash)
	{
		if (c->isActive)activeNum++;
	}
	_activeCameraNum = activeNum;
}

void FrameManager::deActivateCamera(QString cameraId)
{
	if (_cameraStreamHash.contains(cameraId))
	{
		_cameraStreamHash[cameraId]->isActive = false;
	}

	int activeNum = 0;
	for (auto& c : _cameraStreamHash)
	{
		if (c->isActive)activeNum++;
	}
	_activeCameraNum = activeNum;
}

void FrameManager::removeCameraStream(QString cameraId)
{
	// First, check if the camera stream exists to avoid unnecessary work.
	if (_cameraStreamHash.contains(cameraId))
	{
		// --- 1. Clean up the Onnx::InferenceEngine ---

		// take() retrieves the value and removes the key-value pair from the hash.
		Onnx::InferenceEngine* onnxInfer = _onnxInferenceHash.take(cameraId);

		// Always check if the pointer is valid before deleting.
		if (onnxInfer) {
			delete onnxInfer;
			onnxInfer = nullptr; // Good practice to null the pointer after deleting.
		}


		// --- 2. Clean up the CameraStream struct ---

		CameraStream* camStream = _cameraStreamHash.take(cameraId);
		if (camStream) {
			delete camStream;
			camStream = nullptr;
		}

		qDebug() << "Successfully removed camera stream and cleaned up resources for:" << cameraId;
	}
}

QStringList FrameManager::getActiveCameraIds()
{
	QStringList activeCameraIds;
	for (auto& c : _cameraStreamHash)
	{
		if (c->isActive)activeCameraIds.append(c->camId);
	}

	return activeCameraIds;
}






