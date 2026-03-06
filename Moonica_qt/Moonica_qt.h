#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Moonica_qt.h"
#include <opencv2/core/types.hpp>
#include <QFileDialog> 
#include <QPixmap>    
#include <QFile>
#include <QApplication>
#include <QSize>   
#include <QVector> 
#include <QDebug>
#include <QObject>
#include <QPropertyAnimation>                                
#include <QSlider>
#include <QPushButton>
#include <QComboBox>
#include <QStringList>
#include <QThread>
#include <QSettings>
#include <QShortcut>
#include <QLabel>
#include <QtCore>
#include <QtGui>
#include <QGraphicsOpacityEffect>
#include <opencv2/opencv.hpp>
#include <QMessageBox>
#include <QTimer>
#include <QWidget>
#include <QDesktopWidget>
#include <QSplashScreen>
#include <QMetaType>
#include <QProgressDialog>

#include <QGridLayout>
#include <QGraphicsPixmapItem>
#include <QGraphicsRectItem>
#include <QVideoWidget>
#include <QMediaPlayer>
#include <cmath>
#include <QLayout>

#include "QLineItem.h"
#include "QEllipseItem.h"
#include "QDragPolygon.h"

#include "QMainGraphicsView.h"
#include "QMainGraphicsScene.h"
#include "IDunification.h"
#include "VideoTest.h"
#include "Project.h"
#include "FrameGrabberThread.h"
#include "CommonStruct.h";
#include "FrameManager.h";
#include "RecorderThread.h";
#include "TrackingManager.h";
#include "TrackingStruct.h"
#include "UserAccount.h"

class VideoCapture;
class QVideoWidget;
class QMediaPlayer;
class QGraphicsScene;
class QGraphicsScene;
class QThread;




class Moonica_qt : public QMainWindow
{
    Q_OBJECT

public:
    Moonica_qt(QWidget* parent = nullptr);
    ~Moonica_qt();


private:
    Ui::Moonica ui;

    UserAccount* _userAccount;
    AccountInfo _curUserAccInfo;

    QStringList _colorPallete = {
        "#FF0028", "#FFAE00", "#76FF00", "#00FF60", "#00C5FF", "#1300FF", "#ED00FF", "#FF4200",
        "#FFB400", "#71FF00", "#00FF66", "#00C0FF", "#1900FF", "#F200FF", "#FF4D00", "#D8FF00",
        "#00FF6B", "#00BAFF", "#1E00FF", "#8B00FF", "#FF0018", "#FFBF00", "#66FF00", "#00FF70",
        "#00B5FF", "#2400FF", "#FD00FF", "#FF0013", "#FFC400", "#61FF00", "#00FF76", "#00AFFF",
        "#0043FF", "#9600FF", "#FF000E", "#FFC900", "#5BFF00", "#00FF7B", "#00AAFF", "#2F00FF",
        "#FF00F5", "#FF0003", "#FFD400", "#51FF00", "#00FF81", "#009FFF", "#3900FF", "#FF00EA",
        "#FF0100", "#FFDA00", "#4BFF00", "#00FF8B", "#002DFF", "#AC00FF", "#FF0700", "#FF7300",
        "#B2FF00", "#00FF91", "#0094FF", "#4400FF", "#B600FF", "#FF1200", "#FFE400", "#00FF96",
        "#008FFF", "#4F00FF", "#FF00D4", "#FF1700", "#FFEF00", "#36FF00", "#00FFA1", "#0084FF",
        "#5500FF", "#FF00CF", "#FF1C00", "#FFF500", "#30FF00", "#00FFA6", "#007EFF", "#5A00FF",
        "#C700FF",   "#FF9300", "#91FF00", "#FF0000", "#0000FF", "#FF00FF", "#000000",
        "#008000", "#808000", "#008080", "#C00000", "#0000C0", "#C000C0", "#C0C0C0", "#004000",
        "#404000", "#004040", "#202000", "#002020", "#606000", "#006060", "#A00000", "#0000A0",
        "#A000A0", "#A0A0A0", "#00E000", "#E0E000", "#00E0E0"
    };
    int _colorPalleteIndex = 0;
    enum WorkingPage {
        WORK_PAGE,
        CAM_TEST_PAGE,
        PROJECTS_PAGE,
        SETTING_PAGE,
        ACCOUNT_PAGE
    };

