#include "Moonica_qt.h"


Moonica_qt::Moonica_qt(QWidget* parent)
	: QMainWindow(parent),
	m_scene_camView(nullptr), m_scene_floorView(nullptr), m_perspectiveViewScene(nullptr), m_pixmapItem_camView(nullptr),
	m_pixmapItem_floorView(nullptr), m_pixmapItem_perspectiveView(nullptr)
{
	


	QSplashScreen* splash = new QSplashScreen;
	splash->setPixmap(QPixmap(":/ResultViewer/Icon/NVsionLogo.png"));
	splash->show();
	qApp->processEvents(); // allow splash to repaint

	_idManager = std::make_shared<IDunification>();
	ui.setupUi(this);


	PathManager::setAllPath();

	_userAccount = new UserAccount();
	_userAccount->setModal(true);

	qDebug() << "Setting up user dialog...";
	_isLogIn = openUserDialog(User_Dialog_Mode::LOGIN);
	if (!_isLogIn) return;




	// Create a container widget for the scroll area
	QWidget* scrollContainer = new QWidget();
	QGridLayout* gridLayout = new QGridLayout(scrollContainer);
	gridLayout->setSpacing(5);
	gridLayout->setContentsMargins(5, 5, 5, 5);

	// Assign this container to the scroll area
	ui.scrollArea_cameraView->setWidget(scrollContainer);
	ui.scrollArea_cameraView->setWidgetResizable(true);

	// Keep reference so we can reuse it later
	ui.scrollArea_cameraView->setProperty("gridLayout", QVariant::fromValue((void*)gridLayout));


	m_urlValidationTimer = new QTimer(this);
	m_urlValidationTimer->setSingleShot(true); //fires once per timeout 
	m_urlValidationTimer->setInterval(500);

	connect(m_urlValidationTimer, &QTimer::timeout, this, &Moonica_qt::onUrlInputTimeout);
	//settings end

	_floorPlanScene = new QMainGraphicsScene(this);
	ui.graphicsView_viewFloorPlan2->setScene(_floorPlanScene);


	setupGraphicsViews();
	connectSignalAndSlot();
	initGifIcon();

	ui.toolButton_calibrationMode->click();
	ui.toolButton_select->click();
	splash->close();

	loadAppSetting(PathManager::_appSettingPath);

	

	if (_curProject._projectName.isEmpty()) showProjectsPage();
}

Moonica_qt::~Moonica_qt()
{
	clearCamSelectionResources();
	clearStreamingThread();
	stopVideoTest();
	stopWorkspaceLiveFeed();
	_curProject.saveProject();
	saveAppSetting(PathManager::_appSettingPath);
}


void Moonica_qt::switchPageWithAnimation(WorkingPage index)
{
	if (!_animationCompleted) return;
	_animationCompleted = false;
	QWidget* currentPage = ui.stackedWidget_mainBody->widget(ui.stackedWidget_mainBody->currentIndex());
	QWidget* newPage = ui.stackedWidget_mainBody->widget((int)index);

	int fadeTime = 200;


	QGraphicsOpacityEffect* effOut = new QGraphicsOpacityEffect();
	currentPage->setGraphicsEffect(effOut);
	QPropertyAnimation* fadeOut = new QPropertyAnimation(effOut, "opacity");
	fadeOut->setDuration(fadeTime);
	fadeOut->setStartValue(1.0);
	fadeOut->setEndValue(0.0);
	fadeOut->setEasingCurve(QEasingCurve::OutBack);

	QGraphicsOpacityEffect* effIn = new QGraphicsOpacityEffect();
	if (ui.stackedWidget_mainBody->currentIndex() != (int)index) newPage->setGraphicsEffect(effIn);
	QPropertyAnimation* fadeIn = new QPropertyAnimation(effIn, "opacity");
	fadeIn->setDuration(fadeTime);
	fadeIn->setStartValue(0.0);
	fadeIn->setEndValue(1.0);
	fadeIn->setEasingCurve(QEasingCurve::InBack);

	connect(fadeOut, &QPropertyAnimation::finished, this, [=]() {
		if (ui.stackedWidget_mainBody->currentIndex() == (int)index)
		{
			newPage->setGraphicsEffect(nullptr);
			newPage->setGraphicsEffect(effIn);

			fadeIn->start();
		}
		else
		{
			ui.stackedWidget_mainBody->setCurrentIndex((int)index);
			fadeIn->start();
		}
		_animationCompleted = true;
		//ui.graphicsView_fullImg->fitInView(_pPixmapItemFull, Qt::KeepAspectRatio);
		});

	fadeOut->start();

}

bool Moonica_qt::openUserDialog(User_Dialog_Mode mode)
{
	_userAccount->setMode(mode, _curUserAccInfo.accessLevel);
	int returnValue = _userAccount->exec();
	if (returnValue == QDialog::Accepted)
	{

		qDebug() << "Accept";
		_curUserAccInfo = _userAccount->getCurrentAccount();
		QString accessLevel;
		if (_curUserAccInfo.accessLevel == AccessLevel::ADMIN) accessLevel = "Admin";
		if (_curUserAccInfo.accessLevel == AccessLevel::ENGINEER) accessLevel = "Engineer";
		if (_curUserAccInfo.accessLevel == AccessLevel::OPERATOR) accessLevel = "Operator";

		//ui.label_accessLevel->setText(accessLevel);
		changeAccessMode();
		return true;
	}
	else if (returnValue == QDialog::Rejected)
	{
		if (mode == User_Dialog_Mode::LOGIN)
		{
			qDebug() << "quit";
			this->closeWindow();
			return false;
		}

	}
}

void Moonica_qt::changeAccessMode()
{
	if (_curUserAccInfo.accessLevel == AccessLevel::OPERATOR)
	{
		ui.pushButton_createAcc->hide();
	}
	else
	{
		ui.pushButton_createAcc->show();
	}
}

void Moonica_qt::initGifIcon()
{
	qDebug() << "initGifIcon";
	_movieCctv = new QMovie(":/ResultViewer/Icon/cctv-ezgif.com-gif-maker.gif");
	_movieRecording = new QMovie(":/ResultViewer/Icon/icons8-recording-1.gif");


	connect(_movieCctv, &QMovie::frameChanged, this, [=](int frame) {
		ui.toolButton_movieCctv->setIcon(QIcon(_movieCctv->currentPixmap()));
		});
	connect(_movieRecording, &QMovie::frameChanged, this, [=](int frame) {
		ui.toolButton_recordMovie->setIcon(QIcon(_movieRecording->currentPixmap()));
		});

	// if movie doesn't loop forever, force it to.
	if (_movieCctv->loopCount() != -1) connect(_movieCctv, SIGNAL(finished()), _movieCctv, SLOT(start()));
	if (_movieRecording->loopCount() != -1) connect(_movieRecording, SIGNAL(finished()), _movieRecording, SLOT(start()));

	_movieCctv->start();

}

void Moonica_qt::initStreamingThread()
{
	_frameThread = new QThread(this);
	_frameManager = new FrameManager();
	_frameManager->moveToThread(_frameThread);

	qRegisterMetaType<std::vector<OnnxResult>>("std::vector<OnnxResult>");
	connect(_frameManager, &FrameManager::updateCameraGraphicView,
		this, &Moonica_qt::updateCameraGraphicView,
		Qt::QueuedConnection);

	connect(_frameManager, &FrameManager::frameReadyRecord,
		&_recorderThread, &RecorderThread::enqueueFrame,
		Qt::QueuedConnection);

	connect(_frameThread, &QThread::finished, _frameManager, &QObject::deleteLater);

	_trackingThread = new QThread(this);
	_trackManager = new TrackingManager();
	_trackManager->moveToThread(_trackingThread);

	connect(_frameManager, &FrameManager::frameReadyTracking,
		_trackManager, &TrackingManager::onResultReady,
		Qt::QueuedConnection);

	qRegisterMetaType<QVector<ParentObject>>("QVector<ParentObject>");
	connect(_trackManager, &TrackingManager::updateGlobalCoordinate,
		this, &Moonica_qt::updateGlobalCoordinate,
		Qt::QueuedConnection);

	qRegisterMetaType< QHash<QString, SingleViewParentObject>>("QHash<QString, SingleViewParentObject>");
	qRegisterMetaType< QHash<QString, std::vector<OnnxResult>>>("QHash<QString, std::vector<OnnxResult>>");
	connect(_trackManager, &TrackingManager::updateSingleViewResult,
		this, &Moonica_qt::updateSingleViewResult,
		Qt::QueuedConnection);

	connect(_trackingThread, &QThread::finished, _trackManager, &QObject::deleteLater);

	// Start processing thread
	_frameThread->start();
	_trackingThread->start();
}

void Moonica_qt::clearStreamingThread()
{
	 //---------- Stop frame thread ----------
	if (_frameThread)
	{
		qDebug() << "[Cleanup] Stopping frame thread...";
		_frameThread->quit();
		_frameThread->wait();
		_frameThread->deleteLater();
		_frameThread = nullptr;
		qDebug() << "[Cleanup] Frame thread stopped";
	}
	else
	{
		qDebug() << "[Cleanup] Frame thread already null";
	}

	qDebug() << "[Cleanup] FrameManager released";
	_frameManager = nullptr; // deleted via deleteLater

	// ---------- Stop tracking thread ----------
	if (_trackingThread)
	{
		qDebug() << "[Cleanup] Stopping tracking thread...";
		_trackingThread->quit();
		_trackingThread->wait();
		_trackingThread->deleteLater();
		_trackingThread = nullptr;
		qDebug() << "[Cleanup] Tracking thread stopped";
	}
	else
	{
		qDebug() << "[Cleanup] Tracking thread already null";
	}

	qDebug() << "[Cleanup] TrackingManager released";
	_trackManager = nullptr;
}

void Moonica_qt::loadAppSetting(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qWarning() << "Could not open file for reading:" << path;
		return;
	}

	QByteArray jsonData = file.readAll();
	file.close();

	QJsonParseError parseError;
	QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

	if (parseError.error != QJsonParseError::NoError)
	{
		qWarning() << "JSON parse error:" << parseError.errorString();
		return;
	}

	if (!jsonDoc.isObject())
	{
		qWarning() << "Invalid JSON format: root is not an object";
		return;
	}

	QJsonObject rootObj = jsonDoc.object();

	QString projectName = rootObj.value("Project_Name").toString();


	if (!projectName.isEmpty())openProject(projectName);


}


void Moonica_qt::saveAppSetting(const QString& path)
{
	QJsonDocument jsonDoc;
	QJsonObject rootObj;

	rootObj.insert(QStringLiteral("Project_Name"), _curProject._projectName);

	jsonDoc.setObject(rootObj);
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		qWarning() << "Could not open file for writing:" << path;
		return;
	}
	file.write(jsonDoc.toJson(QJsonDocument::Indented)); // Indented for readability
	file.close();
}

