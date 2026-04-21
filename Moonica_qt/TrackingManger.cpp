#include "TrackingManager.h"

TrackingManager::TrackingManager()
{

}

TrackingManager::~TrackingManager()
{

}

void TrackingManager::setDebugMode(bool debugMode)
{
	_debugMode = debugMode;
}

void TrackingManager::setCriteria(CheckingCriteria checkingCriteria)
{
	_checkingCriteria = checkingCriteria;
}

void TrackingManager::forceSetTotalFLoorPlanObject(int totalFloorPlanObject, bool debugMode)
{
	_totalObjectInFloorPlan = totalFloorPlanObject;
	_trackingResult.clear();
	_forceTriggerCleaning = true;
	setDebugMode(debugMode);



	_moonica.forceSetTotalParentObject(totalFloorPlanObject, debugMode);

	//_activeCameraList.append("Front");
	//_activeCameraList.append("Back");
}


void TrackingManager::addActiveCamera(QString camId, cv::Mat homographyMatrix, QVector<QPointF> doorPoints,
	QVector<QPointF> perspectivePoint, double imageWidth, double imageHeight, bool isDoorCam)
{
	if (!_activeCameraList.contains(camId))
	{
		_activeCameraList.append(camId);
		_doorPointHash[camId] = doorPoints;
		_moonica.addActiveCamera(camId, homographyMatrix, doorPoints, perspectivePoint, imageWidth, imageHeight, isDoorCam);
	}
}

void TrackingManager::removeActiveCamera(QString camId)
{
	if (_activeCameraList.contains(camId))
	{
		_activeCameraList.removeAt(_activeCameraList.indexOf(camId));

		_moonica.removeActiveCamera(camId);
	}
}


void TrackingManager::onResultReady(
	QString camId,
	const cv::Mat& frame,
	const std::vector<OnnxResult>& peResult, const std::vector<OnnxResult>& odResult)
{

	// tower light no need do OD
	_localTrackingResult[camId] = peResult;  // overwrite old frame
	_localOdResult[camId] = odResult;


	_camFrame[camId] = frame;

	if (_localTrackingResult.size() == _activeCameraList.size() && !_isProceesingGlobalId)
	{
		runCvPressCleaningCheck();

	}

}

void TrackingManager::runMoonicaApi()
{
	_isProceesingGlobalId = true;


	_moonica.runMcmoTracking(_localTrackingResult, _localOdResult);
	_trackingResult = _moonica.getTrackingResult();

	emit updateGlobalCoordinate(_trackingResult, _trackingResult.size());

	_isProceesingGlobalId = false;
	_forceTriggerCleaning = false;
}

void TrackingManager::forceSetTowerLightColor(TowerLightColor twColor)
{
	_forceSetTowerLightColor = twColor;
}


//void TrackingManager::runCvPressCleaningCheck()
//{
//	_isProceesingGlobalId = true;
//
//	// work do here
//
//    TowerLightColor towerLightColor = TowerLightColor::OFF;
//	// 1. Detect Yellow light for 50 frames lets say
//    if (_doorPointHash.contains(_towerLightCam) && _camFrame.contains(_towerLightCam))
//    {
//     
//        QPolygonF lightRoi;
//        for (auto d : _doorPointHash[_towerLightCam])
//        {
//            lightRoi << d;
//        }
//      
//        towerLightColor = indentifyTowerLightColor(_camFrame[_towerLightCam], lightRoi);   
//
//        if (_debugMode)
//        { 
//            qDebug() << "FORCE SET LIGHT TO: " << _forceSetTowerLightColor;
//            towerLightColor = _forceSetTowerLightColor;
//       
//        }
//    }
// 
//   
//
//    if (_prevTowerLightColor == towerLightColor)
//    {
//        sameColorStateFrameNum++;
//    }
//    else
//    {
//        sameColorStateFrameNum = 0;
//
//    }
//    _prevTowerLightColor = towerLightColor;
//  
//
//    if (_towerLightStatus.color != towerLightColor )
//    {
//        if (sameColorStateFrameNum > 50)
//        {
//
//            if (_towerLightStatus.color == TowerLightColor::GREEN_COLOR &&
//                towerLightColor == TowerLightColor::YELLOW_COLOR)
//            {
//                qDebug() << "!! cleaning kick started";
//
//                _startCleaningProcess = true;
//
//                _cleaningTimer.start();
//                _cleaningRunning = true;
//            }
//            else if (_towerLightStatus.color == TowerLightColor::YELLOW_COLOR &&
//                towerLightColor == TowerLightColor::GREEN_COLOR)
//            {
//                qDebug() << "!! cleaning kick end";
//
//                _startCleaningProcess = false;
//
//                if (_cleaningRunning)
//                {
//                    qint64 durationMs = _cleaningTimer.elapsed();
//
//                    double durationSec = durationMs / 1000.0;
//
//                    qDebug() << "Cleaning duration:" << durationSec << "seconds";
//
//                    _cleaningRunning = false;
//                }
//            }
//
//            qDebug() << "Trigger change color:" << towerLightColor;
//
//            _towerLightStatus.frameNum = 0;
//            _towerLightStatus.color = towerLightColor;
//        }
//    }
//    else
//    {
//        _towerLightStatus.frameNum++;
//    }
//
//   
//	
//
//	emit updateSingleViewResult(towerLightColor, _cleaningResult);
//	_isProceesingGlobalId = false;
//}


