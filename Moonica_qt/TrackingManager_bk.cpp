#include "TrackingManager.h"
#include "CommonStruct.h"



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
	cv::Point2f dir = pts[wrist] - pts[elbow]; // elbow → wrist
	float scale = expectedForearm / (cv::norm(dir) + 1e-6f);
	correctedWrist = pts[elbow] + dir * scale;

	return correctedWrist;
}


cv::Point2f TrackingManager::CorrectAnklePoint(const OnnxResult& localBox, bool left) {
	int shoulder = left ? Keypoint::LEFT_SHOULDER : Keypoint::RIGHT_SHOULDER;
	int hip = left ? Keypoint::LEFT_HIP : Keypoint::RIGHT_HIP;
	int knee = left ? Keypoint::LEFT_KNEE : Keypoint::RIGHT_KNEE;
	int ankle = left ? Keypoint::LEFT_ANKLE : Keypoint::RIGHT_ANKLE;

	auto pts = localBox.keypoints;

	// Step 1: torso length (shoulder–hip)
	float torsoLen = cv::norm(pts[shoulder] - pts[hip]);

	// Step 2: expected shin length (knee–ankle)
	float expectedShin = 0.8f * torsoLen;

	// Step 3: actual shin length
	float kneeToAnkle = cv::norm(pts[knee] - pts[ankle]);

	cv::Point2f correctedAnkle = pts[ankle];

	// Step 4: correction logic
	if (kneeToAnkle < 0.5f * expectedShin) {
		// Force ankle directly below knee with corrected shin length
		correctedAnkle.x = pts[knee].x;
		correctedAnkle.y = pts[knee].y + expectedShin;
	}
	else {
		// Optional: if it's already reasonable, still align directly below knee
		correctedAnkle.x = pts[knee].x;
		correctedAnkle.y = pts[knee].y + kneeToAnkle; // keep original shin length

	}
	

	return correctedAnkle;
}



float TrackingManager::computeIoU(cv::Rect_<float>& rect1, cv::Rect_<float>& rect2)
{
	float intersectionArea = (rect1 & rect2).area();
	float unionArea = rect1.area() + rect2.area() - intersectionArea;

	if (unionArea <= 0.0f)
		return 0.0f;

	return intersectionArea / unionArea;
}
QRectF TrackingManager::transformBoundingBox(const OnnxResult& localBox, const cv::Mat& homographyMatrix, bool isSitting)
{

	if (homographyMatrix.empty()) {
		return QRectF();
	}


	// 1. Compute bottom-center point of local box
	float cx = 0;
	float cy = 0;
	if (localBox.keypoints.size() >= 17) {
		if (isSitting)
		{
			cx = (localBox.keypoints[11].x + localBox.keypoints[12].x) / 2.0f;
			cy = (localBox.keypoints[15].y + localBox.keypoints[16].y) / 2.0f;
		}
		else
		{
			cx = (localBox.keypoints[15].x + localBox.keypoints[16].x) / 2.0f;
			cy = (localBox.keypoints[15].y + localBox.keypoints[16].y) / 2.0f;

		}

	}


	std::vector<cv::Point2f> localPoint = { cv::Point2f(cx, cy) };
	std::vector<cv::Point2f> globalPoint;

	// 2. Transform bottom center using homography
	cv::perspectiveTransform(localPoint, globalPoint, homographyMatrix);

	if (globalPoint.empty()) {
		return QRectF();
	}

	// 3. Use transformed bottom-center to build fixed rect
	float gx = globalPoint[0].x;
	float gy = globalPoint[0].y;


	float halfBox = BOX_SIZE / 2.0f;

	// QRectF is defined from top-left, so we offset from bottom-center
	return QRectF(gx - halfBox, gy - halfBox, BOX_SIZE, BOX_SIZE);
}


double TrackingManager::calculateIoU(const QRectF& boxA, const QRectF& boxB)
{
	// Determine the coordinates of the intersection rectangle
	double xA = std::max(boxA.left(), boxB.left());
	double yA = std::max(boxA.top(), boxB.top());
	double xB = std::min(boxA.right(), boxB.right());
	double yB = std::min(boxA.bottom(), boxB.bottom());

	// Compute the area of intersection
	double intersectionArea = std::max(0.0, xB - xA) * std::max(0.0, yB - yA);
	if (intersectionArea <= 0) {
		return 0.0;
	}

	// Compute the area of both bounding boxes
	double boxAArea = boxA.width() * boxA.height();
	double boxBArea = boxB.width() * boxB.height();

	// Compute the area of the union
	double unionArea = boxAArea + boxBArea - intersectionArea;

	// Return the IoU
	return intersectionArea / unionArea;
}

QVector<CurrentDetectionInfo> TrackingManager::adjustDetections(
	const QVector<CurrentDetectionInfo>& detections,
	int targetCount)
{
	QVector<CurrentDetectionInfo> adjusted = detections;

	if (detections.size() == targetCount || detections.size() < targetCount) {
		return adjusted; // nothing to do
	}

	// --- prepare data for k-means ---
	cv::Mat points(detections.size(), 2, CV_32F);
	for (int i = 0; i < detections.size(); i++) {
		QPointF c = detections[i].globalRect.center();
		points.at<float>(i, 0) = c.x();
		points.at<float>(i, 1) = c.y();
	}

	// --- run kmeans ---
	cv::Mat labels, centers;
	cv::kmeans(points, targetCount, labels,
		cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 10, 1.0),
		3, cv::KMEANS_PP_CENTERS, centers);

	// --- build adjusted detections ---
	adjusted.clear();
	adjusted.resize(targetCount);

	for (int i = 0; i < targetCount; i++) {
		CurrentDetectionInfo info;
		QPointF c(centers.at<float>(i, 0), centers.at<float>(i, 1));
		// use a small rect around the cluster center
		info.globalRect = QRectF(c.x() - BOX_SIZE / 2.0, c.y() - BOX_SIZE / 2.0, BOX_SIZE, BOX_SIZE);
		adjusted[i] = info;
	}

	return adjusted;
}

TrackingManager::TrackingManager()
{

}

TrackingManager::~TrackingManager()
{

}

void TrackingManager::run()
{

}

void TrackingManager::setDebugMode(bool debugMode)
{
	_debugMode = debugMode;
}

void TrackingManager::forceSetTotalFLoorPlanObject(int totalFloorPlanObject, bool debugMode)
{
	_totalObjectInFloorPlan = totalFloorPlanObject;
	_trackingResult.clear();
	_forceTrace = true;
	setDebugMode(debugMode);

	_moonica.forceSetTotalParentObject(totalFloorPlanObject, debugMode);
}

void TrackingManager::addActiveCamera(QString camId, cv::Mat homographyMatrix, QVector<QPointF> doorPoints)
{
	if (!_activeCameraList.contains(camId))
	{
		_activeCameraList.append(camId);
		_homographyMatrixHash[camId] = homographyMatrix;
		_doorPointHash[camId] = doorPoints;

		_moonica.addActiveCamera(camId, homographyMatrix, doorPoints);
	}
}

void TrackingManager::removeActiveCamera(QString camId)
{
	if (_activeCameraList.contains(camId))
	{
		_activeCameraList.removeAt(_activeCameraList.indexOf(camId));
		_homographyMatrixHash.remove(camId);
	}
}

void TrackingManager::onResultReady(QString camId, std::vector<OnnxResult> peResult, std::vector<OnnxResult> odResult)
{

	_localTrackingResult[camId] = peResult;  // overwrite old frame
	_localOdResult[camId] = odResult;

	if (_localTrackingResult.size() == _activeCameraList.size() && !_isProceesingGlobalId)
	{
		/*if (_debugMode) testAllCamRect();
		else processGlobalId();*/

		runMoonicaApi();
	}

}

