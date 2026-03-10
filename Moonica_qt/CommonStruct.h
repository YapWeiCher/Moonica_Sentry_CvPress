#pragma once

#include "FrameGrabberThread.h"

#include <opencv2/opencv.hpp>
#include "QMainGraphicsView.h"
#include "QMainGraphicsScene.h"
#include "OnnxInference.h"
#include "TrackingStruct.h"
#include <QGraphicsLineItem>
#include <QGraphicsEllipseItem>

//multi cam structure
struct SingleViewParentObject
{
    QString globalId;
    OnnxResult trackingResult;
    QString camId;

    int hasFaceMaskFrame = 0;
    int hasGloveFrame = 0;
    int hasEsdShoeFrame = 0;
    int hasSmockFrame = 0;
  

    bool hasFacemask = false;
    bool hasGlove = false;
    bool hasEsdShoe = false;
    bool hasSmock = false;
   

    bool isPass;

    int lostTrackFrame = 0;

    // reset every frame
    bool isTracking = true;
    bool inCheckingArea = false;

    //
    int totalFailFrame = 0;
    bool failRecordWritten = false;
    //


};
// chart 
struct CleaningDashboardRecord
{
    QString id;
    QDateTime towerTriggeringTime;
    double triggerToStartCleaningDuration = 0.0;
    double cleaningDuration = 0.0;
    double totalDuration = 0.0;
};

struct CameraStream
{
    int id;
    QString camId;
    QString url;
    bool isActive = false;

};

struct CameraDisplay
{
    QImage frame;

    QMainGraphicsView* camView = nullptr;
    QMainGraphicsScene* camScene = nullptr;
    QGraphicsPixmapItem* camPixmapItem = nullptr;
    QVector<QGraphicsRectItem*> rectItems;
    QVector<QGraphicsTextItem*> textItems;
    QVector<QGraphicsRectItem*> textBgItems;

    QVector<QGraphicsLineItem*> lineItems;
    QVector<QGraphicsEllipseItem*> ellipseItems;

    QVector<QGraphicsPolygonItem*> polygonItems;

    
    QHash<QString, SingleViewParentObject> singleParentObjHash;

    TowerLightColor towerLightColor;
};






enum User_Dialog_Mode {
    LOGIN,
    CREATE_ACCOUNT
};


enum AccessLevel {
    ADMIN,
    ENGINEER,
    OPERATOR
};

struct AccountInfo {
    QString userID;
    QString userName;
    QString password;
    AccessLevel accessLevel = AccessLevel::OPERATOR;
};

enum CleaningStatus 
{
    CLEANING_START,
    CLEANING_END
};

struct CleaningResult
{
    CleaningStatus cleaningStatus = CleaningStatus::CLEANING_END;
    QString id;
    bool isCleaning = false;

    QString towerTriggeringTime;
    float triggerToStartCleaningDuration =-1;
    QString cleaningTime;
    float cleaningDuration =-1;
    float totalDuration =-1;

    bool isCompleteCleaning = false;
   
};