void Moonica_qt::connectSignalAndSlot()
{

	


	//settings start
		//cap image button
	connect(ui.pushButton_captureImage, &QPushButton::clicked, this, [this]() {

		if (m_currentCaptureFrame.isNull())
		{
			QMessageBox::warning(this, "Capture Failed", "No valid frame received from cctv");
			return;
		}

		// Define a file path for the new image inside the project's CamViews folder
		QString viewName = _curView.viewName;
		QString imagePath = PathManager::_camViewsDir + "/" + viewName + ".png";
		if (m_currentCaptureFrame.save(imagePath, "PNG"))
		{
			qDebug() << "Captured image saved to:" << imagePath;
			stopWorkspaceLiveFeed();

			//update viewinfo and save projfect
			_curProject._viewInfoHash[viewName].viewPath = imagePath;
			_curProject._viewInfoHash[viewName].extension = "png";
			_curProject.saveProject();

			//refreash display show image
			openViewImage(viewName);
		}
		});

	//cctv preview 
	connect(ui.lineEdit_cctvUrl, &QLineEdit::textChanged, this, [this](const QString& text)
		{
			stopVideoTest();
			ui.label_cctvFeedPreview->setText("Validating");

			if (text.isEmpty())
			{
				m_urlValidationTimer->stop();
				ui.label_cctvFeedPreview->setText("CCTV Preview");
			}
			else
			{
				m_urlValidationTimer->start();
			}
		});

	//dialog apply/cancel
	connect(ui.buttonBox_cctvSettings, &QDialogButtonBox::clicked, this, [this](QAbstractButton* button) {

		if (ui.buttonBox_cctvSettings->buttonRole(button) == QDialogButtonBox::ApplyRole)
		{
			QString cctvName = ui.lineEdit_cctvName->text();
			QString cctvUrl = ui.lineEdit_cctvUrl->text();

			if (cctvName.isEmpty() || cctvUrl.isEmpty())
			{
				QMessageBox::warning(this, "Error", "CCTV Name or URL invalid");
				return;
			}
			if (_curProject._viewInfoHash.contains(cctvName))
			{
				QMessageBox::warning(this, "Error", "CCTV exists in project");
				return;
			}

			ViewInfo newCameraInfo;
			newCameraInfo.viewName = cctvName;
			newCameraInfo.cctvUrl = cctvUrl;
			_curProject._viewInfoHash.insert(cctvName, newCameraInfo);

			//sync new cam list to Cam Test page
			_curProject._viewList = _curProject._viewInfoHash.values();

			//update spin box with cam count

			refreshViewInfoTreeWidget(); //update ui first
			_curProject.saveProject(); //then save
			_curProject.loadProject(_curProject._projectName); //then load again 

			qDebug() << "View List Size: " << _curProject._viewList.size();

			QMessageBox::information(this, "Success", QString("Camera '%1' has been added to the project.").arg(cctvName));
			stopVideoTest();
			ui.lineEdit_cctvName->clear();
			ui.lineEdit_cctvUrl->clear();
		}

		else if (ui.buttonBox_cctvSettings->buttonRole(button) == QDialogButtonBox::RejectRole || ui.buttonBox_cctvSettings->buttonRole(button) == QDialogButtonBox::ResetRole)
		{
			stopVideoTest();
			ui.lineEdit_cctvName->clear();
			ui.lineEdit_cctvUrl->clear();
		}
		});
	//Settings end

	ui.treeWidget_views->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.treeWidget_views, &QTreeWidget::customContextMenuRequested, this, &Moonica_qt::onViewTreeContextMenu);
	connect(ui.actionImport_Cam_Views, SIGNAL(triggered()), this, SLOT(importNewViewImage()));
	connect(ui.actionRemove_Image, SIGNAL(triggered()), this, SLOT(removeViewImage()));
	connect(ui.actionResnap_Image, SIGNAL(triggered()), this, SLOT(resnapImage()));

	connect(ui.actionSetAsDoorCam, SIGNAL(triggered()), this, SLOT(setAsDoorCamView()));
	connect(ui.actionRemoveDoorCam, SIGNAL(triggered()), this, SLOT(removeDoorCamView()));

	connect(ui.treeWidget_views, &QTreeWidget::currentItemChanged, this, &Moonica_qt::onViewChildItemClicked);

	connect(ui.toolButton_minimize, &QToolButton::clicked, this, [=]()
		{
			showMinimized();
		});

	connect(ui.toolButton_maximize_restore, SIGNAL(clicked()), this, SLOT(maximize_restoreWindow()));
	connect(ui.toolButton_close, SIGNAL(clicked()), this, SLOT(closeWindow()));
	connect(ui.toolButton_calibrate, &QToolButton::clicked, this, [this]() {

		cv::Mat homography;
		if (birdEyeCalibration(homography))
		{
			_curView.isCalibrated = true;
			_curView.homographyMatrix = homography;
			_curView.perspectivePointVec = getBirdEyePolygon(_imgCam, homography, _imgFloor);

			updateViewHash();
			openPerspectiveImage();


		}
		else
		{
			QMessageBox::warning(this, "Homogrpahy Calibration Failed", "Could not compute homography. Ensure your points are not in a line and are selected in the correct order.");
		}
		});

	

	QShortcut* shortcut_1 = new QShortcut(QKeySequence(Qt::Key_1), this);
	connect(shortcut_1, &QShortcut::activated, [=]() {
		qDebug() << "Snap cam1";
	/*	CAMManager::instance().softTrigger("cam1");
		CAMManager::instance().waitAcquisition("cam1", 5000);
		auto frame = CAMManager::instance().frame("cam1");*/
		//cv::imwrite("C:/Git2/Moonica-Sentry-Maruwa/test.jpg", frame->frame);
		});

	QShortcut* shortcut_2 = new QShortcut(QKeySequence(Qt::Key_2), this);
	connect(shortcut_2, &QShortcut::activated, [=]() {

		});
	QShortcut* shortcut_dlt = new QShortcut(QKeySequence(Qt::Key_Delete), this);
	connect(shortcut_dlt, &QShortcut::activated, [=]() {

		});


	connect(ui.pushButton_laodImg2, &QPushButton::clicked, this, [this]()
		{
			QString fileName = QFileDialog::getOpenFileName(
				this,
				tr("Open Image"),
				QString(),
				tr("Images (*.png *.jpg *.jpeg *.bmp)")
			);

			QPixmap image(fileName);

			_floorPlanScene->clear();
			_floorPlanScene->addPixmap(image);
			ui.graphicsView_viewFloorPlan2->fitInView(_floorPlanScene->itemsBoundingRect());



			if (!fileName.isEmpty())
			{

				QFileInfo fImageInfo(fileName);
				QString extention = fImageInfo.completeSuffix();

				QString destinationPath = PathManager::_floorViewsDir + "FloorPlan." + extention;
				QString floorViewInfoPath = PathManager::_floorViewsDir + "FloorPlanInfo.json";

				if (QFileInfo::exists(destinationPath))QFile::remove(destinationPath);
				if (QFileInfo::exists(floorViewInfoPath))QFile::remove(floorViewInfoPath);

				QFile::copy(fileName, destinationPath);



				ViewInfo vInfo;
				vInfo.viewName = "FloorPlan";
				vInfo.viewPath = destinationPath;
				vInfo.extension = extention;
				vInfo.isCalibrated = false;
				_curProject._floorViewInfo = vInfo;

				_curProject.saveProject();
				clearFloor2Polygons();
				openFloorImage();
			}
		});

	QObject::connect(ui.toolButton_resetPointsCam, &QToolButton::clicked, [&]() {

	/*	for (auto b : _pointLabel_camView)
		{
			m_scene_camView->removeItem(b);
			delete b;
		}
		_pointLabel_camView.clear();*/

		clearPolygon(false, CAM_VIEW);

		if (_curWorkingMode == WorkingMode::CALIBRATION_MODE )
		{
			
			_curView.homographyMatrix = cv::Mat();
			_curView.isCalibrated = false;
			updateViewHash();
			openPerspectiveImage();
		}
		else if (_curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE)
		{
			
			updateViewHash();
		}

	});

	QObject::connect(ui.toolButton_resetPointsFloor, &QToolButton::clicked, [&]() {
		
		clearPolygon(false, FLOOR_VIEW);

		if (_curWorkingMode == WorkingMode::CALIBRATION_MODE)
		{
			_curView.homographyMatrix = cv::Mat();
			_curView.isCalibrated = false;
			updateViewHash();
			openPerspectiveImage();
		}

		});

	QObject::connect(ui.toolButton_workspace, &QToolButton::clicked, [&]() {
		showWorkPage();
		});

	QObject::connect(ui.toolButton_camTest, &QToolButton::clicked, [&]() {
		showCamTestPage();
		});

	QObject::connect(ui.toolButton_projects, &QToolButton::clicked, [&]() {
		showProjectsPage();
		});

	QObject::connect(ui.toolButton_settings, &QToolButton::clicked, [&]() {
		showSettingsPage();
		});

	QObject::connect(ui.toolButton_userAccount, &QToolButton::clicked, [&]() {
		showAccountPage();
		});

	connect(ui.graphicsView_viewCam, &QMainGraphicsView::mousePress,
		this, [=](const QPointF& pos, bool flag) {

			graphicViewMousePress(ViewMode::CAM_VIEW, pos, true);
		});


	connect(ui.graphicsView_viewFloorPlan, &QMainGraphicsView::mousePress,
		this, [=](const QPointF& pos, bool flag) {

			graphicViewMousePress(ViewMode::FLOOR_VIEW, pos, true);
		});

	connect(ui.graphicsView_viewCam,
		&QMainGraphicsView::mouseMove,    // Replace with your custom view class type
		this,
		[=](const QPoint& pt) {
			if (_startDrawPolygon)
			{
				graphicViewMouseMove(CAM_VIEW, pt);
			}

		});


	connect(ui.graphicsView_viewFloorPlan,
		&QMainGraphicsView::mouseMove,    // Replace with your custom view class type
		this,
		[=](const QPoint& pt) {
			if (_startDrawPolygon)
			{
				graphicViewMouseMove(FLOOR_VIEW, pt);
			}

		});

	// user account 
	connect(ui.pushButton_createAcc, &QPushButton::clicked, this, [=]() {
		if (openUserDialog(User_Dialog_Mode::CREATE_ACCOUNT))
		{
			showAccountPage();
		}
		});

	connect(ui.pushButton_logOut, &QPushButton::clicked, this, [=]() {
		if (openUserDialog(User_Dialog_Mode::LOGIN))
		{
			showAccountPage();
		}
		});


	//coords: link mouse mvmt to update coordinates 
	connect(ui.graphicsView_viewFloorPlan, &QMainGraphicsView::mouseMove, this, &Moonica_qt::updateFloorPlanCoords);

	QObject::connect(ui.toolButton_calibrationMode, &QToolButton::clicked, [&]() {
		switchWorkingMode(WorkingMode::CALIBRATION_MODE);
		});
	QObject::connect(ui.toolButton_analysisMode, &QToolButton::clicked, [&]() {
		switchWorkingMode(WorkingMode::ANALYSIS_MODE);
		});
	QObject::connect(ui.toolButton_entranceExit, &QToolButton::clicked, [&]() {
		switchWorkingMode(WorkingMode::ENTRANCE_EXIT_MODE);
		});

	QObject::connect(ui.toolButton_select, &QToolButton::clicked, [&]() {
		switchDrawingMode(DrawingMode::SELECT_MODE);
		});
	QObject::connect(ui.toolButton_drawPoly, &QToolButton::clicked, [&]() {
		switchDrawingMode(DrawingMode::DRAW_POLYGON_MODE);
		});



	connect(ui.listWidget_projectList, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem* item) {
		if (item)
		{
			qDebug() << "Opening project: " << item->text();
			openProject(item->text());
		}

		});

	QObject::connect(ui.toolButton_addRecipe, &QToolButton::clicked, [&]() {
		QString projecteName = ui.lineEdit_newRecipeName->text();
		addProject(projecteName);
		});

	QObject::connect(ui.toolButton_deleteRecipe, &QToolButton::clicked, [&]() {
		QListWidgetItem* item = ui.listWidget_projectList->currentItem();
		if (item) {
			QString projectName = item->text();
			deleteProject(projectName);
		}

		});

	connect(ui.toolButton_setNumberOfPeople, SIGNAL(clicked()), this, SLOT(forceSetNumberFloorObject()));
	//qRegisterMetaType<OnnxResult>("OnnxResult");


	connect(ui.toolButton_insertFloorPoint, &QToolButton::clicked, this, [=]()
		{
			drawPolygonGrabber("-", ui.spinBox_floorX->value(), ui.spinBox_floorY->value(), 20, Qt::red, Qt::red, ViewMode::FLOOR_VIEW);
		});

	connect(ui.toolButton_doneFloorPoint, &QToolButton::clicked, this, [=]()
		{
			addPolygonObject(ViewMode::FLOOR_VIEW);
			updateViewHash();
		});

	connect(ui.toolButton_recordCamTest, &QToolButton::clicked, this, [=]()
		{
			recordCams();
		});
	connect(ui.radioButton_liveMode, &QRadioButton::toggled, this, [=](bool checked)
		{
			if (checked)
			{
				ui.stackedWidget_camTestMode->setCurrentIndex(0);
			}
		});
	connect(ui.radioButton_videoMode, &QRadioButton::toggled, this, [=](bool checked)
		{
			if (checked)
			{
				ui.stackedWidget_camTestMode->setCurrentIndex(1);
			}
		});

	connect(ui.toolButton_camTestVideoDir, &QToolButton::clicked, this, [=]()
		{
			QString dir = QFileDialog::getExistingDirectory(
				this,
				tr("Select Video Directory"),
				PathManager::_recordedVideoDir,
				QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

			if (!dir.isEmpty())
			{
				qDebug() << "Selected directory:" << dir;

				ui.lineEdit_videoDir->setText(dir);
			}
		});

	connect(ui.toolButton_startVideoStream, &QToolButton::clicked, this, [=]()
		{
			startStreamVideo();
		});
	connect(ui.toolButton_stopVideoStream, &QToolButton::clicked, this, [=]()
		{
			stopStreamVideo();
		});
		
	connect(ui.comboBox_humanTrackingModelName,
		&QComboBox::currentTextChanged,
		this,
		[=](const QString& text)
		{
			_curProject._settings.humanTracking_modelName = text;
		});

	connect(ui.comboBox_objectDetectionModelName,
		&QComboBox::currentTextChanged,
		this,
		[=](const QString& text)
		{
			_curProject._settings.objectDetection_modelName = text;
		});

	connect(ui.doubleSpinBox_humanTrackingAccuracy,
		QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this,
		[=](double value)
		{
			_curProject._settings.humanTracking_accuracy = value;
		});

	connect(ui.doubleSpinBox_objectDetectionAccuracy,
		QOverload<double>::of(&QDoubleSpinBox::valueChanged),
		this,
		[=](double value)
		{
			_curProject._settings.objectDetection_accuracy = value;
		});

	connect(ui.checkBox_enableFeatureRecognition,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			_curProject._settings.humanTracking_enableFeatureExtraction = checked;
		});
	
	QObject::connect(ui.toolButton_savePorjectSetting, &QToolButton::clicked, [&]() {

		_curProject._settings.viewSequenceList.clear();
		for (int i = 0; i < ui.listWidget_viewSeqience->count(); i++)
		{
			_curProject._settings.viewSequenceList.append(ui.listWidget_viewSeqience->item(i)->text());
		}


		_curProject.saveProjectSetting();

		QMessageBox::information(this, ("Project Setting saved"),
			"Project Setting saved");
	});


	connect(ui.checkBox_checkSmock,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			_checkingCriteria.checkSmock = checked;
			_trackManager->setCriteria(_checkingCriteria);
		});

	connect(ui.checkBox_checkMask,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			_checkingCriteria.checkMask = checked;
			_trackManager->setCriteria(_checkingCriteria);
		});

	connect(ui.checkBox_checkGlove,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			_checkingCriteria.checkGlove = checked;
			_trackManager->setCriteria(_checkingCriteria);
		});

	connect(ui.checkBox_checkShoe,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			_checkingCriteria.checkShoe = checked;
			_trackManager->setCriteria(_checkingCriteria);
		});


	connect(ui.spinBox_detectionBufferFrame,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[=](int value)
		{
			_checkingCriteria.detectionFrameBuffer = value;
			_trackManager->setCriteria(_checkingCriteria);
		});


	connect(ui.spinBox_viewRow,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[=](int value)
		{
			_curProject._settings.viewRow = value;
		});

	connect(ui.spinBox_viewCol,
		QOverload<int>::of(&QSpinBox::valueChanged),
		this,
		[=](int value)
		{
			_curProject._settings.viewCol = value;
		});

	// -=
	connect(ui.radioButton_singleViewTracking,
		&QRadioButton::toggled,
		this,
		[=](bool checked)
		{
			if (checked)
			{
				_curProject._settings.isGlobalViewChecking = !checked;
				_curProject.saveProjectSetting();
			}
			
		});

	connect(ui.radioButton_globalTracking,
		&QRadioButton::toggled,
		this,
		[=](bool checked)
		{
			if (checked)
			{
				_curProject._settings.isGlobalViewChecking = checked;
				_curProject.saveProjectSetting();
			}
			
		});


	connect(ui.checkBox_streamSetting,
		&QCheckBox::toggled,
		this,
		[=](bool checked)
		{
			if (checked)
			{
				ui.frame_camSelection->show();
			}
			else
			{
				ui.frame_camSelection->hide();
			}

			for (auto& c : _cameraDisplayHash)
			{
				c.camView->resetTransform();
				c.camView->fitInView(
					c.camView->sceneRect(),
					Qt::KeepAspectRatio
				);
			}
		});

	connect(ui.horizontalSlider_videoStream, &QSlider::sliderPressed, this, [=]()
		{
			
			for (auto& fG : _frameGrabberHash)
			{
				fG->stopFrameGrabbing();
			}
		});

	connect(ui.horizontalSlider_videoStream, &QSlider::sliderReleased, this, [=]()
		{
			int targetMs = ui.horizontalSlider_videoStream->value();
			double targetSec = targetMs / 1000.0;

			for (auto& fG : _frameGrabberHash)
			{
				fG->setCurrentSecond(targetSec);
				fG->setVideoMode(true);
				fG->start();
			}
			
		});

	connect(ui.pushButton_openAllCam, &QPushButton::clicked, this, [this]() {

		for (auto& c : _camSelectionCheckBoxs)
		{
			c->setChecked(true);
		}
		});


	connect(ui.pushButton_closeAllCam, &QPushButton::clicked, this, [this]() {

		for (auto& c : _camSelectionCheckBoxs)
		{
			c->setChecked(false);
		}
		});

}