void TrackingManager::runCvPressCleaningCheck()
{
	_isProceesingGlobalId = true;
	_cleaningResult.isCompleteCleaning = false;
	// work do here

	TowerLightColor towerLightColor = TowerLightColor::OFF;
	// 1. Detect Yellow light for 50 frames lets say
	if (_doorPointHash.contains(_towerLightCam) && _camFrame.contains(_towerLightCam))
	{

		QPolygonF lightRoi;
		for (auto d : _doorPointHash[_towerLightCam])
		{
			lightRoi << d;
		}

		towerLightColor = indentifyTowerLightColor(_camFrame[_towerLightCam], lightRoi);

		if (_debugMode)
		{
			qDebug() << "FORCE SET LIGHT TO: " << _forceSetTowerLightColor;
			towerLightColor = _forceSetTowerLightColor;

		}
	}

	if (_prevTowerLightColor != towerLightColor)
	{
		_prevTowerLightColor = towerLightColor;
		_last4TowerLightColor.push_back(towerLightColor);

		if (_last4TowerLightColor.size() > 4)
			_last4TowerLightColor.removeFirst();
	}


	if (_last4TowerLightColor.size() == 4)
	{
		if (
			(_last4TowerLightColor[0] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[1] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[2] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[3] == TowerLightColor::YELLOW_COLOR)
			||
			(_last4TowerLightColor[0] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[1] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[2] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[3] == TowerLightColor::RED_COLOR
				)
			)
		{
			// start clening
			if (!_cleaningRunning)
			{
				_cleaningRunning = true;
				
				_alarmToCleaningTimer.start();

				qDebug() << "START CLEANING";
				CleaningResult cResult;
				cResult.id = QUuid::createUuid().toString();
				cResult.cleaningStatus = CleaningStatus::CLEANING_START;
				cResult.towerTriggeringTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");


				_cleaningResult = cResult;
			}

		}
		else if (
			(_last4TowerLightColor[0] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[1] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[2] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[3] == TowerLightColor::GREEN_COLOR)
			||
			(_last4TowerLightColor[0] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[1] == TowerLightColor::RED_COLOR &&
				_last4TowerLightColor[2] == TowerLightColor::YELLOW_COLOR &&
				_last4TowerLightColor[3] == TowerLightColor::GREEN_COLOR
				)
			)
		{
			// end cleaning


			if (_cleaningRunning)
			{
				
				_hasStartCleaning = false;
				_cleaningRunning = false;

				_cleaningResult.cleaningStatus = CleaningStatus::CLEANING_END;
				_cleaningResult.isCompleteCleaning = true;
			

				

				_cleaningResult.totalDuration =
					_cleaningResult.triggerToStartCleaningDuration +
					_cleaningResult.cleaningDuration;
			}
		}
	}

	if (_hasStartCleaning)
	{
		qint64 durationMs = 0;
		if (_cleaningTimer.isValid())
		{
			durationMs = _cleaningTimer.elapsed();
			double durationSec = durationMs / 1000.0;
			_cleaningResult.cleaningDuration = durationSec;
		}
	}
	if (_cleaningRunning && !_hasStartCleaning)
	{
		qint64 durationMs = 0;
		if (_alarmToCleaningTimer.isValid())
		{
			durationMs = _alarmToCleaningTimer.elapsed();
			double durationSec = durationMs / 1000.0;
			_cleaningResult.triggerToStartCleaningDuration = durationSec;
		}
	}

	if (_cleaningRunning 
		&& _doorPointHash.contains(_cleaningCam)
		&& _camFrame.contains(_cleaningCam)
		&& _localOdResult.contains(_cleaningCam))
	{
		QPolygonF cleaningRoi;
		for (auto d : _doorPointHash[_cleaningCam])
		{
			cleaningRoi << d;
		}

		bool isCleaning = false;
		for (auto& oR : _localOdResult[_cleaningCam])
		{
			QPointF oRPoint = QPointF(oR.x1, (oR.y1 + oR.y2) / 2.0);

			if (cleaningRoi.containsPoint(oRPoint, Qt::OddEvenFill))
			{
				if (!_hasStartCleaning)
				{
					_cleaningResult.cleaningTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
					_hasStartCleaning = true;
					_cleaningTimer.start();

					
				}
			
				isCleaning = true;
				
				
				break; // already found one inside
			}

		}
		_cleaningResult.isCleaning = isCleaning;
	}

	if (_hasStartCleaning && _localOdResult.contains(_cleaningCam))
	{
		QPolygonF cleaningRoi;
		for (auto d : _doorPointHash[_cleaningCam])
		{
			cleaningRoi << d;
		}
		for (auto& oR : _localOdResult[_cleaningCam])
		{
			QPointF oRPoint = QPointF(oR.x1, (oR.y1 + oR.y2) / 2.0);
			if (cleaningRoi.containsPoint(oRPoint, Qt::OddEvenFill))
			{
				if (oR.obj_id == 1)
				{
					_cleaningResult.hasBlower = true;
				}
				else if (oR.obj_id == 2)
				{
					_cleaningResult.hasCloth = true;
				}
			}
		}
	}

	emit updateSingleViewResult(towerLightColor, _cleaningResult);

	/*if (!_cleaningRunning)
	{
		_cleaningResult = CleaningResult();
	}*/

	_isProceesingGlobalId = false;
}