    enum ViewMode {
        CAM_VIEW,
        PERSPECTIVE_VIEW,
        FLOOR_VIEW
    };

    enum DrawingMode {
        SELECT_MODE,
        DRAW_MODE,
        DRAW_POLYGON_MODE,
        DRAW_QUAD_POLYGON_MODE
    };
    enum WorkingMode {
        CALIBRATION_MODE,
        ANALYSIS_MODE,
        ENTRANCE_EXIT_MODE
    };

    Project _curProject;

    WorkingMode _curWorkingMode = WorkingMode::CALIBRATION_MODE;
    DrawingMode _curDrawingMode = DrawingMode::SELECT_MODE;
    bool _maximizedState = true;

    QVector<int> _keyInFloorPoints;

    QPointF _startDragPos_camView;
    QPointF _endDragPos_camView;

    QPointF _startDragPos_floorView;
    QPointF _endDragPos_floorView;

    QMainGraphicsScene* m_scene_camView;
    QMainGraphicsScene* m_scene_floorView;
    QMainGraphicsScene* m_perspectiveViewScene;

    QGraphicsPixmapItem* m_pixmapItem_camView = nullptr;
    QGraphicsPixmapItem* m_pixmapItem_floorView;
    QGraphicsPixmapItem* m_pixmapItem_perspectiveView;

    QRectF _sceneBound_camView;
    QRectF _sceneBound_floorView;
    QRectF _sceneBound_perspectiveView;


    QVector<QGraphicsRectItem*> _sceneCamRectVector;
    QVector<QGraphicsRectItem*> _sceneFloorRectVector;
    QVector<QGraphicsPolygonItem*> _sceneFloor2PolyVector;

    cv::Mat _imgCam;
    cv::Mat _imgFloor;


    QVector <QEllipseItem*> _polyGrabber_camView;
    QVector <QLineItem*> _polyLine_camView;

    QVector <QEllipseItem*> _polyGrabber_floorView;
    QVector <QLineItem*> _polyLine_floorView;

    QVector <QDragPolygon*> _dragPolygonROI_camView;
    QVector <QDragPolygon*> _dragPolygonROI_floorView;
    QVector <QDragPolygon*> _dragPolygonEntranceExitRoi_camView;

    QVector <QGraphicsItemGroup*> _pointLabel_camView;
    QVector <QGraphicsItemGroup*> _pointLabel_floorView;

    QEllipseItem* _prevPolygonGrabber;
    QEllipseItem* _firstPolygonGrabber;
    QLineItem* _curPolygonLine;
    bool _startDrawPolygon = false;
    int _polygonGrabberCount_camView = 0;
    int _polygonGrabberCount_floorView = 0;
    bool _dragMode;


    QHash<QString, QTreeWidgetItem*> _childViewHash_workspace;
    ViewInfo _curView;

    void switchPageWithAnimation(WorkingPage index);
    bool _animationCompleted = true;

    bool openUserDialog(User_Dialog_Mode mode);
    void changeAccessMode();
    bool _isLogIn;

    cv::Mat QImage_to_cvMat(const QImage& image);
    QImage cvMat_to_QImage(const cv::Mat& mat);

    void setupGraphicsViews();
    void displayImageInScene(const QPixmap& pixmap, ViewMode viewMode);
    void clearAllView();
    void clearScene(QMainGraphicsScene* scene, QGraphicsPixmapItem*& item);
    void connectSignalAndSlot();