void Moonica_qt::maximize_restoreWindow()
{
	QPropertyAnimation* animation = new QPropertyAnimation(this, "geometry");
	animation->setDuration(200);

	int screenIndex = QApplication::desktop()->screenNumber(this);

	if (!_maximizedState)
	{
		_maximizedState = true;
		setMask(QRegion());
		animation->setEndValue(QRect(QApplication::desktop()->screenGeometry(screenIndex).x(), QApplication::desktop()->screenGeometry(screenIndex).y(), QApplication::desktop()->screenGeometry(screenIndex).width(), QApplication::desktop()->screenGeometry(screenIndex).height()));
		animation->start();
		ui.toolButton_maximize_restore->setIcon(QIcon(":/24x24/Icon/24x24/cil-window-restore.png"));
		connect(animation, &QPropertyAnimation::finished, this, [=]() {
			setWindowState(Qt::WindowFullScreen);

			});

	}
	else
	{
		_maximizedState = false;
		animation->setEndValue(QRect(QApplication::desktop()->screenGeometry(screenIndex).x() + 100, QApplication::desktop()->screenGeometry(screenIndex).y(), 1566, 1010));
		animation->start();
		ui.toolButton_maximize_restore->setIcon(QIcon(":/24x24/Icon/24x24/cil-window-maximize.png"));
		connect(animation, &QPropertyAnimation::finished, this, [=]() {
			setWindowState(Qt::WindowNoState);

			});

	}
}

void Moonica_qt::maximizedWindow()
{
	QPropertyAnimation* animation = new QPropertyAnimation(this, "geometry");
	animation->setDuration(200);

	_maximizedState = true;
	setMask(QRegion());

	int screenIndex = QApplication::desktop()->screenNumber(this);
	animation->setEndValue(QRect(QApplication::desktop()->screenGeometry(screenIndex).x(), QApplication::desktop()->screenGeometry(screenIndex).y(), QApplication::desktop()->screenGeometry(screenIndex).width(), QApplication::desktop()->screenGeometry(screenIndex).height()));
	animation->start();
	ui.toolButton_maximize_restore->setIcon(QIcon(":/24x24/Icon/24x24/cil-window-restore.png"));
	connect(animation, &QPropertyAnimation::finished, this, [=]() {
		setWindowState(Qt::WindowFullScreen);

		});
}

void Moonica_qt::closeWindow() //fade out
{
	//fade out
	QGraphicsOpacityEffect* eff = new QGraphicsOpacityEffect(this);
	setGraphicsEffect(eff);
	QPropertyAnimation* a = new QPropertyAnimation(eff, "opacity");
	a->setDuration(500);
	a->setStartValue(1);
	a->setEndValue(0);
	a->setEasingCurve(QEasingCurve::OutBack);
	a->start(QPropertyAnimation::DeleteWhenStopped);
	connect(a, &QPropertyAnimation::finished, this, [=]() {
		setGraphicsEffect(nullptr);
		close();
		});
}


void Moonica_qt::showWorkPage()
{
	qDebug() << "Show work page";
	if (_frameManager && _trackManager)
	{
		camViewSelectOff_all();
	}
	
	
	stopVideoTest();
	
	switchPageWithAnimation(WORK_PAGE);
	//ui.stackedWidget_mainBody->setCurrentIndex(WORK_PAGE);
	ui.toolButton_workspace->setChecked(true);
	ui.toolButton_camTest->setChecked(false);
	ui.toolButton_projects->setChecked(false);	
	ui.toolButton_settings->setChecked(false);	
	ui.toolButton_userAccount->setChecked(false);
	
}

void Moonica_qt::showCamTestPage()
{

	clearCamSelectionResources();
	stopVideoTest();

	switchPageWithAnimation(CAM_TEST_PAGE);
	//ui.stackedWidget_mainBody->setCurrentIndex(CAM_TEST_PAGE);

	ui.toolButton_workspace->setChecked(false);
	ui.toolButton_camTest->setChecked(true);
	ui.toolButton_projects->setChecked(false);
	ui.toolButton_settings->setChecked(false);
	ui.toolButton_userAccount->setChecked(false);

	ui.checkBox_streamSetting->setChecked(true);

	initCamSelection();
}


void Moonica_qt::showProjectsPage()
{
	clearCamSelectionResources();
	stopVideoTest();

	switchPageWithAnimation(PROJECTS_PAGE);
	//ui.stackedWidget_mainBody->setCurrentIndex(PROJECTS_PAGE);

	ui.toolButton_workspace->setChecked(false);
	ui.toolButton_camTest->setChecked(false);
	ui.toolButton_projects->setChecked(true);
	ui.toolButton_settings->setChecked(false);
	ui.toolButton_userAccount->setChecked(false);

	initProjectList();

	if (_curProject._projectName.isEmpty())
	{
		ui.toolButton_workspace->setEnabled(false);
		ui.toolButton_camTest->setEnabled(false);
		ui.toolButton_settings->setEnabled(false);
	}
}

