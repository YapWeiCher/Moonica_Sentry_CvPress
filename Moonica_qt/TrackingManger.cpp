#include "TrackingManager.h"

float TrackingManager::computeAngle(const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c) {
	// angle at point b formed by (a-b-c)
	cv::Point2f ab = a - b;
	cv::Point2f cb = c - b;
	float dot = ab.x * cb.x + ab.y * cb.y;
	float norm_ab = std::sqrt(ab.x * ab.x + ab.y * ab.y);
	float norm_cb = std::sqrt(cb.x * cb.x + cb.y * cb.y);
	float cos_angle = dot / (norm_ab * norm_cb + 1e-6);
	return std::acos(std::clamp(cos_angle, -1.0f, 1.0f)) * 180.0 / CV_PI;
}
cv::Point2f TrackingManager::CorrectWristPoint(const OnnxResult& localBox, bool left) {
	int shoulder = left ? Keypoint::LEFT_SHOULDER : Keypoint::RIGHT_SHOULDER;
	int elbow = left ? Keypoint::LEFT_ELBOW : Keypoint::RIGHT_ELBOW;
	int wrist = left ? Keypoint::LEFT_WRIST : Keypoint::RIGHT_WRIST;

	auto pts = localBox.keypoints;

	// Step 1: upper arm length (shoulder–elbow)
	float upperArmLen = cv::norm(pts[shoulder] - pts[elbow]);

	// Step 2: expected forearm length (elbow–wrist)
	float expectedForearm = 0.9f * upperArmLen;  // tweak factor (0.9–1.1)

	// Step 3: actual forearm length
	float elbowToWrist = cv::norm(pts[elbow] - pts[wrist]);

	cv::Point2f correctedWrist = pts[wrist];

	// Step 4: if forearm too short, extend wrist away from elbow
	if (elbowToWrist < 0.9f * expectedForearm) {

	}
	cv::Point2f dir = pts[wrist] - pts[elbow]; // elbow -> wrist
	float scale = expectedForearm / (cv::norm(dir) + 1e-6f);
	correctedWrist = pts[elbow] + dir * scale;

	return correctedWrist;
}



void TrackingManager::transformSittingToStanding(OnnxResult& localBox) {

	auto& pts = localBox.keypoints;

	// Shoulders & hips
	cv::Point2f leftShould = pts[LEFT_SHOULDER];
	cv::Point2f rightShould = pts[RIGHT_SHOULDER];
	cv::Point2f leftHip = pts[LEFT_HIP];
	cv::Point2f rightHip = pts[RIGHT_HIP];

	// Knees & ankles
	cv::Point2f leftKnee = pts[LEFT_KNEE];
	cv::Point2f rightKnee = pts[RIGHT_KNEE];
	cv::Point2f leftAnkle = pts[LEFT_ANKLE];
	cv::Point2f rightAnkle = pts[RIGHT_ANKLE];

	// --- Torso direction (shoulder ? hip) ---
	cv::Point2f shoulderMid = 0.5f * (leftShould + rightShould);
	cv::Point2f hipMid = 0.5f * (leftHip + rightHip);

	cv::Point2f torsoDir = hipMid - shoulderMid;
	float len = cv::norm(torsoDir);
	if (len > 1e-5) {
		torsoDir *= (1.0f / len); // normalize
	}
	else {
		torsoDir = cv::Point2f(0, 1); // fallback: vertical
	}

	// --- Left side ---
	float leftLegLen = cv::norm(leftHip - leftKnee);
	float leftShinLen = cv::norm(leftKnee - leftAnkle);

	// knee = hip + torsoDir * legLength
	pts[LEFT_KNEE] = leftHip + torsoDir * leftLegLen;
	// ankle = knee + torsoDir * shinLength
	pts[LEFT_ANKLE] = pts[LEFT_KNEE] + torsoDir * leftShinLen;

	// --- Right side ---
	float rightLegLen = cv::norm(rightHip - rightKnee);
	float rightShinLen = cv::norm(rightKnee - rightAnkle);

	pts[RIGHT_KNEE] = rightHip + torsoDir * rightLegLen;
	pts[RIGHT_ANKLE] = pts[RIGHT_KNEE] + torsoDir * rightShinLen;
}


