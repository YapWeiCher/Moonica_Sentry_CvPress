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
	_forceTrace = true;
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
	_forceTrace = false;
}




void TrackingManager::runCvPressCleaningCheck()
{
	_isProceesingGlobalId = true;

	// work do here

    QString towerLightColor = "off";
	// 1. Detect Yellow light for 50 frames lets say
    if (_doorPointHash.contains(_towerLightCam) && _camFrame.contains(_towerLightCam))
    {
     
        QPolygonF lightRoi;
        for (auto d : _doorPointHash[_towerLightCam])
        {
            lightRoi << d;
        }
      

        towerLightColor = indentifyTowerLightColor(_camFrame[_towerLightCam], lightRoi);

     
    }
 

	// 2. check trigger to operator start cleaning time

	// 3. once detect hand, trigger cleaning start time

	// 4. check if hand is inside cleaning area

	// 5. End Cleaning triggering

	emit updateSingleViewResult(towerLightColor, _cleaningResult);
	_isProceesingGlobalId = false;
}

QString TrackingManager::indentifyTowerLightColor(const cv::Mat& frame, const QPolygonF& lightRoi)
{
	// tower light position is within the light ROi
	// this function indentify whether the color is 
	// 1. Red
	// 2. Orange/Yellow
	// 3. Green

    if (frame.empty() || lightRoi.size() < 3)
        return "Unknown";

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
        return "Unknown";
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
        return "Unknown";

    // Ratio against ROI area
    double redRatio = static_cast<double>(redCount) / totalCount;
    double yellowRatio = static_cast<double>(yellowCount) / totalCount;
    double greenRatio = static_cast<double>(greenCount) / totalCount;

    // Choose dominant color only if enough pixels are present
    const double minRatio = 0.08; // tune this

    double maxRatio = std::max({ redRatio, yellowRatio, greenRatio });

    if (maxRatio < minRatio)
        return "Off";

    if (redRatio >= yellowRatio && redRatio >= greenRatio)
        return "Red";

    if (yellowRatio >= redRatio && yellowRatio >= greenRatio)
        return "Yellow";

    return "Green";
}