void TrackingManager::processGlobalId_old()
{
	

	_isProceesingGlobalId = true;
	removeOverlappingResults();



	// Make a local copy of the incoming results and clear the shared buffer
	QHash<QString, std::vector<OnnxResult>> currentLocalTracking = _localTrackingResult;
	_localTrackingResult.clear();


	// This will hold the consolidated tracking results for the *current* frame.
	// We'll build this up by first matching against previous tracks, then against new unmatched detections.
	QVector<ParentObject> newTrackingResult;

	//const double IOU_THRESHOLD = 1.05; // Tune this threshold as needed

	// Step 1: Collect all global bounding boxes from the current cycle's local detections.
	// This makes it easier to compare against both previous and new tracks.

	QVector<CurrentDetectionInfo> currentGlobalDetections;

	for (auto it = currentLocalTracking.begin(); it != currentLocalTracking.end(); ++it) {
		QString camId = it.key();
		std::vector<OnnxResult>& oResult = it.value();
		cv::Mat homographyMatrix = _homographyMatrixHash.value(camId);
		if (homographyMatrix.empty()) continue;

		for (auto& localResult : oResult) {


			// pre-process result 
			// lengthen abnormal ankle skeleton
			float leftKneeAngle = computeAngle(localResult.keypoints[LEFT_HIP], localResult.keypoints[LEFT_KNEE], localResult.keypoints[LEFT_ANKLE]);
			float rightKneeAngle = computeAngle(localResult.keypoints[RIGHT_HIP], localResult.keypoints[RIGHT_KNEE], localResult.keypoints[RIGHT_ANKLE]);


			localResult.keypoints[Keypoint::LEFT_ANKLE] = CorrectAnklePoint(localResult, true);
			localResult.keypoints[Keypoint::RIGHT_ANKLE] = CorrectAnklePoint(localResult, false);

			localResult.keypoints[Keypoint::LEFT_WRIST] = CorrectWristPoint(localResult, true);
			localResult.keypoints[Keypoint::RIGHT_WRIST] = CorrectWristPoint(localResult, false);


			bool isSitting = (leftKneeAngle < 140 || rightKneeAngle < 140);

			QRectF globalRect = transformBoundingBox(localResult, homographyMatrix, isSitting);
			if (!globalRect.isValid()) continue;

			CurrentDetectionInfo cInfo;
			cInfo.isSitting = isSitting;
			cInfo.globalRect = globalRect;

			currentGlobalDetections.push_back(cInfo);
		}
	}

	if (currentGlobalDetections.isEmpty())
	{
		_isProceesingGlobalId = false;
		return;
	}

	// check increase or decreace trackingObject by tracing doorCam
	for (auto it2 = currentLocalTracking.begin(); it2 != currentLocalTracking.end(); ++it2)
	{

		const QString& camId = it2.key();
		std::vector<OnnxResult>& objectCoordinates = it2.value();

		// only process Door cam
		if (!_doorCamObjectHash.contains(camId))
			continue;

		auto& dCamObject = _doorCamObjectHash[camId];

		for (auto& curResult : objectCoordinates)
		{
			int tid = curResult.tracking_id;
			QRectF curCoordinate(curResult.x1, curResult.y1,
				curResult.x2 - curResult.x1,
				curResult.y2 - curResult.y1);

			// check if we saw this tracking_id before
			if (dCamObject.objectRectHash.contains(tid))
			{
				QRectF prevCoordinate = dCamObject.objectRectHash[tid];


				if (camId == "Door")
				{
					double prevX = prevCoordinate.x() + (prevCoordinate.width() / 2.0);
					double currentX = (curResult.x1 + curResult.x2) / 2.0;

					/*	qDebug() << "-----------";
						qDebug() << "Prev Coordinate: " << prevX;
						qDebug() << "Current Coordinate: " << currentX;
						qDebug() << "-----------";*/
						// Enter condition
					if (prevX < 600 && currentX >= 600)
					{
						QString uniqueId = camId + QString::number(tid);
						if (!_objectIdList.contains(uniqueId))
						{
							//qDebug() << "@@@@Object added: " << uniqueId;
							_totalObjectInFloorPlan++;
							_objectIdList.append(uniqueId);
							qDebug() << "@@@@Object added: " << _totalObjectInFloorPlan;
						}

					}
					// Exit condition
					else if (prevX > 600 && currentX <= 600)
					{
						//qDebug() << "@@@object removed";
						_totalObjectInFloorPlan--;
						if (_totalObjectInFloorPlan < 0) _totalObjectInFloorPlan = 0;
						qDebug() << "@@@object removed: " << _totalObjectInFloorPlan;
					}
				}
				else if (camId == "Walkway")
				{
					double prevY = prevCoordinate.y() + prevCoordinate.height();
					double currentY = curResult.y2;

					/*qDebug() << "-----------";
					qDebug() << "Prev Coordinate: " << prevY;
					qDebug() << "Current Coordinate: " << currentY;
					qDebug() << "-----------";*/

					if (prevY < 1000 && currentY >= 1000)
					{
						QString uniqueId = camId + QString::number(tid);
						if (!_objectIdList.contains(uniqueId))
						{

							_totalObjectInFloorPlan++;
							_objectIdList.append(uniqueId);
							qDebug() << "@@@@Object added: " << _totalObjectInFloorPlan;
						}

					}
					// Exit condition
					else if (prevY > 1000 && currentY <= 1000)
					{

						_totalObjectInFloorPlan--;
						if (_totalObjectInFloorPlan < 0) _totalObjectInFloorPlan = 0;
						qDebug() << "@@@object removed: " << _totalObjectInFloorPlan;
					}
				}

			}

			// update (or insert) current rect
			//dCamObject.objectRectHash[tid] = curCoordinate;
		}
	}



	for (auto it2 = currentLocalTracking.begin(); it2 != currentLocalTracking.end(); ++it2)
	{

		const QString& camId = it2.key();
		std::vector<OnnxResult>& objectCoordinates = it2.value();


		if (camId == "Door" || camId == "Walkway")
		{
			for (auto& curResult : objectCoordinates)
			{
				QRectF curCoordinate(curResult.x1, curResult.y1,
					curResult.x2 - curResult.x1,
					curResult.y2 - curResult.y1);


				_doorCamObjectHash[camId].camId = camId;
				_doorCamObjectHash[camId].objectRectHash[curResult.tracking_id] = curCoordinate;
			}
		}

	}


	if (_totalObjectInFloorPlan <= 0)
	{
		_isProceesingGlobalId = false;
		return;
	}

	QVector<CurrentDetectionInfo> mergedDetections;
	QVector<bool> processed(currentGlobalDetections.size(), false);

	for (int i = 0; i < currentGlobalDetections.size(); ++i) {
		if (processed[i]) continue;

		CurrentDetectionInfo current = currentGlobalDetections[i];
		processed[i] = true;

		for (int j = i + 1; j < currentGlobalDetections.size(); ++j) {
			if (processed[j]) continue;

			CurrentDetectionInfo other = currentGlobalDetections[j];

			// Use the dedicated calculateIoU function
			double iou = calculateIoU(current.globalRect, other.globalRect);

			if (iou > IOU_THRESHOLD) {
				// Merge the rectangles
				QRectF unionRect = current.globalRect.united(other.globalRect);

				qreal minX = unionRect.center().x() - (BOX_SIZE / 2.0);
				qreal minY = unionRect.center().y() - (BOX_SIZE / 2.0);
				qreal maxX = minX + BOX_SIZE;
				qreal maxY = minY + BOX_SIZE;

				current.globalRect = QRectF(minX, minY, maxX - minX, maxY - minY);

				// Combine: values from hash2 overwrite hash1 if key exists


				processed[j] = true; // Mark the merged rectangle as processed
			}
		}
		mergedDetections.push_back(current);
	}

	currentGlobalDetections = mergedDetections;
	currentGlobalDetections = adjustDetections(currentGlobalDetections, _totalObjectInFloorPlan);

	//qDebug() << "_totalObjectInFloorPlan: " << _totalObjectInFloorPlan;
	//qDebug() << "Global Detection size: " << currentGlobalDetections.size();

	//!!! here take advantage of local id
		// find "home first"
		// no home find iou 
		// both fail = new rect
		// out need know whcih globa id out

	// //Step 2: Match current global detections against previous frame's tracked objects (_trackingResult).
	// //The previous _trackingResult effectively represents the "known" objects.
	//for (auto& prevTrackedObject : _trackingResult) {
	//	QRectF prevRect(prevTrackedObject.global_x, prevTrackedObject.global_y,
	//		prevTrackedObject.global_w, prevTrackedObject.global_h);

	//	double bestIou = 0.0;
	//	int bestMatchIndex = -1;

	//	// Find the best matching current detection for this previous tracked object
	//	for (int i = 0; i < currentGlobalDetections.size(); ++i) {
	//		// Only consider detections not yet matched to a previous track
	//		if (currentGlobalDetections[i].matchedToPrevious) continue;

	//		double iou = calculateIoU(prevRect, currentGlobalDetections[i].globalRect);

	//		if (iou > bestIou && iou > IOU_THRESHOLD) {
	//			bestIou = iou;
	//			bestMatchIndex = i;
	//		}
	//	}

	//	if (bestMatchIndex != -1) {
	//		// A match was found! Update the previous tracked object.
	//		// We'll carry over the globalId and update its properties.
	//		ParentObject updatedObject = prevTrackedObject; // Start with the previous object's state
	//		updatedObject.localResult.clear(); // Clear local results from previous frame

	//		const auto& matchedDetection = currentGlobalDetections[bestMatchIndex];

	//		// Add the new local result
	//		//updatedObject.localResult[matchedDetection.camId]->push_back(matchedDetection.localResult);

	//		// Update global bounding box to the matched detection's box or a merged one
	//		// For simplicity, let's update it to the current detection's box.
	//		// A more advanced approach might use a weighted average or Kalman filter.
	//		updatedObject.global_x = matchedDetection.globalRect.x();
	//		updatedObject.global_y = matchedDetection.globalRect.y();
	//		updatedObject.global_w = matchedDetection.globalRect.width();
	//		updatedObject.global_h = matchedDetection.globalRect.height();
	//		updatedObject.isSitting = matchedDetection.isSitting;

	//		newTrackingResult.push_back(updatedObject);
	//		currentGlobalDetections[bestMatchIndex].matchedToPrevious = true; // Mark as matched
	//	}
	//	else {
	//		// No match found for this prevTrackedObject.
	//		// You might consider it "lost" or try to predict its position for a few frames.
	//		// For now, if no match, it's not added to newTrackingResult.
	//		// If you want to keep "lost" tracks for a grace period, you'd add it here
	//		// and decrement a "track_age" counter, removing it when it's too old.

	//		// can record the lost track iD
	//	}
	//}

	// Step 3: Process current global detections that were *not* matched to previous tracks.
	// These are either genuinely new objects or objects that appeared while others were lost.
	for (const auto& currentDetection : currentGlobalDetections) {
		if (currentDetection.matchedToPrevious) continue; // Already handled

		bool matchedToNew = false;
		// Compare this unmatched detection with other already-processed new objects in newTrackingResult
		for (auto& existingNewObject : newTrackingResult) {
			// Only compare with objects that originated from a new detection (i.e., not carrying over a global ID from previous frame)
			// This logic is a bit tricky. A simpler approach is to treat all remaining current detections as potentially new.
			// For now, let's assume if it wasn't matched to a previous track, it could be a new one or merge with another *new* track in this frame.
			// To properly handle this, we might need another temporary list for truly "new" tracks in this frame before merging.

			// A simpler approach for the remaining unmatched detections:
			// If it's not matched to a previous track, it's either a brand new object,
			// or an object that appeared and needs a new global ID for this frame.
			// We will assign a new ID for now. If you want to merge new detections within the same frame
			// that didn't match a previous object, you'd replicate your original inner loop here.
			// However, it's more common to first establish identity across frames.

			QRectF existingRect(existingNewObject.global_x, existingNewObject.global_y,
				existingNewObject.global_w, existingNewObject.global_h);

			double iou = calculateIoU(currentDetection.globalRect, existingRect);

			if (iou > IOU_THRESHOLD) {
				// Merge this new detection into an existing "new" object in newTrackingResult
				//existingNewObject.localResult[currentDetection.camId]->push_back(currentDetection.localResult);

				QRectF unionRect = existingRect.united(currentDetection.globalRect);


				/* existingNewObject.global_x = unionRect.x();
				 existingNewObject.global_y = unionRect.y();
				 existingNewObject.global_w = unionRect.width();
				 existingNewObject.global_h = unionRect.height();*/
				existingNewObject.global_x = unionRect.center().x() - (BOX_SIZE / 2.0);
				existingNewObject.global_y = unionRect.center().y() - (BOX_SIZE / 2.0);
				existingNewObject.global_w = BOX_SIZE;
				existingNewObject.global_h = BOX_SIZE;
				matchedToNew = true;
				break;
			}
		}

		if (!matchedToNew) {
			// This is a truly new object for this frame (not matched to previous or other new ones yet)
			ParentObject newObject;
			//newObject.globalId = QString::number(_totalObjectInFloorPlan); // Generate a unique ID
			newObject.globalId = QUuid::createUuid().toString();
			//newObject.globalId = QString::number(_uuidGenerated); // Generate a unique ID
			_uuidGenerated++;
			newObject.isSitting = currentDetection.isSitting;
			newObject.global_x = currentDetection.globalRect.x();
			newObject.global_y = currentDetection.globalRect.y();
			newObject.global_w = currentDetection.globalRect.width();
			newObject.global_h = currentDetection.globalRect.height();
			//newObject.localResult[currentDetection.camId]->push_back(currentDetection.localResult);
			//newObject.objectId = QString::number(currentDetection.localResult->obj_id); // Initial objectId

			newTrackingResult.push_back(newObject);
		}
	}


	// Step 4: Update _trackingResult with the new consolidated results.
	// This becomes the "previous frame" for the next cycle.
	processGlobalTracking(newTrackingResult);
	_trackingResult = newTrackingResult;

	//qDebug() << "Tracking result: " << _trackingResult.size();
	// Emit the results
	emit updateGlobalCoordinate(_trackingResult, _totalObjectInFloorPlan);
	_isProceesingGlobalId = false;
}

