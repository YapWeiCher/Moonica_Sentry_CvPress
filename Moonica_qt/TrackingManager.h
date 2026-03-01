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


// === helper ====
static inline cv::Rect2f rectFromXYXY(int x1, int y1, int x2, int y2)
{
	float fx1 = (float)std::min(x1, x2);
	float fy1 = (float)std::min(y1, y2);
	float fx2 = (float)std::max(x1, x2);
	float fy2 = (float)std::max(y1, y2);
	return cv::Rect2f(fx1, fy1, fx2 - fx1, fy2 - fy1);
}

static inline float iouRect(const cv::Rect2f& a, const cv::Rect2f& b)
{
	float x1 = std::max(a.x, b.x);
	float y1 = std::max(a.y, b.y);
	float x2 = std::min(a.x + a.width, b.x + b.width);
	float y2 = std::min(a.y + a.height, b.y + b.height);

	float iw = std::max(0.0f, x2 - x1);
	float ih = std::max(0.0f, y2 - y1);
	float inter = iw * ih;

	float uni = a.area() + b.area() - inter;
	return (uni > 0.0f) ? (inter / uni) : 0.0f;
}

static inline bool pointValid(const cv::Point2f& p)
{
	return std::isfinite(p.x) && std::isfinite(p.y) && p.x > 0.0f && p.y > 0.0f;
}

static inline bool rectValid(const cv::Rect2f& r)
{
	return r.width > 1.0f && r.height > 1.0f;
}

static inline cv::Rect2f clampRectTo(const cv::Rect2f& r, const cv::Rect2f& bound)
{
	float x1 = std::max(r.x, bound.x);
	float y1 = std::max(r.y, bound.y);
	float x2 = std::min(r.x + r.width, bound.x + bound.width);
	float y2 = std::min(r.y + r.height, bound.y + bound.height);
	if (x2 <= x1 || y2 <= y1) return cv::Rect2f();
	return cv::Rect2f(x1, y1, x2 - x1, y2 - y1);
}

static inline cv::Point2f rectCenter(const cv::Rect2f& r)
{
	return cv::Point2f(r.x + r.width * 0.5f, r.y + r.height * 0.5f);
}


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
	
	float computeAngle(const cv::Point2f& a, const cv::Point2f& b, const cv::Point2f& c);
	cv::Point2f CorrectAnklePoint(const OnnxResult& localBox ,bool left = true);
	cv::Point2f CorrectWristPoint(const OnnxResult& localBox, bool left = true);
	void transformSittingToStanding(OnnxResult& localBox);

    void setCriteria(CheckingCriteria checkingCriteria);


	bool checkHasMask(const OnnxResult& person, const std::vector<OnnxResult>& objDetections,
        QList<int> maskClassIds, float minObjConf = 0.30f, float minKptConf = 0.30f) const;

	bool checkHasGlove(const OnnxResult& person, const std::vector<OnnxResult>& objDetections,
        QList<int> gloveClassIds, float minObjConf = 0.30f, float minKptConf = 0.25f) const;

	bool checkHasShoe(const OnnxResult& person, const std::vector<OnnxResult>& objDetections,
        QList<int> shoeClassIds, float minObjConf = 0.30f, float minKptConf = 0.25f) const;

	bool checkHasSmock(const OnnxResult& person, const std::vector<OnnxResult>& objDetections,
		QList<int> smockClassIds, float minObjConf = 0.30f, float minKptConf = 0.25f) const;