void Moonica_qt::showSettingsPage()
{
	ui.toolButton_workspace->setChecked(false);
	ui.toolButton_camTest->setChecked(false);
	ui.toolButton_projects->setChecked(false);
	ui.toolButton_settings->setChecked(true);
	ui.toolButton_userAccount->setChecked(false);


	
	switchPageWithAnimation(SETTING_PAGE);
	//ui.stackedWidget_mainBody->setCurrentIndex(SETTING_PAGE);

	clearCamSelectionResources();
	qDebug() << "Show Setting end";
}

void Moonica_qt::showAccountPage()
{
	ui.toolButton_workspace->setChecked(false);
	ui.toolButton_camTest->setChecked(false);
	ui.toolButton_projects->setChecked(false);
	ui.toolButton_settings->setChecked(false);
	ui.toolButton_userAccount->setChecked(true);




	switchPageWithAnimation(ACCOUNT_PAGE);
	//ui.stackedWidget_mainBody->setCurrentIndex(ACCOUNT_PAGE);
	ui.label_userName1->setText(_curUserAccInfo.userName);

	clearCamSelectionResources();
	qDebug() << "Show Setting end";
}

void Moonica_qt::switchWorkingMode(WorkingMode mode)
{
	ui.toolButton_calibrationMode->setChecked(false);
	ui.toolButton_analysisMode->setChecked(false);
	ui.toolButton_entranceExit->setChecked(false);
	if (mode == WorkingMode::CALIBRATION_MODE)
	{
		ui.toolButton_select->setEnabled(true);

		switchDrawingMode(DrawingMode::SELECT_MODE);
		clearPolygon(true);
		ui.toolButton_calibrationMode->setChecked(true);
	}
	else if (mode == WorkingMode::ANALYSIS_MODE)
	{
		ui.toolButton_select->setEnabled(false);

		switchDrawingMode(DrawingMode::DRAW_POLYGON_MODE);
		clearPolygon(true);
		ui.toolButton_analysisMode->setChecked(true);

	}

	else if (mode == WorkingMode::ENTRANCE_EXIT_MODE)
	{
		ui.toolButton_select->setEnabled(false);

		switchDrawingMode(DrawingMode::DRAW_POLYGON_MODE);
		clearPolygon(true);
		ui.toolButton_entranceExit->setChecked(true);
		ui.graphicsView_viewFloorPlan->setCursor(Qt::ForbiddenCursor);

	}

	_curWorkingMode = mode;
	openViewImage(_curView.viewName);
}

void Moonica_qt::switchDrawingMode(DrawingMode mode)
{

	ui.toolButton_select->setChecked(false);
	ui.toolButton_drawPoly->setChecked(false);


	if (mode == DrawingMode::SELECT_MODE)
	{
		setDragMode(true);
		ui.toolButton_select->setChecked(true);

		ui.graphicsView_viewCam->setCursor(Qt::ArrowCursor);
		ui.graphicsView_viewFloorPlan->setCursor(Qt::ArrowCursor);
	}
	else if (mode == DrawingMode::DRAW_POLYGON_MODE)
	{
		setDragMode(false);
		ui.toolButton_drawPoly->setChecked(true);

		ui.graphicsView_viewCam->setCursor(Qt::CrossCursor);
		ui.graphicsView_viewFloorPlan->setCursor(Qt::CrossCursor);
	}


	_curDrawingMode = mode;
}




void Moonica_qt::setDragMode(bool flag)
{
	qDebug() << "setDragMode: " << flag;
	_dragMode = flag;

	if (!_dragMode)
	{
		for (int i = 0; i < _dragPolygonROI_camView.size(); i++)
		{
			_dragPolygonROI_camView.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, false);
			_dragPolygonROI_camView.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
			_dragPolygonROI_camView.at(i)->setDragable(false);
		}

		for (int i = 0; i < _dragPolygonROI_floorView.size(); i++)
		{
			_dragPolygonROI_floorView.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, false);
			_dragPolygonROI_floorView.at(i)->setFlag(QGraphicsItem::ItemIsMovable, false);
			_dragPolygonROI_floorView.at(i)->setDragable(false);
		}
	}
	else
	{
		for (int i = 0; i < _dragPolygonROI_camView.size(); i++)
		{
			_dragPolygonROI_camView.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, true);
			_dragPolygonROI_camView.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
			_dragPolygonROI_camView.at(i)->setDragable(true);
		}

		for (int i = 0; i < _dragPolygonROI_floorView.size(); i++)
		{
			_dragPolygonROI_floorView.at(i)->setFlag(QGraphicsItem::ItemIsSelectable, true);
			_dragPolygonROI_floorView.at(i)->setFlag(QGraphicsItem::ItemIsMovable, true);
			_dragPolygonROI_floorView.at(i)->setDragable(true);
		}
	}

}