cv::Point2f TrackingManager::CorrectAnklePoint(const OnnxResult& localBox,

	bool left)
{

	int shoulder = left ? Keypoint::LEFT_SHOULDER : Keypoint::RIGHT_SHOULDER;
	int hip = left ? Keypoint::LEFT_HIP : Keypoint::RIGHT_HIP;
	int knee = left ? Keypoint::LEFT_KNEE : Keypoint::RIGHT_KNEE;
	int ankle = left ? Keypoint::LEFT_ANKLE : Keypoint::RIGHT_ANKLE;

	auto pts = localBox.keypoints;


	// Step 1: torso length (shoulder–hip)
	float torsoLen = cv::norm(pts[shoulder] - pts[hip]);

	// Step 2: expected shin length (knee–ankle)
	float expectedShin = 0.9f * torsoLen;

	// Step 3: actual shin length
	float kneeToAnkle = cv::norm(pts[knee] - pts[ankle]);

	cv::Point2f correctedAnkle = pts[ankle];

	// Step 4: correction logic
	if (kneeToAnkle < 0.5f * expectedShin) {
		correctedAnkle.x = pts[knee].x;
		correctedAnkle.y = pts[knee].y + expectedShin;
	}
	else {
		correctedAnkle.x = pts[knee].x;
		correctedAnkle.y = pts[knee].y + kneeToAnkle;
	}



	return correctedAnkle;
}

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