    void showWorkPage();
    void showCamTestPage();
    void showProjectsPage();
    void showSettingsPage();
    void showAccountPage();
    void clearFloor2Polygons();
    void clearPolygon(bool clearAll, ViewMode cView = CAM_VIEW);
    QEllipseItem* drawPolygonGrabber(QString id, const qreal& x, const qreal& y,
        const qreal& radiusX, const QColor& borderColor, const QColor& innerColor, ViewMode viewMode);
    QLineItem* drawPolygonLine(const QLineF& line,
        const QColor& color, const int& lineWidth, ViewMode viewMode);
    void removeSetupPolygon(ViewMode view);
    void addPolygonObject(ViewMode viewMode);

    void addDragbox(ViewMode viewMode);
    void setDragMode(bool flag);

    QPointF tranformPointToFloorView(QPointF pt, cv::Mat J);
    void deleteTree(QTreeWidgetItem* item);
    void openViewImage(QString viewName);
    bool openFloorImage();
    void updateViewHash();
    void openProject(QString recipeName);
    void openPerspectiveImage();

    void loadAppSetting(const QString& path);
    void saveAppSetting(const QString& path);
    void initStreamingThread();
    void clearStreamingThread();
    // recipe page
    void initProjectList();
    void addProject(QString projectName);
    void deleteProject(QString projectName);

    void initGifIcon();
    QMovie* _movieRecording;
    QMovie* _movieCctv;
   
    void clearGridLayout(QGridLayout* layout, QWidget* protectedWidget = nullptr);
   
    QVector<QPointF> getBirdEyePolygon(const cv::Mat& camImg, const cv::Mat& H, const cv::Mat& floorImg);
    
    // ----
    QVector<QCheckBox*> _camSelectionCheckBoxs;
    QWidget* _camSelectionContainer;
    FrameManager* _frameManager;
    QThread* _frameThread;

    TrackingManager* _trackManager;
    QThread* _trackingThread;

    RecorderThread _recorderThread;
    QHash<QString, CameraDisplay> _cameraDisplayHash;
    QHash<QString, FrameGrabberThread*> _frameGrabberHash;

    void addCameraStream(QString cameraId, QString cameraUrl, int camIndex);
    QWidget* createCameraWidget(const QString& cameraId, QGraphicsView* view);

    // === FOR UI SHOW INVIDUAL PPE COMPLIANCE OBJECT IMAGE ===
    QHash<QString, QImage> _objectImage; // key: global id, 
    QHash<QString, QStringList> _objectCamTrackingId; // key: global id, value: parent id
    QStringList _globalIDImageProcessed; // store rendered people object global id
    QHash<QString, QWidget*> _objectContainers;
    QHash<QString, QLabel*> _gloveLabels_left;
    QHash<QString, QLabel*> _gloveLabels_right;
    QHash<QString, QLabel*> _jumpsuitLabels;
    // === END ===
  
    QMap<QString, QString> _globalIdColor;

    //floorplan tracking
    QMainGraphicsScene* _floorPlanScene; // floor plan image at cam test
    QVector<QGraphicsEllipseItem*> _floorPlanEllipseItems;
    QVector<QGraphicsRectItem*> _floorPlanRectItems;
    QVector<QGraphicsTextItem*> _floorPlanTextItems;


    QHash<QString, QVector<QPointF>> _objectTrailPoints;
    const int MAX_TRAIL_POINTS = 500; // keep last 500 positions

    QHash<QString, QGraphicsRectItem*> _floorPlanRectMap;
    QHash<QString, QGraphicsEllipseItem*> _floorPlanCirMap;
    QHash<QString, QGraphicsEllipseItem*> _floorPlanEllipseMap;
    QHash<QString, QGraphicsTextItem*> _floorPlanTextMap;
    QHash<QString, QGraphicsPathItem*> _floorPlanTrailMap;
    QMap<QString, QGraphicsLineItem*> _floorPlanArrowLineMap;
    QMap<QString, QGraphicsPolygonItem*> _floorPlanArrowHeadMap;

    QHash<QString, QGraphicsEllipseItem*> _trackingDots;
    void updateTrackingDots(const std::vector<OnnxResult>& detections, int currentCameraId);
    void removeImageObject(QString globalId);
    std::vector<OnnxResult> _prevTrackingResult; // key: camId
    //global id
    std::shared_ptr<IDunification> _idManager;