TowerLightColor TrackingManager::indentifyTowerLightColor(const cv::Mat& frame, const QPolygonF& lightRoi)
{
	// tower light position is within the light ROi
	// this function indentify whether the color is 
	// 1. Red
	// 2. Orange/Yellow
	// 3. Green

	if (frame.empty() || lightRoi.size() < 3)
		return TowerLightColor::OFF;

	// Support both BGR and grayscale safety
	cv::Mat bgrFrame;
	if (frame.channels() == 3)
	{
		bgrFrame = frame;
	}
	else if (frame.channels() == 4)
	{
		cv::cvtColor(frame, bgrFrame, cv::COLOR_BGRA2BGR);
	}
	else if (frame.channels() == 1)
	{
		cv::cvtColor(frame, bgrFrame, cv::COLOR_GRAY2BGR);
	}
	else
	{
		return TowerLightColor::OFF;
	}

	// Create ROI mask from polygon
	cv::Mat mask = cv::Mat::zeros(bgrFrame.size(), CV_8UC1);

	std::vector<cv::Point> pts;
	pts.reserve(lightRoi.size());
	for (int i = 0; i < lightRoi.size(); ++i)
	{
		QPointF p = lightRoi[i];
		pts.emplace_back(static_cast<int>(std::round(p.x())),
			static_cast<int>(std::round(p.y())));
	}

	std::vector<std::vector<cv::Point>> contours = { pts };
	cv::fillPoly(mask, contours, cv::Scalar(255));

	// Optional: reduce noise near polygon border
	cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 1);

	// Convert to HSV
	cv::Mat hsv;
	cv::cvtColor(bgrFrame, hsv, cv::COLOR_BGR2HSV);

	// HSV thresholds
	// Note:
	// Red wraps around hue, so use 2 ranges.
	cv::Mat redMask1, redMask2, redMask;
	cv::Mat yellowMask, greenMask;

	cv::inRange(hsv, cv::Scalar(0, 80, 80), cv::Scalar(10, 255, 255), redMask1);
	cv::inRange(hsv, cv::Scalar(170, 80, 80), cv::Scalar(180, 255, 255), redMask2);
	cv::bitwise_or(redMask1, redMask2, redMask);

	cv::inRange(hsv, cv::Scalar(15, 80, 80), cv::Scalar(40, 255, 255), yellowMask);
	cv::inRange(hsv, cv::Scalar(40, 80, 80), cv::Scalar(90, 255, 255), greenMask);

	// Keep only pixels inside polygon ROI
	cv::bitwise_and(redMask, mask, redMask);
	cv::bitwise_and(yellowMask, mask, yellowMask);
	cv::bitwise_and(greenMask, mask, greenMask);

	int redCount = cv::countNonZero(redMask);
	int yellowCount = cv::countNonZero(yellowMask);
	int greenCount = cv::countNonZero(greenMask);
	int totalCount = cv::countNonZero(mask);

	if (totalCount <= 0)
		return TowerLightColor::OFF;

	// Ratio against ROI area
	double redRatio = static_cast<double>(redCount) / totalCount;
	double yellowRatio = static_cast<double>(yellowCount) / totalCount;
	double greenRatio = static_cast<double>(greenCount) / totalCount;

	// Choose dominant color only if enough pixels are present
	const double minRatio = 0.08; // tune this

	double maxRatio = std::max({ redRatio, yellowRatio, greenRatio });

	if (maxRatio < minRatio)
		return TowerLightColor::OFF;

	if (redRatio >= yellowRatio && redRatio >= greenRatio)
		return TowerLightColor::RED_COLOR;

	if (yellowRatio >= redRatio && yellowRatio >= greenRatio)
		return TowerLightColor::YELLOW_COLOR;

	return TowerLightColor::GREEN_COLOR;
}



