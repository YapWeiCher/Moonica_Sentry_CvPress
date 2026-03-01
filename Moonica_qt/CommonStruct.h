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

};

//const std::unordered_map<int, std::string> classNames = {
//   {0, "person"}, {1, "bicycle"}, {2, "car"}, {3, "motorcycle"}, {4, "airplane"},
//   {5, "bus"}, {6, "train"}, {7, "truck"}, {8, "boat"}, {9, "traffic light"},
//   {10, "fire hydrant"}, {11, "stop sign"}, {12, "parking meter"}, {13, "bench"},
//   {14, "bird"}, {15, "cat"}, {16, "dog"}, {17, "horse"}, {18, "sheep"},
//   {19, "cow"}, {20, "elephant"}, {21, "bear"}, {22, "zebra"}, {23, "giraffe"},
//   {24, "backpack"}, {25, "umbrella"}, {26, "handbag"}, {27, "tie"}, {28, "suitcase"},
//   {29, "frisbee"}, {30, "skis"}, {31, "snowboard"}, {32, "sports ball"}, {33, "kite"},
//   {34, "baseball bat"}, {35, "baseball glove"}, {36, "skateboard"}, {37, "surfboard"},
//   {38, "tennis racket"}, {39, "bottle"}, {40, "wine glass"}, {41, "cup"}, {42, "fork"},
//   {43, "knife"}, {44, "spoon"}, {45, "bowl"}, {46, "banana"}, {47, "apple"},
//   {48, "sandwich"}, {49, "orange"}, {50, "broccoli"}, {51, "carrot"}, {52, "hot dog"},
//   {53, "pizza"}, {54, "donut"}, {55, "cake"}, {56, "chair"}, {57, "couch"},
//   {58, "potted plant"}, {59, "bed"}, {60, "dining table"}, {61, "toilet"}, {62, "tv"},
//   {63, "laptop"}, {64, "mouse"}, {65, "remote"}, {66, "keyboard"}, {67, "cell phone"},
//   {68, "microwave"}, {69, "oven"}, {70, "toaster"}, {71, "sink"}, {72, "refrigerator"},
//   {73, "book"}, {74, "clock"}, {75, "vase"}, {76, "scissors"}, {77, "teddy bear"},
//   {78, "hair drier"}, {79, "toothbrush"}
//};

const std::unordered_map<int, std::string> classNames = {
   {0, "Smock_Green"}, {1, "Smock_Pink"},  {2, "Smock_White"},  {3, "Smock_Dark_Blue"},  {4, "Smock_Light_Blue"}
   ,  {5, "Glove"},  {6, "Mask"},  {7, "Hair_Net"},  {8, "Shoe_Cover"},{9, "Smock_Brown"}, {10,"ESD_Shoe"}
   ,  {11, "Safety_shoe"}
};

//const std::unordered_map<int, std::string> classNames = {
//   {0, "Fire"}, {1, "Smoke"}
//};

//struct ParentObject
//{
//    QString globalId;
//    //QHash<QString, std::vector<OnnxResult> > localResult;
//
//    QString dominantTrackingId;
//
//    double global_x, global_y, global_w, global_h;
//
//    bool isTracking = false; // tracking false = lost track
//    int lostTracKFrameCount = 0;
//    bool isSitting = false;
//
//    bool isParent = false;
//    QStringList parentIds; // this record the cam[@]<localId>
//
//    // this will be erased for each processing
//    // one parent cannto have two same cam tracking id attached to it
//    QStringList childCamIds; 
//
//    bool hasJumpsuit = false;
//    bool hasGlove_left = false;
//    bool hasGlove_right = false;
//};
//
//struct CurrentDetectionInfo //raw global rect, no combine no iou
//{
//    QRectF globalRect;
//    QString camID;
//    QString camTrackingId; // camId[@]TrackingID
//    bool matchedToPrevious = false; 
//    bool isSitting = false;
//};
//
//enum Keypoint {
//    NOSE = 0,             // Tip of the nose (face center)
//    LEFT_EYE = 1,         // Center of left eye
//    RIGHT_EYE = 2,        // Center of right eye
//    LEFT_EAR = 3,         // Base of left ear
//    RIGHT_EAR = 4,        // Base of right ear
//    LEFT_SHOULDER = 5,    // Shoulder joint (left)
//    RIGHT_SHOULDER = 6,   // Shoulder joint (right)
//    LEFT_ELBOW = 7,       // Elbow joint (left arm)
//    RIGHT_ELBOW = 8,      // Elbow joint (right arm)
//    LEFT_WRIST = 9,       // Wrist joint (left hand)
//    RIGHT_WRIST = 10,     // Wrist joint (right hand)
//    LEFT_HIP = 11,        // Hip joint (left leg)
//    RIGHT_HIP = 12,       // Hip joint (right leg)
//    LEFT_KNEE = 13,       // Knee joint (left leg)
//    RIGHT_KNEE = 14,      // Knee joint (right leg)
//    LEFT_ANKLE = 15,      // Ankle joint (left foot)
//    RIGHT_ANKLE = 16      // Ankle joint (right foot)
//};


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