void TrackingManager::removeOverlappingResults()
{
	// remove local cam ovellapped boudnign box
	for (auto it = _localTrackingResult.begin(); it != _localTrackingResult.end(); ++it) {
		QString camId = it.key();
		auto& results = it.value();

		// Use indices so we can erase safely
		for (size_t i = 0; i < results.size(); ++i) {
			cv::Rect_<float> rect1(results[i].x1, results[i].y1,
				results[i].x2 - results[i].x1,
				results[i].y2 - results[i].y1);

			for (size_t j = i + 1; j < results.size();) {
				cv::Rect_<float> rect2(results[j].x1, results[j].y1,
					results[j].x2 - results[j].x1,
					results[j].y2 - results[j].y1);

				if (results[j].obj_id != results[i].obj_id) continue;

				float iou = computeIoU(rect1, rect2);

				if (iou > 0.5f) {
					// Remove the lower-accuracy one
					if (results[i].accuracy >= results[j].accuracy) {
						results.erase(results.begin() + j); // erase j, keep i
					}
					else {
						results.erase(results.begin() + i); // erase i
						--i; // backtrack since i element is gone
						break; // break inner loop and restart for this i
					}
				}
				else {
					++j; // move to next
				}
			}
		}
	}
}


void TrackingManager::checkFloorPlanObjectCount(QHash<QString, std::vector<OnnxResult>>& currentLocalTracking)
{
	_newBornChildIdList.clear();
	_exitParentIdList.clear();
	// check increase or decreace trackingObject by tracing doorCam

	auto processPolygonRegion = [&](const QString& camId,
		int tid,
		const QRectF& prevCoordinate,
		const OnnxResult& curResult)
		{
			// Build polygon for this camera
			QPolygonF regionPoly;
			//qDebug() << "Door point hash: " << _doorPointHash[camId].size();
			if (_doorPointHash[camId].isEmpty()) return;
			if (camId != "Door" && camId != "Walkway")
			{
				return;
			}

			for (auto& p : _doorPointHash[camId])  // could rename _doorPointHash → _regionPointHash
				regionPoly << p;

			// Pick point representation (bottom-center works for both Door & Walkway)
			QPointF prevBottomCenter(prevCoordinate.x() + prevCoordinate.width() / 2.0,
				prevCoordinate.y() + prevCoordinate.height());
			QPointF currBottomCenter((curResult.x1 + curResult.x2) / 2.0,
				curResult.y2);

			// Containment check
			bool wasInside = regionPoly.containsPoint(prevBottomCenter, Qt::OddEvenFill);
			bool isInside = regionPoly.containsPoint(currBottomCenter, Qt::OddEvenFill);

			QString uniqueId = camId + "[@]" + QString::number(tid);

			if (wasInside && !isInside)  // Enter
			{

				if (!_objectIdList.contains(uniqueId))
				{
					//_totalObjectInFloorPlan++;
					_objectIdList.append(uniqueId);
					_newBornChildIdList.append(uniqueId);
					qDebug() << "[Floorplan] Object Entered from cam:" << camId << " Tracking Id: " << tid;
				}
				else
				{
					qDebug() << "[Floorplan] Duplicated Object Entered: " << camId << " Tracking Id: " << tid;;
				}
			}
			else if (!wasInside && isInside)  // Exit
			{
				//_totalObjectInFloorPlan--;

				_exitParentIdList.append(uniqueId);
				qDebug() << "[Floorplan] Object Existed: " << camId << " Tracking Id: " << tid;;
			}
		};

	for (auto it2 = currentLocalTracking.begin(); it2 != currentLocalTracking.end(); ++it2)
	{
		const QString& camId = it2.key();
		std::vector<OnnxResult>& objectCoordinates = it2.value();

		// only process cams with polygons
		if (!_doorCamObjectHash.contains(camId))
			continue;

		auto& dCamObject = _doorCamObjectHash[camId];

		for (auto& curResult : objectCoordinates)
		{
			int tid = curResult.tracking_id;
			QRectF curCoordinate(curResult.x1, curResult.y1,
				curResult.x2 - curResult.x1,
				curResult.y2 - curResult.y1);

			if (dCamObject.objectRectHash.contains(tid))
			{
				QRectF prevCoordinate = dCamObject.objectRectHash[tid];
				processPolygonRegion(camId, tid, prevCoordinate, curResult);
			}

			// update rect
			dCamObject.objectRectHash[tid] = curCoordinate;
		}
	}


	// ignore/remove the child object if it is fall inside ignore area (door point)
	for (auto it2 = currentLocalTracking.begin(); it2 != currentLocalTracking.end(); ++it2)
	{
		const QString& camId = it2.key();
		std::vector<OnnxResult>& objectCoordinates = it2.value();


		for (auto& curResult : objectCoordinates)
		{
			QRectF curCoordinate(curResult.x1, curResult.y1,
				curResult.x2 - curResult.x1,
				curResult.y2 - curResult.y1);

			_doorCamObjectHash[camId].camId = camId;
			_doorCamObjectHash[camId].objectRectHash[curResult.tracking_id] = curCoordinate;
		}

		// Build polygon for this camera
		QPolygonF regionPoly;
		for (auto& p : _doorPointHash[camId])
			regionPoly << p;

		// Filter: remove objects if inside polygon
		objectCoordinates.erase(
			std::remove_if(objectCoordinates.begin(), objectCoordinates.end(),
				[&](const OnnxResult& curResult)
				{
					QRectF curCoordinate(curResult.x1, curResult.y1,
					curResult.x2 - curResult.x1,
					curResult.y2 - curResult.y1);

					// 1. Compute bottom-center point of local box
					float cx = 0;
					float cy = 0;
					/*if (curResult.keypoints.size() >= 17) {
						cx = (curResult.keypoints[15].x + curResult.keypoints[16].x) / 2.0f;
						cy = (curResult.keypoints[15].y + curResult.keypoints[16].y) / 2.0f;
					}*/

					 cx = curCoordinate.x() + curCoordinate.width() / 2.0; 
					 cy = curCoordinate.y() + curCoordinate.height();   

					// use bottom-center point for containment check
					QPointF bottomCenter(cx ,cy);

					//qDebug() << "Bottom point: " << bottomCenter;
					//qDebug() << "Region Polygon: " << regionPoly;
					if (regionPoly.containsPoint(bottomCenter, Qt::OddEvenFill))
					{
						//qDebug() << "Ereased due to entering ignore area";
						return true;
					}
					else
					{
						//qDebug() << "Proceed";
						return false;
					}

				}),
			objectCoordinates.end()
		);

	}
	auto removeExitedParents = [&](QVector<ParentObject>& vec)
		{
			vec.erase(std::remove_if(vec.begin(), vec.end(),
				[&](const ParentObject& obj) {
					for (const auto& tId : obj.parentIds) {
						if (_exitParentIdList.contains(tId)) {
							_totalObjectInFloorPlan--;
							if (_totalObjectInFloorPlan < 0)_totalObjectInFloorPlan == 0;

							if (_objectIdList.contains(tId))_objectIdList.removeAll(tId);

							qDebug() << "[Floorplan] Parent Removed due to exit: " << obj.globalId;
							qDebug() << "[Floorplan] Total Object: " << _totalObjectInFloorPlan;

							return true; // mark for removal
						}
					}
					return false;
				}),
				vec.end());
		};

	// Kill exit parents from both lists
	//removeExitedParents(newParents);
	removeExitedParents(_trackingResult);


}

