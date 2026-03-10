#pragma once

#include <QObject>
#include <QThread>
#include <QDebug>
#include <QProcess>
#include <QImage>
#include <QRegularExpression>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QQueue>
#include <QUuid>


#include "OnnxInference.h"
#include "CommonStruct.h"
#include "TrackingStruct.h"

#include "Moonica.h";

class TrackingManager : public QObject
{
	Q_OBJECT
public:
	TrackingManager();
	~TrackingManager();

	
	void addActiveCamera(QString camId, cv::Mat homographyMatrix, 
		QVector<QPointF> doorPoints, QVector<QPointF> perspectivePoints,
		double imageWidth, double imageHeight, bool isDoorCam);
	void removeActiveCamera(QString camId);
	void forceSetTotalFLoorPlanObject(int totalFloorPlanObject, bool debugMode);
	
	void forceSetTowerLightColor(TowerLightColor twColor);

    void setCriteria(CheckingCriteria checkingCriteria);


	
    void runCvPressCleaningCheck();
	TowerLightColor indentifyTowerLightColor(const cv::Mat& frame, const QPolygonF& lightRoi);

private:

	Moonica _moonica;

	const QString _towerLightCam = "towerLightCam";
	const QString _cleaningCam = "handCam";

	bool _triggerStart = false;

	

	
	int _totalObjectInFloorPlan = 0;
	bool _debugMode = false;


	bool _isProceesingGlobalId = false;
	QStringList _activeCameraList;
	QHash<QString, QVector<QPointF>> _doorPointHash; // key: camId;
	QHash<QString, cv::Mat>  _homographyMatrixHash; // key: camId;

	QHash<QString, std::vector<OnnxResult>> _localTrackingResult; // key: camId
	QHash<QString, std::vector<OnnxResult>> _localOdResult; // key: camId
	QHash<QString, cv::Mat> _camFrame; // key: camId

	

    CheckingCriteria _checkingCriteria;
	void runMoonicaApi();
	void setDebugMode(bool debugMode);
	QHash<QString, SingleViewParentObject> _sViewParentObjHash;

	// cleaning 
	TowerLightColor _forceSetTowerLightColor = TowerLightColor::OFF;
	bool _forceTriggerCleaning = false;
	QVector<ParentObject> _trackingResult;
	CleaningResult _cleaningResult;
	
	
	//int sameColorStateFrameNum = 0;
	TowerLightStatus _towerLightStatus;

	TowerLightColor _prevTowerLightColor = TowerLightColor::OFF;
	QVector<TowerLightColor> _last4TowerLightColor;

	QElapsedTimer _cleaningTimer;
	QElapsedTimer _alarmToCleaningTimer;
	bool _cleaningRunning = false;
	bool _hasStartCleaning = false;

public slots:
	void onResultReady(
		QString camId, 
		const cv::Mat& frame, 
		const std::vector<OnnxResult>& poseEstimationResult ,
		const std::vector<OnnxResult>& odResult
	);


signals:
	void updateGlobalCoordinate(const QVector<ParentObject>& trackingObjects, int numberOfPeople);
	void updateSingleViewResult(
		TowerLightColor towerLightColor,
		const CleaningResult& cleaningResult);


};