void Moonica_qt::setupGraphicsViews()
{
	m_scene_camView = new QMainGraphicsScene(this); ui.graphicsView_viewCam->setScene(m_scene_camView);
	m_scene_floorView = new QMainGraphicsScene(this); ui.graphicsView_viewFloorPlan->setScene(m_scene_floorView);
	m_perspectiveViewScene = new QMainGraphicsScene(this); ui.graphicsView_viewPerspective->setScene(m_perspectiveViewScene);

	//coords
	ui.graphicsView_viewFloorPlan->setMouseTracking(true);
	/*ui.graphicsView_viewFloorPlan->setAlignment(QPainter::Antialiasing, true);*/

	ui.graphicsView_viewCam->setRenderHint(QPainter::Antialiasing, true);
	ui.graphicsView_viewCam->setDragMode(QGraphicsView::RubberBandDrag);
	ui.graphicsView_viewCam->setOptimizationFlags(QGraphicsView::DontSavePainterState);
	ui.graphicsView_viewCam->setCacheMode(QGraphicsView::CacheNone);
	ui.graphicsView_viewCam->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	ui.graphicsView_viewCam->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

	ui.graphicsView_viewFloorPlan->setRenderHint(QPainter::Antialiasing, true);
	ui.graphicsView_viewFloorPlan->setDragMode(QGraphicsView::RubberBandDrag);
	ui.graphicsView_viewFloorPlan->setOptimizationFlags(QGraphicsView::DontSavePainterState);
	ui.graphicsView_viewFloorPlan->setCacheMode(QGraphicsView::CacheNone);
	ui.graphicsView_viewFloorPlan->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
	ui.graphicsView_viewFloorPlan->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void Moonica_qt::displayImageInScene(const QPixmap& pixmap, ViewMode viewMode)
{
	auto updateScene = [&](QGraphicsScene* scene,
		QGraphicsPixmapItem*& pixmapItem,
		QRectF& sceneBound)
		{

			if (!scene) return;

			scene->clear();
			pixmapItem = scene->addPixmap(pixmap);

			if (!scene->views().isEmpty())
			{

				scene->views().first()->fitInView(pixmapItem, Qt::KeepAspectRatio);

			}


			sceneBound.setRect(0, 0, pixmap.width(), pixmap.height());
			scene->setSceneRect(sceneBound);
		};

	switch (viewMode)
	{
	case CAM_VIEW:
		updateScene(m_scene_camView, m_pixmapItem_camView, _sceneBound_camView);
		break;
	case FLOOR_VIEW:
		updateScene(m_scene_floorView, m_pixmapItem_floorView, _sceneBound_floorView);
		break;
	case PERSPECTIVE_VIEW:
		updateScene(m_perspectiveViewScene, m_pixmapItem_perspectiveView, _sceneBound_perspectiveView);
		break;
	default:
		break;
	}
}

void Moonica_qt::clearAllView()
{
	if (m_scene_camView) m_scene_camView->clear();
	if (m_scene_floorView) m_scene_floorView->clear();
	if (m_perspectiveViewScene) m_perspectiveViewScene->clear();
}


void Moonica_qt::clearScene(QMainGraphicsScene* scene, QGraphicsPixmapItem*& item)
{
	if (scene) scene->clear();
	item = nullptr;
	//clearAllDefectRects();
}

void Moonica_qt::clearPolygon(bool clearAll, ViewMode cView)
{
	qDebug() << "clearPolygon";

	removeSetupPolygon(CAM_VIEW);
	removeSetupPolygon(FLOOR_VIEW);


	for (auto b : _pointLabel_camView)
	{

		m_scene_camView->removeItem(b);
		delete b;
	}
	_pointLabel_camView.clear();

	
	for (auto b : _pointLabel_floorView)
	{

		m_scene_floorView->removeItem(b);

		delete b;
	}
	_pointLabel_floorView.clear();


	_polygonGrabberCount_camView = _pointLabel_camView.size();
	_polygonGrabberCount_floorView = _pointLabel_floorView.size();

	QVector<QDragPolygon*> removedPolygon_view;
	QVector<QDragPolygon*> removedPolygon_floor;
	QVector<QDragPolygon*> removedPolygon_view_entranceExit;

	
	for (auto b : _dragPolygonROI_camView)
	{
		if (clearAll)
		{
			removedPolygon_view.append(b);
		}
		else if (cView == CAM_VIEW)
		{
			removedPolygon_view.append(b);
		}

	}

	
	for (auto b : _dragPolygonROI_floorView)
	{
		if (clearAll)
		{
			removedPolygon_floor.append(b);
		}
		else if (cView == FLOOR_VIEW)
		{
			removedPolygon_floor.append(b);
		}
	}

	for (auto b : _dragPolygonEntranceExitRoi_camView)
	{
		removedPolygon_view_entranceExit.append(b);
	}

	if (_curWorkingMode == WorkingMode::CALIBRATION_MODE)
	{
		
		for (auto b : removedPolygon_view)
		{
			m_scene_camView->removeItem(b);
			_dragPolygonROI_camView.removeOne(b);

			delete b;
		}
		

		
		for (auto b : removedPolygon_floor)
		{
			m_scene_floorView->removeItem(b);
			_dragPolygonROI_floorView.removeOne(b);

			delete b;
		}
		
		
	}

	

	if (_curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE)
	{
		for (auto b : removedPolygon_view_entranceExit)
		{
			m_scene_camView->removeItem(b);
			_dragPolygonEntranceExitRoi_camView.removeOne(b);

			delete b;
		}
		removedPolygon_view_entranceExit.clear();
	}
	

	
	_polygonGrabberCount_camView = 0;
	_polygonGrabberCount_floorView = 0;

	qDebug() << "clearPolygon - end";


}

//void Moonica_qt::clearPolygon(bool clearAll, ViewMode cView)
//{
//
//	qDebug() << "clearPolygon";
//
//	if (_curWorkingMode == WorkingMode::ANALYSIS_MODE)
//	{
//
//		qDebug() << 1;
//		removeSetupPolygon(CAM_VIEW);
//
//		removeSetupPolygon(FLOOR_VIEW);
//
//		qDebug() << 2;
//
//		for (auto b : _pointLabel_camView)
//		{
//
//			m_scene_camView->removeItem(b);
//			delete b;
//		}
//
//		_pointLabel_camView.clear();
//
//		qDebug() << 3;
//		for (auto b : _pointLabel_floorView)
//		{
//
//			m_scene_floorView->removeItem(b);
//
//			delete b;
//		}
//		_pointLabel_floorView.clear();
//
//
//		_polygonGrabberCount_camView = _pointLabel_camView.size();
//		_polygonGrabberCount_floorView = _pointLabel_floorView.size();
//
//		qDebug() << 4;
//
//	}
//	else if (_curWorkingMode == WorkingMode::CALIBRATION_MODE || _curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE)
//	{
//		QVector<QDragPolygon*> removedPolygon_view;
//		QVector<QDragPolygon*> removedPolygon_floor;
//		QVector<QDragPolygon*> removedPolygon_view_entranceExit;
//
//		qDebug() << 5;
//		for (auto b : _dragPolygonROI_camView)
//		{
//			if (clearAll)
//			{
//				removedPolygon_view.append(b);
//			}
//			else if (cView == CAM_VIEW)
//			{
//				removedPolygon_view.append(b);
//			}
//
//		}
//
//		qDebug() << 6;
//		for (auto b : _dragPolygonROI_floorView)
//		{
//			if (clearAll)
//			{
//				removedPolygon_floor.append(b);
//			}
//			else if (cView == FLOOR_VIEW)
//			{
//				removedPolygon_floor.append(b);
//			}
//		}
//		for (auto b : _dragPolygonEntranceExitRoi_camView)
//		{
//			removedPolygon_view_entranceExit.append(b);
//		}
//
//		qDebug() << 7;
//		for (auto b : removedPolygon_view)
//		{
//			m_scene_camView->removeItem(b);
//			_dragPolygonROI_camView.removeOne(b);
//
//			delete b;
//		}
//		_pointLabel_camView.clear();
//
//		qDebug() << 8;
//		for (auto b : removedPolygon_floor)
//		{
//			m_scene_floorView->removeItem(b);
//			_dragPolygonROI_floorView.removeOne(b);
//
//			delete b;
//		}
//		_pointLabel_floorView.clear();
//		qDebug() << 9;
//		for (auto b : removedPolygon_view_entranceExit)
//		{
//			m_scene_camView->removeItem(b);
//			_dragPolygonEntranceExitRoi_camView.removeOne(b);
//
//			delete b;
//		}
//		removedPolygon_view_entranceExit.clear();
//
//		//_polygonGrabberCount_camView = _pointLabel_camView.size();
//		//_polygonGrabberCount_floorView = _pointLabel_floorView.size();
//		_polygonGrabberCount_camView = 0;
//		_polygonGrabberCount_floorView = 0;
//
//		qDebug() << "Clear polygon: " << _polygonGrabberCount_floorView;
//	}
//
//}




void Moonica_qt::graphicViewMouseMove(ViewMode viewMode, QPoint pt)
{
	if (_startDrawPolygon)
	{

		QPointF point1 = _prevPolygonGrabber->getGeometry().center();
		QPointF point2 = pt;

		QRectF rect(point1, point2);


		_curPolygonLine->setPoint2(rect);
		_curPolygonLine->scene()->update();
	}
}

void Moonica_qt::graphicViewMousePress(ViewMode viewMode, QPointF pt, bool isLeftClick)
{

	if (_curDrawingMode == DrawingMode::SELECT_MODE)
	{

		if (viewMode == CAM_VIEW)
		{
			for (auto& p : _dragPolygonROI_camView) p->setSelected(false);
		}
		else if (viewMode == FLOOR_VIEW)
		{
			for (auto& p : _dragPolygonROI_floorView) p->setSelected(false);
		}
	}

	if (_curDrawingMode == DrawingMode::DRAW_POLYGON_MODE && (_curWorkingMode == WorkingMode::CALIBRATION_MODE || _curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE))
	{

		if (_startDrawPolygon && _firstPolygonGrabber->getGeometry().contains(pt))
		{
			// end drawing
			_startDrawPolygon = false;



			addPolygonObject(viewMode);
			updateViewHash();

			//lockUi(_startDrawPolygon);
		}
		else
		{


			if (_curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE && viewMode == ViewMode::FLOOR_VIEW)
			{
				QMessageBox::warning(this, tr("Cannot draw Enrance/Exit region on FloorMap!"),
					tr("Please draw on Cam View!"));
				return;
			}

			if (_curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE && !_dragPolygonEntranceExitRoi_camView.isEmpty())
			{
				QMessageBox::warning(this, tr("Cannot insert more region!"),
					tr("Please press reset!"));
				return;
			}

			if (viewMode == ViewMode::CAM_VIEW && !_dragPolygonROI_camView.isEmpty())
			{
				QMessageBox::warning(this, tr("Cannot insert more Points!"),
					tr("Please press reset to re-calibrate points!"));
				return;
			}
			if (viewMode == ViewMode::FLOOR_VIEW && !_dragPolygonROI_floorView.isEmpty())
			{
				QMessageBox::warning(this, tr("Cannot insert more Points!"),
					tr("Please press reset to re-calibrate points!"));
				return;
			}

			QString polyGrabberID = QString::number(_polyGrabber_camView.size());
			int polyRadius = 6;
			if (viewMode == ViewMode::FLOOR_VIEW) polyRadius = 20;
			QEllipseItem* curPolygon = drawPolygonGrabber(polyGrabberID, pt.x(), pt.y(), polyRadius, Qt::red, Qt::yellow, viewMode);



			if (_curDrawingMode == DrawingMode::DRAW_QUAD_POLYGON_MODE && _polygonGrabberCount_camView == 4)
			{
				//// end drawing
				//_startDrawPolygon = false;
				//

				//addPolygonObject(viewMode);
				//// lockUi(_startDrawPolygon);
			}
			else
			{
				if (viewMode == ViewMode::CAM_VIEW)
				{
					if (_polyGrabber_camView.size() > 0 && curPolygon != nullptr)
					{
						if (!_startDrawPolygon)
						{
							_firstPolygonGrabber = curPolygon;
							_startDrawPolygon = true;
							// lockUi(_startDrawPolygon);
						}

						_prevPolygonGrabber = curPolygon;

						QPointF point = curPolygon->getGeometry().center();
						QLineF line(point, point);
						_curPolygonLine = drawPolygonLine(line, Qt::red, 3, CAM_VIEW);

					}
				}
				else if (viewMode == ViewMode::FLOOR_VIEW)
				{
					if (_polyGrabber_floorView.size() > 0 && curPolygon != nullptr)
					{
						if (!_startDrawPolygon)
						{
							_firstPolygonGrabber = curPolygon;
							_startDrawPolygon = true;
							// lockUi(_startDrawPolygon);
						}

						_prevPolygonGrabber = curPolygon;

						QPointF point = curPolygon->getGeometry().center();
						QLineF line(point, point);
						_curPolygonLine = drawPolygonLine(line, Qt::red, 3, FLOOR_VIEW);

					}
				}

			}

		}
	}
	else if (_curWorkingMode == WorkingMode::ANALYSIS_MODE && viewMode == ViewMode::CAM_VIEW)
	{
		if (_curView.isCalibrated)
		{
			QEllipseItem* curPoint = drawPolygonGrabber("-", pt.x(), pt.y(), 6, Qt::red, Qt::red, viewMode);

			QPointF floorPt = tranformPointToFloorView(pt, _curView.homographyMatrix);

			QEllipseItem* floorPoint = drawPolygonGrabber("-", floorPt.x(), floorPt.y(), 20, Qt::red, Qt::red, ViewMode::FLOOR_VIEW);

		}
		else
		{
			QMessageBox::warning(this, tr("Analysis Failed!"),
				tr("Image not yet calibrated"));
		}

	}

}





void Moonica_qt::removeSetupPolygon(ViewMode viewMode)
{
	if (viewMode == ViewMode::CAM_VIEW)
	{
		if (m_scene_camView != nullptr)
		{
			for (int i = 0; i < _polyGrabber_camView.size(); i++)
			{
				m_scene_camView->removeItem(_polyGrabber_camView[i]);
			}

			for (int i = 0; i < _polyLine_camView.size(); i++)
			{
				m_scene_camView->removeItem(_polyLine_camView[i]);
			}
			_polyGrabber_camView.clear();
			_polyLine_camView.clear();
		}
	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		if (m_scene_floorView != nullptr)
		{
			for (int i = 0; i < _polyGrabber_floorView.size(); i++)
			{
				m_scene_floorView->removeItem(_polyGrabber_floorView[i]);
			}

			for (int i = 0; i < _polyLine_floorView.size(); i++)
			{
				m_scene_floorView->removeItem(_polyLine_floorView[i]);
			}
			_polyGrabber_floorView.clear();
			_polyLine_floorView.clear();




		}
	}


}

void Moonica_qt::addPolygonObject(ViewMode viewMode)
{
	qDebug() << "addPolygonObject";
	QPolygonF p;
	if (viewMode == ViewMode::CAM_VIEW)
	{
		for (auto gb : _polyGrabber_camView)
		{
			p << gb->getGeometry().center();;
		}
		//_polygonGrabberCount_camView = 0;
	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		for (auto gb : _polyGrabber_floorView)
		{
			p << gb->getGeometry().center();;
		}
		//_polygonGrabberCount_floorView = 0;
	}


	removeSetupPolygon(viewMode);

	if (p.isEmpty()) return;

	QDragPolygon* pShape = new QDragPolygon();
	pShape->setup(p.toPolygon(), Qt::red, "Setup Region");
	pShape->setFlag(QGraphicsItem::ItemIsSelectable, false);
	pShape->setFlag(QGraphicsItem::ItemIsMovable, false);
	pShape->setDragable(false);
	pShape->setColorAnimation(false);
	pShape->setPenWidth(5);

	//pShape->setFlag(QGraphicsItem::ItemIsSelectable, true);
	//pShape->setFlag(QGraphicsItem::ItemIsMovable, false);
	//pShape->setSelected(true);
	//pShape->setDragable(false); 


	if (viewMode == ViewMode::CAM_VIEW)
	{

		pShape->setOutterBarrier(_sceneBound_camView);
		m_scene_camView->addItem(pShape);

		if (_curWorkingMode == ENTRANCE_EXIT_MODE)
			_dragPolygonEntranceExitRoi_camView.append(pShape);
		else
		{
			_dragPolygonROI_camView.append(pShape);
			for (auto& pLabel : _pointLabel_camView)
			{
				if (pLabel->parentItem() == nullptr)
				{
					pLabel->setParentItem(pShape);
					pLabel->setX(pLabel->x() - pShape->x());
					pLabel->setY(pLabel->y() - pShape->y());
				}

			}
		}
	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		
		pShape->setOutterBarrier(_sceneBound_floorView);
		m_scene_floorView->addItem(pShape);
		_dragPolygonROI_floorView.append(pShape);
		
		for (auto& pLabel : _pointLabel_floorView)
		{
			if (pLabel->parentItem() == nullptr)
			{
				pLabel->setParentItem(pShape);
				pLabel->setX(pLabel->x() - pShape->x());
				pLabel->setY(pLabel->y() - pShape->y());
			}

		}
	
		_pointLabel_floorView.clear();
	
	}


}



QEllipseItem* Moonica_qt::drawPolygonGrabber(QString id,
	const qreal& x,
	const qreal& y,
	const qreal& radius,
	const QColor& borderColor,
	const QColor& innerColor,
	ViewMode viewMode)
{
	QEllipseItem* pShape = new QEllipseItem();
	pShape->setZValue(1);
	pShape->setup(QRectF(x, y, radius, radius), borderColor, innerColor);
	pShape->setName(id);

	int index = 0; // for label

	if (viewMode == ViewMode::CAM_VIEW)
	{

		_polygonGrabberCount_camView++;
		index = _polygonGrabberCount_camView;
		_polyGrabber_camView.append(pShape);
		m_scene_camView->addItem(pShape);

		if (index == 1)
			pShape->setBorderColor(Qt::yellow);
	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		_polygonGrabberCount_floorView++;
		index = _polygonGrabberCount_floorView;
		_polyGrabber_floorView.append(pShape);
		m_scene_floorView->addItem(pShape);

		if (index == 1)
			pShape->setBorderColor(Qt::yellow);
	}

	// Add label as child (use index)
	QGraphicsSimpleTextItem* label = new QGraphicsSimpleTextItem(QString::number(index));
	label->setBrush(Qt::black);  // text color
	label->setZValue(3);

	// Calculate background rectangle size
	QRectF textRect = label->boundingRect();
	qreal padding = 2.0;
	QGraphicsRectItem* bg = new QGraphicsRectItem(
		textRect.adjusted(-padding, -padding, padding, padding)
	);
	bg->setBrush(QColor(255, 255, 0, 200)); // semi-transparent yellow
	bg->setPen(Qt::NoPen);
	bg->setZValue(2);

	// Create a parent container item so both move together
	QGraphicsItemGroup* labelGroup = new QGraphicsItemGroup();
	labelGroup->addToGroup(bg);
	labelGroup->addToGroup(label);

	// Position the group relative to pShape
	qreal posX = x + radius / 2 - textRect.width() / 2;
	qreal posY = y - textRect.height() - 4;
	labelGroup->setPos(posX, posY);


	if (viewMode == ViewMode::CAM_VIEW)
	{
		_pointLabel_camView.append(labelGroup);
		m_scene_camView->addItem(labelGroup);

	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		_pointLabel_floorView.append(labelGroup);
		m_scene_floorView->addItem(labelGroup);
	}

	return pShape;
}
QLineItem* Moonica_qt::drawPolygonLine(const QLineF& line, const QColor& color, const int& lineWidth, ViewMode viewMode)
{
	QLineItem* pShape = new QLineItem();
	pShape->setZValue(0);
	pShape->setup(QRectF(line.p1(), line.p2()), color, lineWidth);

	if (viewMode == ViewMode::CAM_VIEW)
	{
		m_scene_camView->addItem(pShape);
		_polyLine_camView.append(pShape);
	}
	else if (viewMode == ViewMode::FLOOR_VIEW)
	{
		m_scene_floorView->addItem(pShape);
		_polyLine_floorView.append(pShape);
	}


	return pShape;
}

void Moonica_qt::addDragbox(ViewMode viewMode)
{
	qDebug() << "addDragbox";


	if (viewMode == ViewMode::CAM_VIEW)
	{

		double left = std::min(_startDragPos_camView.x(), _endDragPos_camView.y());
		double top = std::min(_startDragPos_camView.y(), _endDragPos_camView.y());

		// Find the bottom-right corner (maximum x and y coordinates)
		double right = std::max(_startDragPos_camView.x(), _endDragPos_camView.x());
		double bottom = std::max(_startDragPos_camView.y(), _endDragPos_camView.y());

		QRectF dBox(left, top, right - left, bottom - top);

		QGraphicsRectItem* pShape = new QGraphicsRectItem(dBox);


		qDebug() << "add to scene 1";
		m_scene_camView->addItem(pShape);
		_sceneCamRectVector.append(pShape);
	}
	else if (viewMode == ViewMode::CAM_VIEW)
	{

		double left = std::min(_startDragPos_floorView.x(), _endDragPos_floorView.x());
		double top = std::min(_startDragPos_floorView.y(), _endDragPos_floorView.y());

		// Find the bottom-right corner (maximum x and y coordinates)
		double right = std::max(_startDragPos_floorView.x(), _endDragPos_floorView.x());
		double bottom = std::max(_startDragPos_floorView.y(), _endDragPos_floorView.y());

		QRectF dBox(left, top, right - left, bottom - top);

		QGraphicsRectItem* pShape = new QGraphicsRectItem(dBox);


		m_scene_floorView->addItem(pShape);
		_sceneFloorRectVector.append(pShape);
	}

}


void Moonica_qt::onViewTreeContextMenu(const QPoint& point)
{
	qDebug() << "onViewTreeContextMenu";

	QTreeWidgetItem* item = ui.treeWidget_views->itemAt(point);
	if (item == nullptr) return;
	QString header = item->text(0);

	if (header == "Cam Views")
	{
		QMenu menu(this);
		menu.addAction(ui.actionImport_Cam_Views);
		menu.exec(ui.treeWidget_views->viewport()->mapToGlobal(point));
	}
	else
	{
		QMenu menu(this);
		menu.addAction(ui.actionSetAsDoorCam);
		menu.addAction(ui.actionRemoveDoorCam);
		menu.addAction(ui.actionResnap_Image);
		menu.addAction(ui.actionRemove_Image);

		menu.exec(ui.treeWidget_views->viewport()->mapToGlobal(point));

	}
}

void Moonica_qt::resnapImage()
{
	QTreeWidgetItem* item = ui.treeWidget_views->currentItem();
	if (item == nullptr || item->text(0) == "Cam Views")
	{
		//QMessageBox::warning(this, "Invalid CCTV URL", "No CCTV is detected");
		return;
	}

	QMessageBox::StandardButton reply;
	reply = QMessageBox::warning(this, "Confirm Image Resnap", "Display will go back to live feed",
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes)
	{
		clearPolygon(true);
		if (m_scene_camView)
		{
			m_scene_camView->clear();
			m_pixmapItem_camView = nullptr; // Reset the pixmap item pointer
		}

		QString viewName = item->text(0);
		ViewInfo& vInfo = _curProject._viewInfoHash[viewName];
		_curView = vInfo;
		m_workspaceLiveFeedThread = new QThread(this);
		m_workspaceLiveFeed = new VideoTester();
		m_workspaceLiveFeed->moveToThread(m_workspaceLiveFeedThread);

		connect(m_workspaceLiveFeedThread, &QThread::started, m_workspaceLiveFeed, [=]() {
			m_workspaceLiveFeed->startGrabbing(vInfo.cctvUrl); });
		connect(m_workspaceLiveFeed, &VideoTester::newFrameReady, this, &Moonica_qt::updateWorkspaceLiveFeed);

		//clean up 
		connect(m_workspaceLiveFeedThread, &QThread::finished, m_workspaceLiveFeed, &QObject::deleteLater);
		connect(m_workspaceLiveFeedThread, &QThread::finished, m_workspaceLiveFeedThread, &QObject::deleteLater);

		m_workspaceLiveFeedThread->start();
		ui.pushButton_captureImage->setVisible(true);


	}
}
void Moonica_qt::removeViewImage()
{
	QMessageBox::StandardButton reply;
	reply = QMessageBox::warning(this, "Confirm Remove Cam View",
		"Are you sure you want to delete cam view?<br>This action cannot be undone.",
		QMessageBox::Yes | QMessageBox::No);

	if (reply == QMessageBox::Yes) {
		if (_curProject._viewInfoHash.contains(_curView.viewName))
		{
			//remove image from workspace
			if (m_scene_camView)
			{
				m_scene_camView->clear();
				m_pixmapItem_camView = nullptr; // Reset the pixmap item pointer
			}

			_curProject._viewInfoHash.remove(_curView.viewName);

			// remove image
			QString targetImage = _curView.viewPath;
			if (QFileInfo::exists(targetImage))QFile::remove(targetImage);
			QString targetInfo = PathManager::_camViewsInfoDir + _curView.viewName + ".json";
			if (QFileInfo::exists(targetInfo))QFile::remove(targetInfo);
			QString matrixInfo = PathManager::_camViewsMatrixDir + _curView.viewName + ".yaml";
			if (QFileInfo::exists(matrixInfo))QFile::remove(matrixInfo);

			refreshViewInfoTreeWidget();
		}
	}
}

void Moonica_qt::importNewViewImage()
{
	QStringList imgList = QFileDialog::getOpenFileNames(
		this,
		tr("Select Images"),
		QDir::homePath(),
		tr("Image Files (%1);;All Files (*)").arg(PathManager::_imageExtensions.join(" "))
	);

	foreach(QString imgPath, imgList)
	{
		QFileInfo fileInfo(imgPath);
		QString filename = fileInfo.baseName();

		QString srcPath = imgPath;
		QString dstPath = PathManager::_camViewsDir + fileInfo.fileName();

		QFile::copy(srcPath, dstPath);
		ViewInfo vInfo;
		vInfo.viewPath = dstPath;
		vInfo.viewName = filename;
		vInfo.extension = fileInfo.suffix();
		vInfo.isCalibrated = false;

		_curProject._viewInfoHash[vInfo.viewName] = vInfo;
	}

	_curProject.saveProject();
	refreshViewInfoTreeWidget();

}

void Moonica_qt::deleteTree(QTreeWidgetItem* item)
{
	for (int i = 0; i < item->childCount(); i++) {
		deleteTree(item->child(i));
	}
	delete item;
}
void Moonica_qt::refreshViewInfoTreeWidget()
{
	qDebug() << "refreshViewInfoTreeWidget";
	ui.treeWidget_views->blockSignals(true);
	for (int i = 0; i < ui.treeWidget_views->topLevelItemCount(); i++)
	{
		deleteTree(ui.treeWidget_views->topLevelItem(i));
	}
	ui.treeWidget_views->clear();
	_childViewHash_workspace.clear();

	QTreeWidgetItem* imageModeTitle = new QTreeWidgetItem();

	ui.treeWidget_views->addTopLevelItem(imageModeTitle);
	imageModeTitle->setText(0, "Cam Views");


	for (auto& v : _curProject._viewInfoHash)
	{
		QTreeWidgetItem* childItem = new QTreeWidgetItem();
		childItem->setText(0, v.viewName);
		_childViewHash_workspace.insert(v.viewName, childItem);
		imageModeTitle->addChild(childItem);

		if (v.isDoorCam)
		{
			QIcon selectedIcon(":/ResultViewer/Icon/16x16/cil-door.png"); // Replace with the path to your icon
			childItem->setIcon(0, selectedIcon); // Set the icon for column 0
		}
	}

	ui.treeWidget_views->header()->setSectionResizeMode(QHeaderView::Stretch);
	ui.treeWidget_views->setHeaderLabel("Cam View Count: " + QString::number(_curProject._viewInfoHash.size()));
	ui.treeWidget_views->expandAll();


	if (!_curProject._floorViewInfo.viewPath.isEmpty())
	{
		for (int i = 0; i < ui.treeWidget_views->topLevelItemCount(); i++)
		{
			QTreeWidgetItem* topLevelItem = ui.treeWidget_views->topLevelItem(i);

			for (int j = 0; j < topLevelItem->childCount(); j++)
			{
				QTreeWidgetItem* childItem = topLevelItem->child(0);
				onViewChildItemClicked(childItem, childItem);
				break;

			}

		}

	}

	ui.treeWidget_views->blockSignals(false);
}

void Moonica_qt::onViewChildItemClicked(QTreeWidgetItem* item, QTreeWidgetItem* prevItem)
{
	//	qDebug() << "onImageInfoChildItemClicked";

	if (item == nullptr || item->text(0) == "Cam Views") return;

	stopWorkspaceLiveFeed();

	QString viewName = item->text(0);

	//Load view info from project data
	if (!_curProject._viewInfoHash.contains(viewName)) return;

	ViewInfo& vInfo = _curProject._viewInfoHash[viewName];
	_curView = vInfo;

	if (vInfo.viewPath.isEmpty()) //no image, show live feed
	{
		clearPolygon(true);

		m_workspaceLiveFeedThread = new QThread(this);
		m_workspaceLiveFeed = new VideoTester();
		m_workspaceLiveFeed->moveToThread(m_workspaceLiveFeedThread);

		connect(m_workspaceLiveFeedThread, &QThread::started, m_workspaceLiveFeed, [=]() {
			m_workspaceLiveFeed->startGrabbing(vInfo.cctvUrl); });
		connect(m_workspaceLiveFeed, &VideoTester::newFrameReady, this, &Moonica_qt::updateWorkspaceLiveFeed);
		connect(m_workspaceLiveFeedThread, &QThread::finished, m_workspaceLiveFeed, &QObject::deleteLater);
		connect(m_workspaceLiveFeedThread, &QThread::finished, m_workspaceLiveFeedThread, &QObject::deleteLater);

		m_workspaceLiveFeedThread->start();
		ui.pushButton_captureImage->setVisible(true);
	}
	else //got image
	{
		qDebug() << "viewPath found.";
		openViewImage(viewName);
		ui.pushButton_captureImage->setVisible(false);
	}


	//penViewImage(imageName);

	//// Set an icon on the selected item
	//QIcon selectedIcon(":/24x24/Icon/24x24/cil-paperclip.png"); // Replace with the path to your icon
	//item->setIcon(0, selectedIcon); // Set the icon for column 0

	//if (prevItem != nullptr) {

	//	prevItem->setIcon(0, QIcon()); // Clear the icon from the previous item
	//}


}

bool Moonica_qt::openFloorImage()
{
	QString floorViewPath = _curProject._floorViewInfo.viewPath;
	qDebug() << "Floow View Path " << floorViewPath;

	if (QFileInfo::exists(floorViewPath))
	{

		_imgFloor = cv::imread(floorViewPath.toStdString());
		QImage image(_curProject._floorViewInfo.viewPath);
		if (!image.isNull()) {

			displayImageInScene(QPixmap::fromImage(image), FLOOR_VIEW);

			_floorPlanScene->clear();
			QGraphicsPixmapItem* floorplanItem = _floorPlanScene->addPixmap(QPixmap::fromImage(image));

			ui.graphicsView_viewFloorPlan2->fitInView(floorplanItem, Qt::KeepAspectRatio);			//unchanged when open

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}
}

void Moonica_qt::openViewImage(QString viewName)
{
	qDebug() << "Open View Image";
	if (_curProject._viewInfoHash.contains(viewName))
	{
		clearPolygon(true);
		_curView = _curProject._viewInfoHash[viewName];


		QImage image;
		QString imagePath = _curView.viewPath;
		_imgCam = cv::imread(imagePath.toStdString());


		image.load(imagePath);
		qDebug() << "Image Path:" << imagePath;
		if (!image.isNull()) {

			displayImageInScene(QPixmap::fromImage(image), CAM_VIEW);
		}


		if (_curWorkingMode == CALIBRATION_MODE)
		{
			for (auto& pt : _curView.imagePointVec)
			{
				QString polyGrabberID = QString::number(_polyGrabber_camView.size());
				drawPolygonGrabber(polyGrabberID, pt.x(), pt.y(), 6, Qt::red, Qt::yellow, CAM_VIEW);
			}
			addPolygonObject(CAM_VIEW);

			//	qDebug() << "[openViewImage] _curView.floorPointVec size: " << _curView.floorPointVec.size();
			for (auto& pt : _curView.floorPointVec)
			{
				QString polyGrabberID = QString::number(_polyGrabber_floorView.size());
				drawPolygonGrabber(polyGrabberID, pt.x(), pt.y(), 20, Qt::red, Qt::yellow, FLOOR_VIEW);
			}
			addPolygonObject(FLOOR_VIEW);
		}
		else if (_curWorkingMode == ENTRANCE_EXIT_MODE)
		{
			for (auto& pt : _curView.doorPointVec)
			{
				QString polyGrabberID = QString::number(_polyGrabber_camView.size());
				drawPolygonGrabber(polyGrabberID, pt.x(), pt.y(), 6, Qt::red, Qt::yellow, CAM_VIEW);
			}
			addPolygonObject(CAM_VIEW);
		}


		//updateViewHash();
		openPerspectiveImage();
	}

}

void Moonica_qt::openPerspectiveImage()
{
	if (m_perspectiveViewScene) m_perspectiveViewScene->clear();
	if (!_imgCam.empty() && !_curView.homographyMatrix.empty() && !_imgFloor.empty())
	{
		cv::Mat birdEye;
		cv::warpPerspective(_imgCam, birdEye, _curView.homographyMatrix, _imgFloor.size());

		QImage qimg1 = cvMat_to_QImage(birdEye);


		displayImageInScene(QPixmap::fromImage(qimg1), PERSPECTIVE_VIEW);
	}
}


void Moonica_qt::updateViewHash()
{

	if (_curWorkingMode == WorkingMode::CALIBRATION_MODE)
	{
		_curView.imagePointVec.clear();
		for (auto& poly : _dragPolygonROI_camView)
		{
			for (auto& pt : poly->getPolygon())
			{
				_curView.imagePointVec.append(pt);
			}

		}
		_curView.floorPointVec.clear();
		for (auto& poly : _dragPolygonROI_floorView)
		{
			for (auto& pt : poly->getPolygon())
			{
				_curView.floorPointVec.append(pt);
			}
		}
	}
	
	if (_curWorkingMode == WorkingMode::ENTRANCE_EXIT_MODE)
	{
		_curView.doorPointVec.clear();
		for (auto& poly : _dragPolygonEntranceExitRoi_camView)
		{
			for (auto& pt : poly->getPolygon())
			{
				_curView.doorPointVec.append(pt);
			}
		}
	}

	



	if (_curProject._viewInfoHash.contains(_curView.viewName))
	{

		_curProject._viewInfoHash[_curView.viewName] = _curView;
		_curProject._viewList = _curProject._viewInfoHash.values();
		_curProject.saveProject();
	}

}


void Moonica_qt::openProject(QString recipeName)
{
	ui.toolButton_workspace->setEnabled(true);
	ui.toolButton_camTest->setEnabled(true);
	ui.toolButton_settings->setEnabled(true);


	// clear all video stream 
	qDebug() << "[Open Project] Stopping Video Test...";
	clearCamSelectionResources();
	clearStreamingThread();
	stopVideoTest();

	qDebug() << "[Open Project] Stopping Workspace Live Feed...";
	stopWorkspaceLiveFeed();
	
	clearPolygon(true);
	clearAllView();

	_curProject.loadProject(recipeName);
	_curProject._viewList = _curProject._viewInfoHash.values();


	initializeProjectSetting();
	initStreamingThread();
	openFloorImage();// open floor image must do first, draw polygon will fail on click refresh view
	refreshViewInfoTreeWidget();

	showWorkPage();


	ui.pushButton_projectName->setText(_curProject._projectName);

}

void Moonica_qt::initProjectList()
{
	QDir dir(PathManager::_projectListDir);

	// Only list directories, excluding "." and ".."
	QStringList folderList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

	ui.listWidget_projectList->clear();
	ui.listWidget_projectList->addItems(folderList);


}

void Moonica_qt::addProject(QString recipeName)
{
	QDir dir(PathManager::_projectListDir);
	QStringList folderList = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

	if (folderList.contains(recipeName))
	{
		QMessageBox::warning(this, tr("Add recipe failed"),
			tr("Duplicated recipe name detecteed"));
		return;
	}
	if (recipeName.isEmpty()) return;

	QString recipePath = PathManager::_projectListDir + recipeName;
	PathManager::makeDir(recipePath);

	initProjectList();

}


void Moonica_qt::deleteProject(QString recipeName)
{
	if (recipeName.isEmpty()) return;

	QString recipePath = PathManager::_projectListDir + recipeName;

	if (QMessageBox::Yes == QMessageBox(QMessageBox::Information,
		"Delete Recipe", "Are you sure to delete <br>" + recipePath + " ? <br> Action cannot be reverted!"
		, QMessageBox::Yes | QMessageBox::No).exec())
	{
		PathManager::clearDirectory(recipePath);
		QDir(recipePath).removeRecursively();

		initProjectList();
	}
}



void Moonica_qt::clearGridLayout(QGridLayout* layout, QWidget* protectedWidget)
{
	if (!layout) return;

	// remove all grid for cctv
	while (QLayoutItem* item = layout->takeAt(0))
	{
		QWidget* widget = item->widget();
		if (widget)
		{
			if (widget == ui.graphicsView_viewFloorPlan2)
			{
				layout->removeWidget(widget);
			}
			else
			{
				delete widget;
			}
		}
		delete item;
	}
}




void Moonica_qt::updateTrackingDots(const std::vector<OnnxResult>& detections, int currentCameraId)
{
	QSet<QString> currentFrameLocalKeys;
	for (const auto& track : detections)
	{
		currentFrameLocalKeys.insert(QString("%1-%2").arg(currentCameraId).arg(track.tracking_id));
	}

	if (currentCameraId < 0 || currentCameraId >= _curProject._viewList.size())
	{
		qWarning() << "Received results for invalid camera ID: " << currentCameraId;
		return;
	}

	const ViewInfo& currentCamInfo = _curProject._viewList.at(currentCameraId);

	if (currentCamInfo.homographyMatrix.empty())
	{
		qWarning() << "Homography matrix for camera" << currentCamInfo.viewName << "is not calculated.";
		return;
	}

	for (const auto& track : detections)
	{
		int local_id = track.tracking_id;
		cv::Point2f camera_point((track.x1 + track.x2) / 2.0f, track.y2);

		//if (_curView.homographyMatrix.empty()) {
		//	qWarning() << "Cannot update tracking dots, homography matrix is not calculated.";
		//	continue;
		//}

		std::vector<cv::Point2f> src_points = { camera_point };
		std::vector<cv::Point2f> dst_points;
		cv::perspectiveTransform(src_points, dst_points, currentCamInfo.homographyMatrix);

		QPointF floor_plan_point(dst_points[0].x, dst_points[0].y);

		for (QGraphicsEllipseItem* existingDot : _trackingDots.values())
		{
			int existingCamId = existingDot->data(0).toInt();
			int existingTrackId = existingDot->data(1).toInt();

			if (existingCamId != currentCameraId)
			{
				qreal distance = QLineF(floor_plan_point, existingDot->pos()).length();

				if (distance <= 70.0)
				{
					_idManager->unify(currentCameraId, local_id, existingCamId, existingTrackId);
				}
			}
		}


		//compare position of dot
		qDebug() << "  [Track ID:" << local_id << "] Transformed Camera Pt (" << camera_point.x << "," << camera_point.y
			<< ") ===> Floor Plan Pt (" << floor_plan_point.x() << "," << floor_plan_point.y() << ")";


		QString dotKey = QString("%1-%2").arg(currentCameraId).arg(local_id);
		if (_trackingDots.contains(dotKey))
		{
			_trackingDots[dotKey]->setPos(floor_plan_point);
		}
		else
		{
			//new id new dot
			QGraphicsEllipseItem* dot = new QGraphicsEllipseItem(0, 0, 140, 140);
			dot->setPos(floor_plan_point);
			dot->setZValue(1);
			cv::Scalar color;
			dot->setBrush(QBrush(QColor(color[2], color[1], color[0])));
			dot->setPen(Qt::NoPen);
			dot->setData(0, currentCameraId);
			dot->setData(1, local_id);
			_floorPlanScene->addItem(dot);
			_trackingDots[dotKey] = dot;
		}
	}

	//remove old dots
	QList<QString> keysToRemove;

	for (const QString& key : _trackingDots.keys())
	{

		if (key.startsWith(QString::number(currentCameraId) + "-"))
		{

			if (!currentFrameLocalKeys.contains(key))
			{
				keysToRemove.append(key);
			}
		}
	}

	for (const QString& key : keysToRemove)
	{
		_floorPlanScene->removeItem(_trackingDots[key]);
		delete _trackingDots[key];
		_trackingDots.remove(key);
	}
}



//settings start
void Moonica_qt::stopVideoTest()
{
	if (m_videoTestThread/* && m_videoTestThread->isRunning()*/)
	{
		
		if (m_videoTester)
		{
			m_videoTester->stopGrabbing(); // Tell the worker to stop its loop
		}
		m_videoTestThread->quit(); // Tell the thread's event loop to exit
	}
	
}



void Moonica_qt::updateVideoTestDisplay(const QImage& image)
{
	if (ui.label_cctvFeedPreview)
	{
		ui.label_cctvFeedPreview->setPixmap(QPixmap::fromImage(image)
			.scaled(ui.label_cctvFeedPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
	}
}

void Moonica_qt::onVideoTestError(const QString& errorMessage)
{
	stopVideoTest();
	QMessageBox::critical(this, "Connection Error", errorMessage);
	ui.label_cctvFeedPreview->setText("Connection Failed");
}

void Moonica_qt::onUrlInputTimeout()
{
	if (m_videoTestThread || m_videoTester) return;  //dont start new test if old object exists

	QString url = ui.lineEdit_cctvUrl->text();
	if (url.length() < 10 || !url.startsWith("rtsp://", Qt::CaseInsensitive)) {
		ui.label_cctvFeedPreview->setText("Invalid URL format");
		return;
	}
	//stopVideoTest(); // Ensure no test is running

	m_videoTestThread = new QThread(this);
	m_videoTester = new VideoTester();
	m_videoTester->moveToThread(m_videoTestThread);

	connect(m_videoTestThread, &QThread::started, m_videoTester, [=]() { m_videoTester->startGrabbing(url); });
	connect(m_videoTester, &VideoTester::newFrameReady, this, &Moonica_qt::updateVideoTestDisplay);
	//	connect(m_videoTester, &VideoTester::error, this, &Moonica_qt::onVideoTestError);

	//	connect(m_videoTester, &VideoTester::finished, m_videoTestThread, &QThread::quit);
	connect(m_videoTestThread, &QThread::finished, m_videoTester, &VideoTester::deleteLater);
	connect(m_videoTestThread, &QThread::finished, m_videoTestThread, &QThread::deleteLater);

	//no crash on entering new cctv url in settings
	connect(m_videoTestThread, &QObject::destroyed, this, [this]() {
		m_videoTestThread = nullptr;
		});
	connect(m_videoTester, &QObject::destroyed, this, [this]() {
		m_videoTester = nullptr;
		});

	//	connect(m_videoTestThread, &QThread::started, m_videoTester, [=]() { m_videoTester->startGrabbing(url); });
	//	connect(m_videoTester, &VideoTester::newFrameReady, this, &Moonica_qt::updateVideoTestDisplay);

	m_videoTestThread->start();
	ui.label_cctvFeedPreview->setText("Connecting...");

}

void Moonica_qt::stopWorkspaceLiveFeed()
{
	if (!m_workspaceLiveFeedThread)
	{
		ui.pushButton_captureImage->setVisible(false);
		return;
	}

	if (m_workspaceLiveFeed)
	{
		disconnect(m_workspaceLiveFeed, nullptr, nullptr, nullptr);
		m_workspaceLiveFeed->stopGrabbing();
	}

	m_workspaceLiveFeedThread->quit();
	m_workspaceLiveFeed = nullptr;
	m_workspaceLiveFeedThread = nullptr;
	ui.pushButton_captureImage->setVisible(false);
}


void Moonica_qt::updateWorkspaceLiveFeed(const QImage& image)
{

	m_currentCaptureFrame = image; //save latest captured frame

	if (m_pixmapItem_camView == nullptr)
	{

		m_pixmapItem_camView = m_scene_camView->addPixmap(QPixmap::fromImage(image));
	}

	else
	{

		//m_pixmapItem_camView = m_scene_camView->addPixmap(QPixmap::fromImage(image));
		m_pixmapItem_camView->setPixmap(QPixmap::fromImage(image));
	}

	if (m_pixmapItem_camView)
	{

		ui.graphicsView_viewCam->fitInView(m_pixmapItem_camView, Qt::KeepAspectRatio);
	}

}
//settings end

//coords
void Moonica_qt::updateFloorPlanCoords(const QPoint& receivedPos)
{

	if (_curProject._floorViewInfo.viewPath.isEmpty())
	{

		return;
	}
	if (!m_pixmapItem_floorView)
	{
		ui.label_mouseCoords->setText("X: --, Y: --");
		return;
	}
	QPointF scenePos = receivedPos;


	QPointF localPos = m_pixmapItem_floorView->mapFromScene(scenePos);
	//QPointF itemPos = m_pixmapItem_floorView->mapFromScene(scenePos);
	//QRectF pixmapBounds = m_pixmapItem_floorView->boundingRect();

	//no negative coords
	if (localPos.x() >= 0 && localPos.x() < m_pixmapItem_floorView->pixmap().width() &&
		localPos.y() >= 0 && localPos.y() < m_pixmapItem_floorView->pixmap().height())
	{
		QString coordsText = QString("X: %1, Y: %2").arg(scenePos.x(), 0, 'f', 0).arg(scenePos.y(), 0, 'f', 0);
		ui.label_mouseCoords->setText(coordsText);
	}
	else
	{
		ui.label_mouseCoords->setText("X: --, Y: --");
	}
}

QVector<QPointF> Moonica_qt::getBirdEyePolygon(const cv::Mat& camImg, const cv::Mat& H, const cv::Mat& floorImg)
{
	// Original corners of the camera image
	std::vector<cv::Point2f> camCorners = {
		cv::Point2f(0, 0),
		cv::Point2f(static_cast<float>(camImg.cols), 0),
		cv::Point2f(static_cast<float>(camImg.cols), static_cast<float>(camImg.rows)),
		cv::Point2f(0, static_cast<float>(camImg.rows))
	};

	// Transform to floor plane
	std::vector<cv::Point2f> floorCorners;
	cv::perspectiveTransform(camCorners, floorCorners, H);

	// Convert to QPointF and clamp within floor image bounds
	QVector<QPointF> qCorners;
	qCorners.reserve(static_cast<int>(floorCorners.size()));
	for (auto& pt : floorCorners) {
		float x = std::max(0.0f, std::min(pt.x, static_cast<float>(floorImg.cols - 1)));
		float y = std::max(0.0f, std::min(pt.y, static_cast<float>(floorImg.rows - 1)));
		qCorners.append(QPointF(x, y));
	}

	return qCorners;
}

QPointF Moonica_qt::findOutlier(const QVector<QPointF>& points, double k)
{
	if (points.isEmpty())
		return QPointF(); // no data

	// 1. Compute centroid
	double sumX = 0, sumY = 0;
	for (const auto& p : points) {
		sumX += p.x();
		sumY += p.y();
	}
	QPointF centroid(sumX / points.size(), sumY / points.size());

	// 2. Compute distances from centroid
	QVector<double> distances;
	distances.reserve(points.size());
	for (const auto& p : points) {
		double d = qSqrt(qPow(p.x() - centroid.x(), 2) +
			qPow(p.y() - centroid.y(), 2));
		distances.append(d);
	}

	// 3. Compute mean & std deviation of distances
	double mean = 0;
	for (double d : distances) mean += d;
	mean /= distances.size();

	double variance = 0;
	for (double d : distances) variance += qPow(d - mean, 2);
	variance /= distances.size();
	double stdDev = qSqrt(variance);

	// 4. Find farthest point that exceeds threshold
	double maxDist = -1;
	int outlierIdx = -1;
	for (int i = 0; i < distances.size(); i++) {
		if (distances[i] > mean + k * stdDev && distances[i] > maxDist) {
			maxDist = distances[i];
			outlierIdx = i;
		}
	}

	qDebug() << "Mean distance: " << mean;

	if (outlierIdx >= 0)
		return points[outlierIdx]; // return the outlier point
	else
		return QPointF(); // no outlier
}


void Moonica_qt::setAsDoorCamView()
{
	QTreeWidgetItem* item = ui.treeWidget_views->currentItem();

	if (item)
	{
		QString viewName = item->text(0);
		if (_curProject._viewInfoHash.contains(viewName))
		{
			_curProject._viewInfoHash[viewName].isDoorCam = true;

			QIcon selectedIcon(":/ResultViewer/Icon/16x16/cil-door.png"); // Replace with the path to your icon
			item->setIcon(0, selectedIcon);

		}

	}
}


void Moonica_qt::removeDoorCamView()
{
	QTreeWidgetItem* item = ui.treeWidget_views->currentItem();

	if (item)
	{
		QString viewName = item->text(0);
		if (_curProject._viewInfoHash.contains(viewName))
		{
			_curProject._viewInfoHash[viewName].isDoorCam = false;

			item->setIcon(0, QIcon());
		}
	}
}

void Moonica_qt::loadOnnxModelsToCombo(
	QComboBox* combo,
	const QString& modelDir
)
{
	combo->blockSignals(true);
	combo->clear();

	QDir dir(modelDir);
	if (!dir.exists())
	{
		qWarning() << "Model dir not found:" << modelDir;
		return;
	}

	QStringList filters;
	filters << "*.onnx";

	QFileInfoList fileList = dir.entryInfoList(
		filters,
		QDir::Files | QDir::Readable,
		QDir::Name
	);

	for (const QFileInfo& fileInfo : fileList)
	{
		combo->addItem(fileInfo.fileName(),
			fileInfo.absoluteFilePath()); // store full path
	}
	combo->blockSignals(false);
}

// ai setting page
void Moonica_qt::initializeProjectSetting()
{
	loadOnnxModelsToCombo(
		ui.comboBox_humanTrackingModelName,
		PathManager::_humanTrackingModelDir
	);

	loadOnnxModelsToCombo(
		ui.comboBox_objectDetectionModelName,
		PathManager::_objectDetectionModelDir
	);

	
	ui.comboBox_humanTrackingModelName->setCurrentIndex(ui.comboBox_humanTrackingModelName->findText(_curProject._settings.humanTracking_modelName));
	ui.doubleSpinBox_humanTrackingAccuracy->setValue(_curProject._settings.humanTracking_accuracy);
	ui.checkBox_enableFeatureRecognition->setChecked(_curProject._settings.humanTracking_enableFeatureExtraction);
	

	ui.comboBox_objectDetectionModelName->setCurrentIndex(ui.comboBox_objectDetectionModelName->findText(_curProject._settings.objectDetection_modelName));
	ui.doubleSpinBox_objectDetectionAccuracy->setValue(_curProject._settings.objectDetection_accuracy);

	ui.spinBox_viewRow->setValue(_curProject._settings.viewRow);
	ui.spinBox_viewCol->setValue(_curProject._settings.viewCol);

	ui.listWidget_viewSeqience->clear();
	for (auto& vl : _curProject._settings.viewSequenceList)
	{
		ui.listWidget_viewSeqience->addItem(vl);
	}
}