    //settings start
    QThread* m_videoTestThread = nullptr;
    VideoTester* m_videoTester = nullptr;
    QTimer* m_urlValidationTimer = nullptr;
    void stopVideoTest();
   
    QThread* m_workspaceLiveFeedThread = nullptr;
    VideoTester* m_workspaceLiveFeed = nullptr; 
    QImage m_currentCaptureFrame;

    bool _globalStreamingReadyFlag = false;

    CheckingCriteria _checkingCriteria;
  
    double _fullVideoStreamSecond;
    void stopWorkspaceLiveFeed();
    //settings end

    void clearCamSelectionResources();
    void initCamSelection();
    void clearCamView();
    void camViewSelectOn(QString camID, bool isVideoMode = false);
    void camViewSelectOff(QString camID);

    void camViewSelectOn_all();
    void camViewSelectOff_all();


    void processObjectImage(const QVector<ParentObject>& trackingObjects);

    QColor getColorForID(int id);

    bool _isCamTestRecording = false;

    void initializeProjectSetting();

public slots:
    void switchWorkingMode(WorkingMode mode);
    void switchDrawingMode(DrawingMode mode);

    void maximizedWindow();
    void closeWindow();
    void maximize_restoreWindow();
    void graphicViewMousePress(ViewMode viewMode, QPointF pt, bool isLeftClick);
    void graphicViewMouseMove(ViewMode viewMode, QPoint pt);
    bool birdEyeCalibration(cv::Mat& homography);

    void onViewTreeContextMenu(const QPoint& point);
    void importNewViewImage();
    void resnapImage();
    void removeViewImage();
    void refreshViewInfoTreeWidget();
    void onViewChildItemClicked(QTreeWidgetItem* item, QTreeWidgetItem* prevItem);

   
  

    //coords
    void updateFloorPlanCoords(const QPoint& viewPos);

    //settings start
    void onUrlInputTimeout();
    void updateVideoTestDisplay(const QImage& image);
    void onVideoTestError(const QString& errorMessage);
    
    void updateWorkspaceLiveFeed(const QImage& image);
    void updateCameraGraphicView(QString cameraId, QImage frame,
        std::vector<OnnxResult> poseEstimation
        ,std::vector<OnnxResult> od,
        double currentSec);
    //settings end

    void updateGlobalCoordinate(const QVector<ParentObject>& trackingObjects, int numberOfPeople);
    void forceSetNumberFloorObject();
    void updateSingleViewResult(const QHash<QString, SingleViewParentObject>& singleViewParent,
        const QHash<QString, std::vector<OnnxResult>>& _localOdResult);

    void clearOverlayItems(CameraDisplay& cd);
    QRectF toRectF(const OnnxResult& r);
    void addBBox(CameraDisplay& cd,
        const QRectF& r,
        const QString& label,
        const QPen& pen,
        const QBrush& brush = Qt::NoBrush,
        const QColor bgRectColor = QColor(0, 0, 0, 160),
        const QColor txtColor = Qt::white);

    void drawKeypoints(CameraDisplay& cd, const OnnxResult& person,
        const QPen& pen, float minKptConf = 0.3f, qreal radius = 3.0);

    void drawSkeletonCOCO17(CameraDisplay& cd, const OnnxResult& person,
        const QPen& pen, float minKptConf = 0.3f);

    void recordCams();
    //void startRecording(CameraDisplay& camDisplay, const QString& filePath, int fps, cv::Size  frameSize);
    //void stopRecording(CameraDisplay& camDisplay);
    //void recordFrame(const QString& camId);

    QPointF findOutlier(const QVector<QPointF>& points, double k = 2.0);
    void startStreamVideo();
    void stopStreamVideo();

    void setAsDoorCamView();
    void removeDoorCamView();

    void loadOnnxModelsToCombo(
        QComboBox* combo,
        const QString& modelDir
    );

};