void TrackingManager::childFormParent(QVector<CurrentDetectionInfo>& currentGlobalDetections, QVector<ParentObject>& newParent)
{
	//QVector<bool> processed(currentGlobalDetections.size(), false);

	//for (int i = 0; i < currentGlobalDetections.size(); ++i) {
	//	if (processed[i]) continue;

	//	CurrentDetectionInfo current = currentGlobalDetections[i];
	//	QString curParentID = current.camTrackingId;
	//	bool merged = false;

	//	for (int j = i + 1; j < currentGlobalDetections.size(); ++j) {
	//		if (processed[j]) continue;

	//		CurrentDetectionInfo other = currentGlobalDetections[j];
	//		QString otherParentID = other.camTrackingId;

	//		double iou = calculateIoU(current.globalRect, other.globalRect);

	//		if (iou > IOU_THRESHOLD) {
	//			// Merge the rectangles
	//			QRectF unionRect = current.globalRect.united(other.globalRect);

	//			qreal minX = unionRect.center().x() - (BOX_SIZE / 2.0);
	//			qreal minY = unionRect.center().y() - (BOX_SIZE / 2.0);

	//			current.globalRect = QRectF(minX, minY, BOX_SIZE, BOX_SIZE);

	//			processed[j] = true; // remove "other"
	//			merged = true;

	//			// Create parent object
	//			_totalParentAdded++;
	//			ParentObject parent;
	//			parent.globalId = QString::number(_totalParentAdded);
	//			parent.global_x = minX;
	//			parent.global_y = minY;
	//			parent.global_w = BOX_SIZE;
	//			parent.global_h = BOX_SIZE;
	//			parent.isTracking = true;
	//			parent.isSitting = current.isSitting;
	//			parent.isParent = true;
	//			qDebug() << "New parent add from child iou";
	//			if (!parent.parentIds.contains(curParentID)) parent.parentIds.append(curParentID);
	//			if (!parent.parentIds.contains(otherParentID)) parent.parentIds.append(otherParentID);

	//			newParent.append(parent);
	//		}
	//	}

	//	// If current was merged into a parent, mark it too
	//	if (merged) {
	//		processed[i] = true;
	//	}
	//}

	currentGlobalDetections = adjustDetections(currentGlobalDetections, _totalObjectInFloorPlan);
	for (int i = 0; i < currentGlobalDetections.size(); ++i) {

		const auto& child = currentGlobalDetections[i];

		_totalParentAdded++;
		ParentObject parent;
		parent.globalId = QString::number(_totalParentAdded);
		parent.global_x = child.globalRect.x();
		parent.global_y = child.globalRect.y();
		parent.global_w = child.globalRect.width();
		parent.global_h = child.globalRect.height();
		parent.isTracking = true;
		parent.lostTracKFrameCount = 0;
		parent.isSitting = child.isSitting;
		parent.isParent = true;
		//parent.parentIds.append(child.camTrackingId);
		//parent.childCamIds.append(child.camID);
		qDebug() << "[Child Form Parent] New parent(ID: "<< parent.globalId <<") formed from single child";
		
		newParent.append(parent);

	}

	// Clear children since all have been upgraded
	//currentGlobalDetections.clear();
}