void TrackingManager::onResultReady(QString camId, const std::vector<OnnxResult>& peResult, const std::vector<OnnxResult>& odResult)
{

	_localTrackingResult[camId] = peResult;  // overwrite old frame
	_localOdResult[camId] = odResult;

	
	if (_localTrackingResult.size() == _activeCameraList.size() && !_isProceesingGlobalId)
	{
		runSingleViewChecking();
		runMoonicaApi();
		
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


void TrackingManager::runSingleViewChecking()
{
	_isProceesingGlobalId = true;

	for (auto it = _sViewParentObjHash.begin(); it != _sViewParentObjHash.end(); )
	{
		auto& sParent = it.value();

		sParent.isTracking = false;
		sParent.inCheckingArea = false;
		sParent.lostTrackFrame++;

		if (sParent.lostTrackFrame > 200)
		{
			it = _sViewParentObjHash.erase(it);  // erase returns next iterator
		}
		else
		{
			++it;
		}
	}

	for (auto& camId : _activeCameraList)
	{
		if (_localTrackingResult.contains(camId))
		{
			for (auto& tResult : _localTrackingResult[camId])
			{
				QString parentId = camId + "[@]" + QString::number(tResult.tracking_id);


				SingleViewParentObject& pObj = _sViewParentObjHash[parentId];
				pObj.camId = camId;
				pObj.globalId = parentId;
				pObj.isTracking = true;
				pObj.lostTrackFrame = 0;
				pObj.trackingResult = tResult;

				if (_localOdResult.contains(camId))
				{
					if (!pObj.hasFacemask)
					{
						if (checkHasMask(tResult, _localOdResult[camId], { 6 }))
						{
							pObj.hasFaceMaskFrame++;
						}
						if (pObj.hasFaceMaskFrame > _checkingCriteria.detectionFrameBuffer)
							pObj.hasFacemask = true;
					}
						
					if (!pObj.hasGlove)
					{
						if (checkHasGlove(tResult, _localOdResult[camId], { 5 }))
						{
							pObj.hasGloveFrame++;
						}
						if (pObj.hasGloveFrame > _checkingCriteria.detectionFrameBuffer)
							pObj.hasGlove = true;
					}
					
					if (!pObj.hasEsdShoe)
					{
						if (checkHasShoe(tResult, _localOdResult[camId], { 8, 10, 11 }))
						{
							pObj.hasEsdShoeFrame++;
						}
						if (pObj.hasEsdShoeFrame > _checkingCriteria.detectionFrameBuffer)
							pObj.hasEsdShoe = true;
					}
					if (!pObj.hasSmock)
					{
						if (checkHasSmock(tResult, _localOdResult[camId], { 0,1,2,3,4,9 }))
						{
							pObj.hasSmockFrame++;
						}
						if (pObj.hasSmockFrame > _checkingCriteria.detectionFrameBuffer)
							pObj.hasSmock = true;
					}
					
				
				}

				if (_doorPointHash.contains(camId))
				{
					QVector<QPointF>& area = _doorPointHash[camId];
					QPolygonF polyArea;
					for (auto& p : area)
					{
						polyArea << p;
					}

					const qreal cx = (pObj.trackingResult.x1 + pObj.trackingResult.x2) * 0.5;
					const qreal cy = std::max(pObj.trackingResult.y1, pObj.trackingResult.y2);   // bottom
					QPointF bottomCenter(cx, cy);
					
					bool inside = polyArea.containsPoint(
						bottomCenter,
						Qt::OddEvenFill  
					);

					if (inside)
					{
						pObj.inCheckingArea = true;

						bool pass = true;
						if (_checkingCriteria.checkMask)
						{
							if (!pObj.hasFacemask)
								pass = false;
						}
						if (_checkingCriteria.checkGlove)
						{
							if (!pObj.hasGlove)
								pass = false;
						}
						if (_checkingCriteria.checkShoe)
						{
							if (!pObj.hasEsdShoe)
								pass = false;
						}
						if (_checkingCriteria.checkSmock)
						{
							if (!pObj.hasSmock)
								pass = false;
						}
						pObj.isPass = pass;
					}
				}

				_sViewParentObjHash.insert(parentId, pObj);
			}
		}
	}

	emit updateSingleViewResult(_sViewParentObjHash, _localOdResult);


	_isProceesingGlobalId = false;
}

bool TrackingManager::checkHasMask(const OnnxResult& person,
	const std::vector<OnnxResult>& objDetections,
	QList<int> maskClassIds,
	float minObjConf,
	float minKptConf) const
{
	cv::Rect2f face = faceRoi(person, minKptConf);

	// Mask boxes are usually small; center-inside is very reliable.
	// IoU threshold low on purpose.
	return anyObjMatchesRoi(objDetections, maskClassIds, face,
		minObjConf,
		/*minIou=*/0.03f,
		/*useCenterInside=*/true);
}

bool TrackingManager::checkHasGlove(const OnnxResult& person,
	const std::vector<OnnxResult>& objDetections,
	QList<int> gloveClassIds,
	float minObjConf,
	float minKptConf) const
{
	cv::Rect2f leftHand = handRoi(person, Keypoint::LEFT_WRIST, Keypoint::LEFT_ELBOW, minKptConf);
	cv::Rect2f rightHand = handRoi(person, Keypoint::RIGHT_WRIST, Keypoint::RIGHT_ELBOW, minKptConf);

	std::vector<cv::Rect2f> rois;
	if (rectValid(leftHand))  rois.push_back(leftHand);
	if (rectValid(rightHand)) rois.push_back(rightHand);

	if (rois.empty()) return false;

	// If both hands visible, require both gloves. If only one visible, require one.
	const int need = (rois.size() >= 2) ? 2 : 1;
	const int got = countMatchesInRois(objDetections, gloveClassIds, rois,
		minObjConf,
		/*minIou=*/0.02f);
	return got >= need;
}

bool TrackingManager::checkHasShoe(const OnnxResult& person,
	const std::vector<OnnxResult>& objDetections,
	QList<int> shoeClassIds,
	float minObjConf,
	float minKptConf) const
{
	cv::Rect2f leftFoot = footRoi(person, Keypoint::LEFT_ANKLE, Keypoint::LEFT_KNEE, minKptConf);
	cv::Rect2f rightFoot = footRoi(person, Keypoint::RIGHT_ANKLE, Keypoint::RIGHT_KNEE, minKptConf);

	std::vector<cv::Rect2f> rois;
	if (rectValid(leftFoot))  rois.push_back(leftFoot);
	if (rectValid(rightFoot)) rois.push_back(rightFoot);

	if (rois.empty()) return false;

	// If both feet visible, require both shoes. If only one visible, require one.
	const int need = (rois.size() >= 2) ? 2 : 1;
	const int got = countMatchesInRois(objDetections, shoeClassIds, rois,
		minObjConf,
		/*minIou=*/0.02f);
	return got >= need;
}

bool TrackingManager::checkHasSmock(const OnnxResult& person,
	const std::vector<OnnxResult>& objDetections,
	QList<int> smockClassIds,
	float minObjConf,
	float minKptConf) const
{
	cv::Rect2f torso = torsoRoi(person, minKptConf);

	// Smock/shirt box usually overlaps torso a lot.
	return anyObjMatchesRoi(objDetections, smockClassIds, torso,
		minObjConf,
		/*minIou=*/0.08f,
		/*useCenterInside=*/true);
}


