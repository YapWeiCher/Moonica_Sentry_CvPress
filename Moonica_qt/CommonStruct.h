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