void TrackingManager::testAllCamRect()
{
	
	_isProceesingGlobalId = true;

	QHash<QString, std::vector<OnnxResult>> currentLocalTracking = _localTrackingResult;

	QVector<ParentObject> newParents; // Thsis vector all will become parent

	// transform all lcoal rects to global rects
	QVector<CurrentDetectionInfo> currentGlobalDetections;
	for (auto it = currentLocalTracking.begin(); it != currentLocalTracking.end(); ++it) {
		QString camId = it.key();
		std::vector<OnnxResult>& oResult = it.value();
		cv::Mat homographyMatrix = _homographyMatrixHash.value(camId);
		if (homographyMatrix.empty()) continue;

		for (auto& localResult : oResult) {

			if (localResult.obj_id != 0) continue; //only process people id 
			// pre-process result 
			// lengthen abnormal ankle skeleton
			float leftKneeAngle = computeAngle(localResult.keypoints[LEFT_HIP], localResult.keypoints[LEFT_KNEE], localResult.keypoints[LEFT_ANKLE]);
			float rightKneeAngle = computeAngle(localResult.keypoints[RIGHT_HIP], localResult.keypoints[RIGHT_KNEE], localResult.keypoints[RIGHT_ANKLE]);


			localResult.keypoints[Keypoint::LEFT_ANKLE] = CorrectAnklePoint(localResult, true);
			localResult.keypoints[Keypoint::RIGHT_ANKLE] = CorrectAnklePoint(localResult, false);


			bool isSitting = (leftKneeAngle < 140 || rightKneeAngle < 140);

			QRectF globalRect = transformBoundingBox(localResult, homographyMatrix, isSitting);
			if (!globalRect.isValid()) continue;

			/*	CurrentDetectionInfo cInfo;
				cInfo.isSitting = isSitting;
				cInfo.globalRect = globalRect;
				cInfo.camTrackingId = camId + "[@]" + QString::number(localResult.tracking_id);
				cInfo.camID = camId;
				currentGlobalDetections.push_back(cInfo);*/

			_totalObjectInFloorPlan++;
			ParentObject newParent;
			newParent.globalId = camId + "[@]" + QString::number(localResult.tracking_id);
			newParent.global_x = globalRect.x();
			newParent.global_y = globalRect.y();
			newParent.global_w = globalRect.width();
			newParent.global_h = globalRect.height();
			newParent.isTracking = true;
			newParents.append(newParent);
		}
	}
	_trackingResult = newParents;
	emit updateGlobalCoordinate(_trackingResult, _totalObjectInFloorPlan);
	_isProceesingGlobalId = false;
	_forceTrace = false;
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

void TrackingManager::processGlobalId()
{

	_isProceesingGlobalId = true;
	//removeOverlappingResults();
	

	// Make a local copy of the incoming results and clear the shared buffer
	QHash<QString, std::vector<OnnxResult>> currentLocalTracking = _localTrackingResult;
	
	checkFloorPlanObjectCount(currentLocalTracking);


	QVector<ParentObject> newParents; // Thsis vector all will become parent 
	QHash<QString, ParentObject> upgradedParents; // key: globalId;

	// transform all lcoal rects to global rects
	QVector<CurrentDetectionInfo> currentGlobalDetections;
	for (auto it = currentLocalTracking.begin(); it != currentLocalTracking.end(); ++it) {
		QString camId = it.key();
		std::vector<OnnxResult>& oResult = it.value();
		cv::Mat homographyMatrix = _homographyMatrixHash.value(camId);
		if (homographyMatrix.empty()) continue;

		for (auto& localResult : oResult) {

			if (localResult.obj_id != 0) continue; //only process people id 
			// pre-process result 
			// lengthen abnormal ankle skeleton
			float leftKneeAngle = computeAngle(localResult.keypoints[LEFT_HIP], localResult.keypoints[LEFT_KNEE], localResult.keypoints[LEFT_ANKLE]);
			float rightKneeAngle = computeAngle(localResult.keypoints[RIGHT_HIP], localResult.keypoints[RIGHT_KNEE], localResult.keypoints[RIGHT_ANKLE]);


			localResult.keypoints[Keypoint::LEFT_ANKLE] = CorrectAnklePoint(localResult, true);
			localResult.keypoints[Keypoint::RIGHT_ANKLE] = CorrectAnklePoint(localResult, false);


			bool isSitting = (leftKneeAngle < 140 || rightKneeAngle < 140);

			QRectF globalRect = transformBoundingBox(localResult, homographyMatrix, isSitting);
			if (!globalRect.isValid()) continue;

			CurrentDetectionInfo cInfo;
			cInfo.isSitting = isSitting;
			cInfo.globalRect = globalRect;
			cInfo.camTrackingId = camId + "[@]" + QString::number(localResult.tracking_id);
			cInfo.camID = camId;
			currentGlobalDetections.push_back(cInfo);
		}
	}

	// upgrade new born child to parent
	for (auto it = currentGlobalDetections.begin(); it != currentGlobalDetections.end(); )
	{

		if (_newBornChildIdList.contains(it->camTrackingId))
		{

			_totalParentAdded++;
			ParentObject parent;
			parent.globalId = QString::number(_totalParentAdded);
			parent.global_x = it->globalRect.x();
			parent.global_y = it->globalRect.y();
			parent.global_w = it->globalRect.width();
			parent.global_h = it->globalRect.height();
			parent.isTracking = true;
			parent.lostTracKFrameCount = 0;
			parent.isSitting = it->isSitting;
			parent.isParent = true;
			parent.parentIds.append(it->camTrackingId);
			parent.childCamIds.append(it->camID);
			newParents.append(parent);

			_totalObjectInFloorPlan++;
			qDebug() << "Parent Object added from new born child: " << parent.globalId;
			qDebug() << "Total Parent: " << _totalObjectInFloorPlan;
			//  erase current element and move iterator forward
			it = currentGlobalDetections.erase(it);
		}
		else
		{
			++it; //  safely move forward if not erased
		}
	}

	//if (currentGlobalDetections.isEmpty() || _totalObjectInFloorPlan <= 0)
	//{
	//	emit updateGlobalCoordinate(_trackingResult, _totalObjectInFloorPlan);
	//	_isProceesingGlobalId = false;
	//	return;
	//}

	
	// scenario 1: no parent can be found
	if (_trackingResult.isEmpty() && newParents.isEmpty() && _totalObjectInFloorPlan > 0)
	{
		// here self handle first initialization
		// to do, when k mean, need record its camtrackId
		childFormParent(currentGlobalDetections, newParents);
	}


	// before find parent, all parent tracking set to false
	for (auto& parent : _trackingResult)
	{

		parent.isTracking = false;
		parent.childCamIds.clear();
	}

	// remove child that too far away from parent
	for (auto parentIt = _trackingResult.begin(); parentIt != _trackingResult.end(); ++parentIt)
	{
		QPointF parentCenter(parentIt->global_x + parentIt->global_w / 2.0,
			parentIt->global_y + parentIt->global_h / 2.0);

		for (auto it = currentGlobalDetections.begin(); it != currentGlobalDetections.end(); ++it)
		{
			QString childCamId = it->camID;
			QString parentId = it->camTrackingId;
			if (parentIt->parentIds.contains(parentId))
			{
				QPointF childCenter = it->globalRect.center();
				QPointF parentCenter(parentIt->global_x + parentIt->global_w / 2.0,
					parentIt->global_y + parentIt->global_h / 2.0);

				double dx = childCenter.x() - parentCenter.x();
				double dy = childCenter.y() - parentCenter.y();
				double dist = std::sqrt(dx * dx + dy * dy);

				if (dist > MAX_PARENT_DIST)
				{
					parentIt->parentIds.removeAll(parentId);
					parentIt->childCamIds.removeAll(childCamId);
					qDebug() << "[Child far from Parent] Removed Child(" + parentId + "), too far from parent(" + parentIt->globalId + ")(Dist:" << dist << ")";
				}

			}
		}
	}

	QSet<QString> childIdsToRemove;
	QSet<QString> parentIdsToRemove;

	for (auto it = currentGlobalDetections.begin(); it != currentGlobalDetections.end(); ++it)
	{
		QString parentId = it->camTrackingId;
		bool foundParent = false;
		QString childCamId = it->camID;

		for (auto parentIt = _trackingResult.begin(); parentIt != _trackingResult.end(); ++parentIt)
		{
			if (parentIt->parentIds.contains(parentId) && parentIt->isParent && !parentIt->childCamIds.contains(childCamId))
			{
				QPointF childCenter = it->globalRect.center();
				QPointF parentCenter(parentIt->global_x + parentIt->global_w / 2.0,
					parentIt->global_y + parentIt->global_h / 2.0);

				double dx = childCenter.x() - parentCenter.x();
				double dy = childCenter.y() - parentCenter.y();
				double dist = std::sqrt(dx * dx + dy * dy);

				if (dist <= MAX_PARENT_DIST)
				{
					QRectF childRect = it->globalRect;


					parentIt->isSitting = it->isSitting;
					parentIt->lostTracKFrameCount = 0;
					parentIt->childCamIds.append(it->camID);
					// mark for removal later
					childIdsToRemove.insert(parentId);
					parentIdsToRemove.insert(parentIt->globalId);
					foundParent = true;

					// before upgrade id, recode the parent
					upgradedParents[parentIt->globalId] = *parentIt;

					if (!parentIt->isTracking)
					{
						parentIt->isTracking = true;
						parentIt->global_x = childRect.x();
						parentIt->global_y = childRect.y();
						parentIt->global_w = childRect.width();
						parentIt->global_h = childRect.height();
					}
					else
					{
						QRectF unionRect = it->globalRect.united(
							QRectF(parentIt->global_x, parentIt->global_y, parentIt->global_w, parentIt->global_h)
						);
						QRectF newRectF = QRectF(
							unionRect.center().x() - BOX_SIZE / 2,
							unionRect.center().y() - BOX_SIZE / 2,
							BOX_SIZE,
							BOX_SIZE
						);
						parentIt->isTracking = true;
						parentIt->global_x = newRectF.x();
						parentIt->global_y = newRectF.y();
						parentIt->global_w = newRectF.width();
						parentIt->global_h = newRectF.height();
					}
				

				}
				else
				{
					parentIt->parentIds.removeAll(parentId);
					parentIt->childCamIds.removeAll(childCamId);
					qDebug() << "[Child far from Parent] Skipped upgrade: child(" + parentId + ") too far from parent(" + parentIt->globalId + ")(dist:" << dist << ")";

				}
				break;
			}
		}
	}

	

	// Remove children
	currentGlobalDetections.erase(
		std::remove_if(currentGlobalDetections.begin(), currentGlobalDetections.end(),
			[&](const auto& child) {
				return childIdsToRemove.contains(child.camTrackingId);
			}),
		currentGlobalDetections.end());

	// Remove parents
	_trackingResult.erase(
		std::remove_if(_trackingResult.begin(), _trackingResult.end(),
			[&](const auto& parent) {
				if (parentIdsToRemove.contains(parent.globalId))
				{
					ParentObject newParent = parent;
					newParents.append(newParent);
					return true;
				}
				else
				{
					return false;
				}

			}),
		_trackingResult.end());

	childIdsToRemove.clear();
	parentIdsToRemove.clear();
	/*for (auto& a : _trackingResult)
	{
		qDebug() << "Global Id: " << a.globalId << " lost its tracking with child Ids: " << a.parentIds;
	}*/






	for (int i = 0; i < 3; i++)
	{
		if (!currentGlobalDetections.isEmpty())
		{
			findNearestChild(_trackingResult, newParents, upgradedParents, currentGlobalDetections);
		}
	}
	

	// for lost track parent, see if can be saved by lost child
	lostTrackParentFindNearestChild(_trackingResult, newParents,currentGlobalDetections);
	

	auto removeExitedParents = [&](QVector<ParentObject>& vec)
		{
			vec.erase(std::remove_if(vec.begin(), vec.end(),
				[&](const ParentObject& obj) {

					if (obj.lostTracKFrameCount > 9999) {
						_totalObjectInFloorPlan--;
						if (_totalObjectInFloorPlan < 0)_totalObjectInFloorPlan = 0;
						for (const auto& tId : obj.parentIds) {
							if (_objectIdList.contains(tId))_objectIdList.removeAll(tId);
						}

						qDebug() << "[Parent Lost] Parent Removed due to lost track: " << obj.globalId;
						qDebug() << "[Parent Lost] Total Object: " << _totalObjectInFloorPlan;
						return true;
					}

					return false;
				}),
				vec.end());
		};

	// Kill lost track parents if lost track more than 20 frames
	removeExitedParents(_trackingResult);
	// lost track parent (no child found)
	for (auto& t : _trackingResult)
	{
		//qDebug() << "Lost Track Parent: " << t.globalId;
		t.isTracking = false;
		t.lostTracKFrameCount++;
		newParents.append(t);
	}

	processCriteriaChecking(newParents);
	_localTrackingResult.clear();
	_trackingResult = newParents;

	/*qDebug() << "====";
	for (auto& tR : _trackingResult)
	{
		qDebug() << "Global Id: " << tR.globalId;
		qDebug() << "Tracking ID " << tR.parentIds;

	}
	qDebug() << "====";*/

	for (auto& c : currentGlobalDetections)
	{
		qDebug() << "[Lost Child] Warning, Child: " << c.camTrackingId << " could not find any parent.";
	}

	emit updateGlobalCoordinate(_trackingResult, _totalObjectInFloorPlan);

	_isProceesingGlobalId = false;
	_forceTrace = false;


}

void TrackingManager::findNearestParent(QVector<ParentObject>& oldParent,
	QVector<ParentObject>& newParent,
	QHash<QString, ParentObject>& upgradedParent,
	QVector<CurrentDetectionInfo>& currentGlobalDetections)
{
	
	QVector<ParentObject> ugradedParentVector;
	for (auto p : upgradedParent)
	{
		ugradedParentVector.append(p);
	}
	
	QStringList oldParentIdToBeRemoved;

	for (auto it = currentGlobalDetections.begin(); it != currentGlobalDetections.end(); )
	{
		if (oldParent.isEmpty() && newParent.isEmpty()) {
			++it;
			continue; // no parents to compare
		}
		
		QPointF childCenter = it->globalRect.center();

		double minDist = std::numeric_limits<double>::max();
		QVector<ParentObject>::iterator nearestIt = oldParent.end(); // safe default
		bool foundAny = false;
		bool foundInOld = false;
		QString childCamId = it->camID;
		// --- Search in oldParent ---
		for (auto pIt = oldParent.begin(); pIt != oldParent.end(); ++pIt)
		{
		
			if (pIt->childCamIds.contains(childCamId)) continue;
			QPointF parentCenter(pIt->global_x + pIt->global_w / 2.0,
				pIt->global_y + pIt->global_h / 2.0);

			double dx = childCenter.x() - parentCenter.x();
			double dy = childCenter.y() - parentCenter.y();
			double dist = std::sqrt(dx * dx + dy * dy);

			if (dist < minDist)
			{
				minDist = dist;
				nearestIt = pIt;
				foundAny = true;
				foundInOld = true;
			}
		}

		// --- Search in upgraded ---
		for (auto pIt = ugradedParentVector.begin(); pIt != ugradedParentVector.end(); ++pIt)
		{

			
			if (pIt->childCamIds.contains(childCamId)) continue;

			QPointF parentCenter(pIt->global_x + pIt->global_w / 2.0,
				pIt->global_y + pIt->global_h / 2.0);

			double dx = childCenter.x() - parentCenter.x();
			double dy = childCenter.y() - parentCenter.y();
			double dist = std::sqrt(dx * dx + dy * dy);

			if (dist < minDist)
			{
				minDist = dist;
				nearestIt = pIt;
				foundAny = true;
				foundInOld = false;
			}
		}



		if (foundAny && minDist <= MAX_NEAREST_DIST) //  only use nearestIt if valid
		{
			QRectF unionRect = QRectF(nearestIt->global_x, nearestIt->global_y,
				nearestIt->global_w, nearestIt->global_h)
				.united(it->globalRect);

			QRectF newGlobalRect(unionRect.center().x() - (BOX_SIZE / 2.0),
				unionRect.center().y() - (BOX_SIZE / 2.0),
				BOX_SIZE,
				BOX_SIZE);

			

			if (foundInOld)
			{
				
				//ParentObject mergedParent = *nearestIt;
				//nearestIt = oldParent.erase(nearestIt);
				oldParentIdToBeRemoved.append(nearestIt->globalId);
				nearestIt->global_x = it->globalRect.x();
				nearestIt->global_y = it->globalRect.y();
				nearestIt->global_w = it->globalRect.width();
				nearestIt->global_h = it->globalRect.height();
				nearestIt->isSitting = it->isSitting;
				nearestIt->isTracking = true;
				nearestIt->lostTracKFrameCount = 0;
				nearestIt->isParent = true;
				nearestIt->childCamIds.append(childCamId);

				if (!nearestIt->parentIds.contains(it->camTrackingId))
					nearestIt->parentIds.append(it->camTrackingId);

				it = currentGlobalDetections.erase(it);
				//newParent.append(mergedParent);
			}
			else
			{
				bool foundP = false;
				for (auto& newP : newParent)
				{
					if (newP.globalId == nearestIt->globalId)
					{
						
						if (false)
						{
							newP.global_x = newGlobalRect.x();
							newP.global_y = newGlobalRect.y();
							newP.global_w = newGlobalRect.width();
							newP.global_h = newGlobalRect.height();
						}
						

						newP.isSitting = it->isSitting;
						newP.isTracking = true;
						newP.lostTracKFrameCount = 0;
						newP.isParent = true;
						newP.childCamIds.append(childCamId);


						if (!newP.parentIds.contains(it->camTrackingId))
						{
							newP.parentIds.append(it->camTrackingId);
						}


						qDebug() << "[FindNearestParent] New Parent(" << nearestIt->globalId << ") found new child(" << it->camTrackingId << ")";

						it = currentGlobalDetections.erase(it);
						foundP = true;


						break;
					}
				}
				if (!foundP) ++it;
				
			}

			
		}
		else
		{
			++it; // no parent found → keep child
		}
	}
	// Remove parents
	oldParent.erase(
		std::remove_if(oldParent.begin(), oldParent.end(),
			[&](const auto& parent) {
				if (oldParentIdToBeRemoved.contains(parent.globalId))
				{
					qDebug() << "[FindNearestParent] Old Parent(" << parent.globalId << ") upgraded due to nearest distance found, tracking ID: " << parent.parentIds;	
					ParentObject nParent = parent;
					newParent.append(nParent);
					return true;
				}
				else
				{
					return false;
				}

			}),
		oldParent.end());
}

void TrackingManager::lostTrackParentFindNearestChild(QVector<ParentObject>& oldParent,
	QVector<ParentObject>& newParent,
	QVector<CurrentDetectionInfo>& currentGlobalDetections)
{
	struct MatchDetail
	{
		CurrentDetectionInfo cDetectionInfo;
		double distance;
	};

	QVector<ParentObject> combinedParent;
	
	for (auto p : oldParent)
	{
		combinedParent.append(p);
	}

	QStringList oldParentIdToBeRemoved;
	QHash<QString, MatchDetail> matchParentChild; // key: parent globalId


	for (auto pIt = combinedParent.begin(); pIt != combinedParent.end(); ++pIt)
	{
		double minDist = MAX_LOSTPARENT_DIST;
		bool foundAny = false;
		for (auto& child : currentGlobalDetections)
		{
			QPointF childCenter = child.globalRect.center();
			QString childCamId = child.camID;

			if (pIt->childCamIds.contains(childCamId)) continue;

			QPointF parentCenter(pIt->global_x + pIt->global_w / 2.0,
				pIt->global_y + pIt->global_h / 2.0);

			double dx = childCenter.x() - parentCenter.x();
			double dy = childCenter.y() - parentCenter.y();
			double dist = std::sqrt(dx * dx + dy * dy);

			if (dist < minDist)
			{
				minDist = dist;
				MatchDetail mD;
				mD.cDetectionInfo = child;
				mD.distance = minDist;
				matchParentChild[pIt->globalId] = mD;
				foundAny = true;

			}
		}
	}

	// scan if theres same child assign to 2 diffeerent parent
	// Map childId -> (parentId, distance)
	QHash<QString, QPair<QString, double>> childAssignments;

	//qDebug() << "1. matchParentChild size() :" << matchParentChild.size();

	for (auto it = matchParentChild.begin(); it != matchParentChild.end(); ) {
		const QString parentId = it.key();
		const MatchDetail& mD = it.value();
		const QString childId = mD.cDetectionInfo.camTrackingId;

		if (childAssignments.contains(childId)) {
			// Already assigned, compare distances
			auto existing = childAssignments[childId];
			if (mD.distance < existing.second) {
				// New one is closer -> keep this, remove old parent
				matchParentChild.remove(existing.first);
				childAssignments[childId] = qMakePair(parentId, mD.distance);
				++it; // valid because we removed by different key
			}
			else {
				// Old one is closer -> remove this parent
				it = matchParentChild.erase(it);
			}
		}
		else {
			// First time assignment
			childAssignments[childId] = qMakePair(parentId, mD.distance);
			++it;
		}
	}

	//qDebug() << "2. matchParentChild size() :" << matchParentChild.size();

	QStringList childToBeRemoved;
	for (auto it = matchParentChild.constBegin(); it != matchParentChild.constEnd(); ++it) {
		const QString& parentGlobalId = it.key();    // key
		const CurrentDetectionInfo& child = it.value().cDetectionInfo;  // value
		const double& dist = it.value().distance;

		//qDebug() << "--Global ID: " << parentGlobalId << " Child ID: " << child.camTrackingId << " Dist: " << dist;

		for (auto pIt = oldParent.begin(); pIt != oldParent.end(); ++pIt)
		{
			if (pIt->globalId == parentGlobalId)
			{
				oldParentIdToBeRemoved.append(pIt->globalId);
				pIt->global_x = child.globalRect.x();
				pIt->global_y = child.globalRect.y();
				pIt->global_w = child.globalRect.width();
				pIt->global_h = child.globalRect.height();
				pIt->isSitting = child.isSitting;
				pIt->isTracking = true;
				pIt->lostTracKFrameCount = 0;
				pIt->isParent = true;
				pIt->childCamIds.append(child.camID);

				if (!pIt->parentIds.contains(child.camTrackingId))
					pIt->parentIds.append(child.camTrackingId);

				childToBeRemoved.append(child.camTrackingId);
				qDebug() << "[LostTrackRecovery] Parent(" << pIt->globalId << ") recovered with child ID: " << child.camTrackingId << " Dist(" << dist << ")";
			}
		}
	}

	// remove child
	currentGlobalDetections.erase(
		std::remove_if(currentGlobalDetections.begin(), currentGlobalDetections.end(),
			[&](const auto& child) {
				return childToBeRemoved.contains(child.camTrackingId);
			}),
		currentGlobalDetections.end());

	// Remove parents
	oldParent.erase(
		std::remove_if(oldParent.begin(), oldParent.end(),
			[&](const auto& parent) {
				if (oldParentIdToBeRemoved.contains(parent.globalId))
				{
					//qDebug() << "Old Parent(" << parent.globalId << ") upgraded due to nearest distance found, tracking ID: " << parent.parentIds;
					//qDebug() << "Parent cam Ids: " << parent.childCamIds;
					ParentObject nParent = parent;
					newParent.append(nParent);
					return true;
				}
				else
				{
					return false;
				}

			}),
		oldParent.end());
}

void TrackingManager::findNearestChild(QVector<ParentObject>& oldParent,
	QVector<ParentObject>& newParent,
	QHash<QString, ParentObject>& upgradedParent,
	QVector<CurrentDetectionInfo>& currentGlobalDetections)
{
	struct MatchDetail
	{
		CurrentDetectionInfo cDetectionInfo;
		double distance;
	};

	QVector<ParentObject> upgradedParentVec;
	QVector<ParentObject> combinedParent;
	for (auto p : upgradedParent)
	{
		upgradedParentVec.append(p);
		combinedParent.append(p);
	}
	for (auto p : oldParent)
	{
		combinedParent.append(p);
	}

	QStringList oldParentIdToBeRemoved;
	QHash<QString, MatchDetail> matchParentChild; // key: parent globalId
	
	
	for (auto pIt = combinedParent.begin(); pIt != combinedParent.end(); ++pIt)
	{
		double minDist = MAX_NEAREST_DIST;
		bool foundAny = false;
		for (auto& child : currentGlobalDetections)
		{
			QPointF childCenter = child.globalRect.center();
			QString childCamId = child.camID;

			if (pIt->childCamIds.contains(childCamId)) continue;

			QPointF parentCenter(pIt->global_x + pIt->global_w / 2.0,
				pIt->global_y + pIt->global_h / 2.0);

			double dx = childCenter.x() - parentCenter.x();
			double dy = childCenter.y() - parentCenter.y();
			double dist = std::sqrt(dx * dx + dy * dy);

			//qDebug() << "Global ID: " << pIt->globalId << " Child ID: " << child.camTrackingId << " Dist: " << dist;

			if (dist < minDist)
			{
				minDist = dist;
				MatchDetail mD;
				mD.cDetectionInfo = child;
				mD.distance = minDist;
				matchParentChild[pIt->globalId] = mD;
				foundAny = true;
				
			}
		}
	}
	
	// scan if theres same child assign to 2 diffeerent parent
	// Map childId -> (parentId, distance)
	QHash<QString, QPair<QString, double>> childAssignments;

	//qDebug() << "1. matchParentChild size() :" << matchParentChild.size();

	for (auto it = matchParentChild.begin(); it != matchParentChild.end(); ) {
		const QString parentId = it.key();
		const MatchDetail& mD = it.value();
		const QString childId = mD.cDetectionInfo.camTrackingId;

		if (childAssignments.contains(childId)) {
			// Already assigned, compare distances
			auto existing = childAssignments[childId];
			if (mD.distance < existing.second) {
				// New one is closer -> keep this, remove old parent
				matchParentChild.remove(existing.first);
				childAssignments[childId] = qMakePair(parentId, mD.distance);
				++it; // valid because we removed by different key
			}
			else {
				// Old one is closer -> remove this parent
				it = matchParentChild.erase(it);
			}
		}
		else {
			// First time assignment
			childAssignments[childId] = qMakePair(parentId, mD.distance);
			++it;
		}
	}

	//qDebug() << "2. matchParentChild size() :" << matchParentChild.size();

	QStringList childToBeRemoved;
	for (auto it = matchParentChild.constBegin(); it != matchParentChild.constEnd(); ++it) {
		const QString& parentGlobalId = it.key();    // key
		const CurrentDetectionInfo& child = it.value().cDetectionInfo;  // value
		const double& dist = it.value().distance;

		//qDebug() << "--Global ID: " << parentGlobalId << " Child ID: " << child.camTrackingId << " Dist: " << dist;
		
		for (auto pIt = oldParent.begin(); pIt != oldParent.end(); ++pIt)
		{
			if (pIt->globalId == parentGlobalId)
			{
				oldParentIdToBeRemoved.append(pIt->globalId);
				pIt->global_x = child.globalRect.x();
				pIt->global_y = child.globalRect.y();
				pIt->global_w = child.globalRect.width();
				pIt->global_h = child.globalRect.height();
				pIt->isSitting = child.isSitting;
				pIt->isTracking = true;
				pIt->lostTracKFrameCount = 0;
				pIt->isParent = true;
				pIt->childCamIds.append(child.camID);

				if (!pIt->parentIds.contains(child.camTrackingId))
					pIt->parentIds.append(child.camTrackingId);

				childToBeRemoved.append(child.camTrackingId);
				qDebug() << "[FindNearestChild] Old Parent(" << pIt->globalId << ") upgraded due to nearest distance found, tracking ID: " << child.camTrackingId << ")(Dist: " << dist << ")";;
			}
		}
		for (auto pIt = newParent.begin(); pIt != newParent.end(); ++pIt)
		{
			if (pIt->globalId == parentGlobalId)
			{
				if (upgradedParent.contains(pIt->globalId))
					upgradedParent.remove(pIt->globalId);
				//pIt->global_x = child.globalRect.x();
				//pIt->global_y = child.globalRect.y();
				//pIt->global_w = child.globalRect.width();
				//pIt->global_h = child.globalRect.height();
				pIt->isSitting = child.isSitting;
				pIt->isTracking = true;
				pIt->lostTracKFrameCount = 0;
				pIt->isParent = true;
				pIt->childCamIds.append(child.camID);

				if (!pIt->parentIds.contains(child.camTrackingId))
					pIt->parentIds.append(child.camTrackingId);

				childToBeRemoved.append(child.camTrackingId);
				qDebug() << "[FindNearestChild] New Parent(" << pIt->globalId << ") found new child(" << child.camTrackingId << ")(Dist: "<< dist << ")";
			}
		}
	}
	
	// remove child
	currentGlobalDetections.erase(
		std::remove_if(currentGlobalDetections.begin(), currentGlobalDetections.end(),
			[&](const auto& child) {
				return childToBeRemoved.contains(child.camTrackingId);
			}),
		currentGlobalDetections.end());

	// Remove parents
	oldParent.erase(
		std::remove_if(oldParent.begin(), oldParent.end(),
			[&](const auto& parent) {
				if (oldParentIdToBeRemoved.contains(parent.globalId))
				{
					//qDebug() << "Old Parent(" << parent.globalId << ") upgraded due to nearest distance found, tracking ID: " << parent.parentIds;
					//qDebug() << "Parent cam Ids: " << parent.childCamIds;
					ParentObject nParent = parent;
					newParent.append(nParent);
					return true;
				}
				else
				{
					return false;
				}

			}),
		oldParent.end());

	// !! POSSIBLE GOT CHILD FIND NO PARENT
}

void TrackingManager::processGlobalTracking(QVector<ParentObject>& trackingObjects)
{
	if (true)
	{
		// use byte track
		std::vector<Object> objects;

		QVector<ParentObject> sittingResults;
		for (const auto& r : trackingObjects) {


			Object obj;
			/*if (r.isSitting)
			{
				for (auto& t : _trackingResult)
				{
					if (t.globalId == r.globalId)
					{
						obj.rect = cv::Rect_<float>(t.global_x, t.global_y, t.global_w, t.global_h);
					}

				}


			}
			else
			{
				obj.rect = cv::Rect_<float>(r.global_x, r.global_y, r.global_w, r.global_h);
			}*/
			obj.rect = cv::Rect_<float>(r.global_x, r.global_y, r.global_w, r.global_h);
			obj.prob = 1;
			obj.label = 0;
			objects.push_back(obj);
		}

		std::vector<STrack> output_stracks = _tracker.update(objects);

		QVector<ParentObject> resultTrackingVector;

		// need to map backt the class id manually
		for (auto& o : output_stracks)
		{
			const auto& tlwh = o.tlwh;
			cv::Rect_<float> rect2(tlwh[0], tlwh[1], tlwh[2], tlwh[3]);


			float bestIou = 0.0f;
			QHash<QString, std::vector<OnnxResult> > bestLocalResult;
			QString bestObjectId;
			QHash<QString, int> bestCamTracking;
			bool bestIsTracking;


			for (auto& r : trackingObjects)
			{
				cv::Rect_<float> rect(r.global_x, r.global_y, r.global_w, r.global_h);

				float iouValue = computeIoU(rect, rect2);

				if (iouValue > bestIou)
				{
					bestIou = iouValue;
					bestIsTracking = r.isTracking;

				}
			}

			if (bestIou > 0.1f)  // optional threshold, or set to 0.0f if you want all matches
			{
				const auto& tlwh = o.tlwh;
				float x1 = tlwh[0];
				float y1 = tlwh[1];
				float x2 = tlwh[0] + tlwh[2];
				float y2 = tlwh[1] + tlwh[3];

				int tracking_id = o.track_id;




				ParentObject tObject;
				tObject.globalId = QString::number(tracking_id);
				tObject.global_x = x1;
				tObject.global_y = y1;
				tObject.global_w = x2 - x1;
				tObject.global_h = y2 - y1;



				resultTrackingVector.push_back(tObject);

			}
		}

		resultTrackingVector.append(sittingResults);
		trackingObjects = resultTrackingVector;
	}


	if (false)
	{
		QVector<ParentObject> newTrackingResult;
		if (_trackingResult.isEmpty()) return;
		for (auto& prevTrackedObject : _trackingResult) {
			QRectF prevRect(prevTrackedObject.global_x, prevTrackedObject.global_y,
				prevTrackedObject.global_w, prevTrackedObject.global_h);


			double bestIou = 0.0;
			int bestMatchIndex = -1;

			// Find the best matching current detection for this previous tracked object
			for (int i = 0; i < trackingObjects.size(); ++i) {
				// Only consider detections not yet matched to a previous track
				//if (trackingObjects[i].matchedToPrevious) continue;
				QRectF curRect(trackingObjects[i].global_x, trackingObjects[i].global_y,
					trackingObjects[i].global_w, trackingObjects[i].global_h);

				double iou = calculateIoU(prevRect, curRect);


				if (iou > bestIou && iou > IOU_THRESHOLD) {
					bestIou = iou;
					bestMatchIndex = i;
				}
			}

			if (bestMatchIndex != -1) {
				// A match was found! Update the previous tracked object.
				// We'll carry over the globalId and update its properties.
				ParentObject updatedObject = prevTrackedObject; // Start with the previous object's state


				const auto& matchedDetection = trackingObjects[bestMatchIndex];
				//if (matchedDetection.isSitting) continue;
				// Add the new local result
				//updatedObject.localResult[matchedDetection.camId]->push_back(matchedDetection.localResult);

				// Update global bounding box to the matched detection's box or a merged one
				// For simplicity, let's update it to the current detection's box.
				// A more advanced approach might use a weighted average or Kalman filter.

				//if (!matchedDetection.isSitting)
				if (true)
				{
					updatedObject.isSitting = matchedDetection.isSitting;
					updatedObject.global_x = matchedDetection.global_x;
					updatedObject.global_y = matchedDetection.global_y;
					updatedObject.global_w = matchedDetection.global_w;
					updatedObject.global_h = matchedDetection.global_h;


				}


				newTrackingResult.push_back(updatedObject);
				//currentGlobalDetections[bestMatchIndex].matchedToPrevious = true; // Mark as matched
			}
			else {
				qDebug() << "lost track";
				// No match found for this prevTrackedObject.
				// You might consider it "lost" or try to predict its position for a few frames.
				// For now, if no match, it's not added to newTrackingResult.
				// If you want to keep "lost" tracks for a grace period, you'd add it here
				// and decrement a "track_age" counter, removing it when it's too old.

				// can record the lost track iD
			}
		}
		trackingObjects = newTrackingResult;

	}

}


void TrackingManager::processCriteriaChecking(QVector<ParentObject>& parentResult)
{
	/*for (auto& p : parentResult)
	{
		QString globalId = p.globalId;
		for (auto pId : p.parentIds)
		{
			if (pId.split("[@]").size() == 2)
			{
				QString camID = pId.split("[@]")[0];
				QString localTrackingId = pId.split("[@]")[1];
				QRectF

				if (_localOdResult.contains(camID))
				{
					for (auto& oR : _localOdResult[camID])
					{

					}
				}
			}
		}
	}*/

	QStringList gloveIds_left;    // camId[@]trackingId
	QStringList gloveIds_right;    // camId[@]trackingId
	QStringList jumpsuitIds; // camId[@]trackingId

	for (auto it = _localTrackingResult.begin(); it != _localTrackingResult.end(); ++it)
	{
		const QString& camId = it.key();
		const std::vector<OnnxResult>& object_results = it.value();

		for (const auto& res : object_results)
		{
			QRectF resRect(res.x1, res.y1, res.x2 - res.x1, res.y2 - res.y1);

			// Compare against all other results (across all cams)
			for (auto it2 = _localOdResult.begin(); it2 != _localOdResult.end(); ++it2)
			{
				const QString& criteria_camId = it2.key();
				const std::vector<OnnxResult>& criteria_results = it2.value();

				for (const auto& crit : criteria_results)
				{
					//if (&res == &crit) continue; // skip same object

					QRectF critRect(crit.x1, crit.y1, crit.x2 - crit.x1, crit.y2 - crit.y1);
					// expand by 30 on each side
					critRect = critRect.adjusted(-50, -50, 50, 50);
					// Check if crit is inside res
					if (resRect.intersects(critRect) && camId == criteria_camId)
					{

						QString idString = criteria_camId + "[@]" + QString::number(res.tracking_id);



						if (crit.obj_id == 0) {

							cv::Point2f wristLeft = res.keypoints[LEFT_WRIST];
							QPointF wristPointLeft(wristLeft.x, wristLeft.y);

							cv::Point2f rightLeft = res.keypoints[RIGHT_WRIST];
							QPointF wristPointRight(rightLeft.x, rightLeft.y);
							// convert OpenCV Point2f -> Qt QPointF

							if (critRect.contains(wristPointLeft))
							{
								gloveIds_left.append(idString);
							}
							if (critRect.contains(wristPointRight))
							{
								gloveIds_right.append(idString);
							}


						}
						else if (crit.obj_id == 1) {

							cv::Point2f shouldertLeft = res.keypoints[LEFT_SHOULDER];
							QPointF shoulderPointLeft(shouldertLeft.x, shouldertLeft.y);

							cv::Point2f shoulderRight = res.keypoints[RIGHT_SHOULDER];
							QPointF shoulderPointRight(shoulderRight.x, shoulderRight.y);

							cv::Point2f hipRight = res.keypoints[RIGHT_HIP];
							QPointF hipPointRight(hipRight.x, hipRight.y);

							cv::Point2f hipLeft = res.keypoints[LEFT_HIP];
							QPointF hipPointLeft(hipLeft.x, hipLeft.y);

							if (critRect.contains(shoulderPointRight) && critRect.contains(hipPointRight) ||
								critRect.contains(shoulderPointLeft) && critRect.contains(hipPointLeft))
							{
								jumpsuitIds.append(idString);
							}

						}
					}
				}
			}
		}
	}

	for (ParentObject& parent : parentResult)
	{
		parent.hasGlove_left = false;
		parent.hasGlove_right = false;
		parent.hasJumpsuit = false;
		for (const QString& childId : parent.parentIds)
		{
			if (gloveIds_left.contains(childId)) {
				parent.hasGlove_left = true;
			}
			if (gloveIds_right.contains(childId)) {
				parent.hasGlove_right = true;
			}
			if (jumpsuitIds.contains(childId)) {
				parent.hasJumpsuit = true;
			}
		}
	}


	//for (ParentObject& parent : parentResult)
	//{
	//	qDebug() << "Parent ID: " << parent.globalId;
	//	qDebug() << "Glove: " << parent.hasGlove;
	//	//qDebug() << "Jumpsuit: " << parent.hasJumpsuit;
	//}
}
