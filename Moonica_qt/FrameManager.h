#pragma once

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QImage>
#include <QRegularExpression>
#include <opencv2/opencv.hpp>
#include <QGraphicsPixmapItem>
#include <QPixmap>
#include <QTimer>
#include <QQueue>
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

#include "OnnxInference.h"
#include "CommonStruct.h"
#include "PathManager.h"

class FrameManager : public QObject
{
	Q_OBJECT
public:
	FrameManager();
	~FrameManager();
	QTimer* _processTimer;
	Onnx::InferenceEngine* _trackingInference;
	Onnx::InferenceEngine* _detectionInference;
	
	void run();
	void addCameraStream(QString cameraID, QString url);
	void removeCameraStream(QString cameraID);

	void activateCamera(QString cameraID);
	void deActivateCamera(QString cameraID);

	QStringList getActiveCameraIds();
	
	void clearAllCameraStreams();

	void setAiSetting(ProjectSetting);
	void processLatestFrames();

private:
	QStringList _doorCam = { "Door", "Walkway" };
	double _currentSec;
	ProjectSetting _aiSetting;

	QTimer* _frameTimer;

	QHash<QString, CameraStream*> _cameraStreamHash; // key cameraId
	QMutex _frameQueueMutex;
	QQueue<QPair<QString, cv::Mat>> _frameQueue; // camId + frame
	QHash<QString, cv::Mat> _latestFrames;
	//Onnx::InferenceEngine _onnxInference;

	QHash<QString, Onnx::InferenceEngine* >_onnxInferenceHash;

	QHash<QString, Onnx::InferenceEngine* >_od_onnxInferenceHash;

	int _activeCameraNum;
	bool _isProcessingQueue = false;

	bool _hasPoseModel = false;
	bool _hasDetectionModel = false;

public slots:

	void processFrameQueue(QHash<QString, cv::Mat> localFrames, double curSecond);
	void onFrameReady(const cv::Mat& frame, QString camId, double currentSec);


signals:
	void updateCameraGraphicView(QString cameraId,
		QImage frame, std::vector<OnnxResult> poseEstimation,std::vector<OnnxResult> od, double currentSec);
	void frameReadyRecord(const cv::Mat& frame, QString);
	void frameReadyTracking(
		QString camId, const cv::Mat& frame,
		const std::vector<OnnxResult>& poseEstimationResult,
		const std::vector<OnnxResult>& odResult
	);

};