private:

    // Helpers
    cv::Point2f getKpt(const OnnxResult& person, int idx, float minKptConf) const
    {
        if (idx < 0 || idx >= (int)person.keypoints.size()) return cv::Point2f(-1, -1);
        if (!person.keypoint_confidences.empty() && idx < (int)person.keypoint_confidences.size())
        {
            if (person.keypoint_confidences[idx] < minKptConf) return cv::Point2f(-1, -1);
        }
        return person.keypoints[idx];
    }

    cv::Rect2f faceRoi(const OnnxResult& person, float minKptConf) const
    {
        const cv::Rect2f personRect = rectFromXYXY(person.x1, person.y1, person.x2, person.y2);

        cv::Point2f nose = getKpt(person, Keypoint::NOSE, minKptConf);
        cv::Point2f le = getKpt(person, Keypoint::LEFT_EYE, minKptConf);
        cv::Point2f re = getKpt(person, Keypoint::RIGHT_EYE, minKptConf);
        cv::Point2f lEar = getKpt(person, Keypoint::LEFT_EAR, minKptConf);
        cv::Point2f rEar = getKpt(person, Keypoint::RIGHT_EAR, minKptConf);

        std::vector<cv::Point2f> pts;
        if (pointValid(nose)) pts.push_back(nose);
        if (pointValid(le)) pts.push_back(le);
        if (pointValid(re)) pts.push_back(re);
        if (pointValid(lEar)) pts.push_back(lEar);
        if (pointValid(rEar)) pts.push_back(rEar);

        if (pts.size() < 2)
        {
            // fallback: top part of person bbox
            cv::Rect2f r(personRect.x + personRect.width * 0.20f,
                personRect.y + personRect.height * 0.02f,
                personRect.width * 0.60f,
                personRect.height * 0.30f);
            return clampRectTo(r, personRect);
        }

        float minx = pts[0].x, maxx = pts[0].x;
        float miny = pts[0].y, maxy = pts[0].y;
        for (auto& p : pts) {
            minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
            miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
        }

        float w = std::max(2.0f, maxx - minx);
        float h = std::max(2.0f, maxy - miny);

        // Expand down to cover mouth/chin (where mask is)
        cv::Rect2f r(minx - 0.35f * w,
            miny - 0.25f * h,
            w + 0.70f * w,
            h + 2.20f * h);

        return clampRectTo(r, personRect);
    }

    cv::Rect2f handRoi(const OnnxResult& person, int wristIdx, int elbowIdx, float minKptConf) const
    {
        const cv::Rect2f personRect = rectFromXYXY(person.x1, person.y1, person.x2, person.y2);
        const cv::Point2f wrist = getKpt(person, wristIdx, minKptConf);
        const cv::Point2f elbow = getKpt(person, elbowIdx, minKptConf);

        if (!pointValid(wrist) && !pointValid(elbow))
            return cv::Rect2f();

        cv::Point2f center = pointValid(wrist) ? wrist : elbow;

        float armLen = 0.0f;
        if (pointValid(wrist) && pointValid(elbow))
            armLen = cv::norm(wrist - elbow);

        // Size heuristic: based on arm length or person bbox width
        float base = (armLen > 1.0f) ? armLen : (personRect.width * 0.12f);
        base = std::max(18.0f, base);

        cv::Rect2f r(center.x - 0.6f * base,
            center.y - 0.6f * base,
            1.2f * base,
            1.2f * base);

        return clampRectTo(r, personRect);
    }

    cv::Rect2f footRoi(const OnnxResult& person, int ankleIdx, int kneeIdx, float minKptConf) const
    {
        const cv::Rect2f personRect = rectFromXYXY(person.x1, person.y1, person.x2, person.y2);
        const cv::Point2f ankle = getKpt(person, ankleIdx, minKptConf);
        const cv::Point2f knee = getKpt(person, kneeIdx, minKptConf);

        if (!pointValid(ankle) && !pointValid(knee))
            return cv::Rect2f();

        cv::Point2f center = pointValid(ankle) ? ankle : knee;

        float legLen = 0.0f;
        if (pointValid(ankle) && pointValid(knee))
            legLen = cv::norm(ankle - knee);

        float base = (legLen > 1.0f) ? legLen : (personRect.width * 0.14f);
        base = std::max(20.0f, base);

        // Foot is slightly wider
        cv::Rect2f r(center.x - 0.7f * base,
            center.y - 0.45f * base,
            1.4f * base,
            0.9f * base);

        return clampRectTo(r, personRect);
    }

    cv::Rect2f torsoRoi(const OnnxResult& person, float minKptConf) const
    {
        const cv::Rect2f personRect = rectFromXYXY(person.x1, person.y1, person.x2, person.y2);

        cv::Point2f ls = getKpt(person, Keypoint::LEFT_SHOULDER, minKptConf);
        cv::Point2f rs = getKpt(person, Keypoint::RIGHT_SHOULDER, minKptConf);
        cv::Point2f lh = getKpt(person, Keypoint::LEFT_HIP, minKptConf);
        cv::Point2f rh = getKpt(person, Keypoint::RIGHT_HIP, minKptConf);

        std::vector<cv::Point2f> pts;
        if (pointValid(ls)) pts.push_back(ls);
        if (pointValid(rs)) pts.push_back(rs);
        if (pointValid(lh)) pts.push_back(lh);
        if (pointValid(rh)) pts.push_back(rh);

        if (pts.size() < 2)
        {
            // fallback: middle of bbox
            cv::Rect2f r(personRect.x + personRect.width * 0.15f,
                personRect.y + personRect.height * 0.20f,
                personRect.width * 0.70f,
                personRect.height * 0.45f);
            return clampRectTo(r, personRect);
        }

        float minx = pts[0].x, maxx = pts[0].x;
        float miny = pts[0].y, maxy = pts[0].y;
        for (auto& p : pts) {
            minx = std::min(minx, p.x); maxx = std::max(maxx, p.x);
            miny = std::min(miny, p.y); maxy = std::max(maxy, p.y);
        }

        float w = std::max(2.0f, maxx - minx);
        float h = std::max(2.0f, maxy - miny);

        // Expand a bit to cover shirt area
        cv::Rect2f r(minx - 0.25f * w,
            miny - 0.20f * h,
            w + 0.50f * w,
            h + 0.35f * h);

        return clampRectTo(r, personRect);
    }

    // Generic: check if any object of classId matches roi
    bool anyObjMatchesRoi(const std::vector<OnnxResult>& objs, QList<int> classIds,
        const cv::Rect2f& roi, float minObjConf,
        float minIou, bool useCenterInside) const
    {
        if (!rectValid(roi)) return false;

        for (const auto& o : objs)
        {
            if (!classIds.contains(o.obj_id)) continue;
            if (o.accuracy < minObjConf) continue;

            cv::Rect2f r = rectFromXYXY(o.x1, o.y1, o.x2, o.y2);
            if (!rectValid(r)) continue;

            bool centerInside = false;
            if (useCenterInside)
            {
                const auto c = rectCenter(r);
                centerInside = (c.x >= roi.x && c.x <= roi.x + roi.width &&
                    c.y >= roi.y && c.y <= roi.y + roi.height);
            }

            float iou = iouRect(roi, r);

            if ((useCenterInside && centerInside) || (iou >= minIou))
                return true;
        }
        return false;
    }

    // For gloves/shoes: count matches in left+right ROIs
    int countMatchesInRois(const std::vector<OnnxResult>& objs, QList<int> classIds,
        const std::vector<cv::Rect2f>& rois,
        float minObjConf, float minIou) const
    {
        int cnt = 0;
        for (const auto& roi : rois)
        {
            if (anyObjMatchesRoi(objs, classIds, roi, minObjConf, minIou, /*center*/true))
                cnt++;
        }
        return cnt;
    }

	
	Moonica _moonica;

	struct DoorCamObject
	{
		QString camId;
		QHash<int, QRectF> objectRectHash; // key: tracking id, value: coordinate
	};
	


	bool _forceTrace = false;
	int _totalObjectInFloorPlan = 0;
	bool _debugMode = false;


	bool _isProceesingGlobalId = false;
	QStringList _activeCameraList;
	QHash<QString, QVector<QPointF>> _doorPointHash; // key: camId;
	QHash<QString, cv::Mat>  _homographyMatrixHash; // key: camId;

	QHash<QString, std::vector<OnnxResult>> _localTrackingResult; // key: camId
	QHash<QString, std::vector<OnnxResult>> _localOdResult; // key: camId

	QVector<ParentObject> _trackingResult;
  
    CheckingCriteria _checkingCriteria;

	void runMoonicaApi();
	void setDebugMode(bool debugMode);

	void runSingleViewChecking();
	QHash<QString, SingleViewParentObject> _sViewParentObjHash;

public slots:
	void onResultReady(QString camId, const std::vector<OnnxResult>& poseEstimationResult ,const std::vector<OnnxResult>& odResult);


signals:
	void updateGlobalCoordinate(const QVector<ParentObject>& trackingObjects, int numberOfPeople);
    void updateSingleViewResult (const QHash<QString, SingleViewParentObject>& singleViewParent,
        const QHash<QString, std::vector<OnnxResult>>& _localOdResult);
};


