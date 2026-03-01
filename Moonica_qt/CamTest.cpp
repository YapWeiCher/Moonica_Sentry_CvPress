#include "Moonica_qt.h"

void Moonica_qt::addCameraStream(QString cameraId, QString cameraUrl, int camIndex)
{
	_frameManager->addCameraStream(cameraId, cameraUrl);
	CameraDisplay camDisplay;
	camDisplay.camScene = new QMainGraphicsScene();
	camDisplay.camView = new QMainGraphicsView();
	camDisplay.camView->setEnableZoom(false);
	camDisplay.camView->setScene(camDisplay.camScene);
	camDisplay.camView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	camDisplay.camView->setMinimumHeight(ui.scrollArea_cameraView->height() / _curProject._settings.viewRow);
	camDisplay.camView->setMaximumHeight(ui.scrollArea_cameraView->height() / _curProject._settings.viewRow);
	

	camDisplay.camPixmapItem = nullptr; // Will be created on first frame
	camDisplay.camView->resetTransform();
	camDisplay.camView->fitInView(camDisplay.camView->sceneRect(),Qt::KeepAspectRatio);
	_cameraDisplayHash.insert(cameraId, camDisplay);


	FrameGrabberThread* fgThread = new FrameGrabberThread(cameraId, cameraUrl);
	_frameGrabberHash.insert(cameraId, fgThread);


	qRegisterMetaType<cv::Mat>("cv::Mat");
	QObject::connect(fgThread, &FrameGrabberThread::frameReady,
		_frameManager, &FrameManager::onFrameReady,
		Qt::QueuedConnection); // important for cross-thread

	// Retrieve the grid layout from scroll area
	auto gridLayout = reinterpret_cast<QGridLayout*>(
		ui.scrollArea_cameraView->property("gridLayout").value<void*>());


	if (!gridLayout) return;

	// Create a widget with label + camera view

	QWidget* camWidget = createCameraWidget(cameraId, camDisplay.camView);

	// Place it in 2x2 grid
	int row = camIndex / _curProject._settings.viewCol;

	int col = camIndex % _curProject._settings.viewCol;


	gridLayout->addWidget(camWidget, row, col);
}


QWidget* Moonica_qt::createCameraWidget(const QString& cameraId, QGraphicsView* view)
{
	QWidget* container = new QWidget();
	QVBoxLayout* vLayout = new QVBoxLayout(container);
	vLayout->setContentsMargins(2, 2, 2, 2);
	vLayout->setSpacing(2);

	QLabel* label = new QLabel(cameraId);
	label->setAlignment(Qt::AlignCenter);
	label->setStyleSheet("background-color: #333; color: white; font-weight: bold;");

	vLayout->addWidget(label);
	vLayout->addWidget(view);

	return container;
}

void Moonica_qt::clearFloor2Polygons()
{
	for (auto* polyItem : _sceneFloor2PolyVector) {
		if (polyItem) {
			_floorPlanScene->removeItem(polyItem);
			delete polyItem; // important to free memory
		}
	}
	_sceneFloor2PolyVector.clear();
}

void Moonica_qt::clearCamSelectionResources()
{
	qDebug() << "========== clearCamSelectionResources START ==========";
	_globalStreamingReadyFlag = false;
	camViewSelectOff_all();

	// Note: Must clear thread only clear widget
	// Else will crash randomly when this fucntion is called
	
	// ---------- Reset palette index ----------
	_colorPalleteIndex = 0;
	
	
	
	qDebug() << "[Cleanup] Clearing camera view";
	clearCamView();

	qDebug() << "[Cleanup] Clearing floor polygons";
	clearFloor2Polygons();

	qDebug() << "========== clearCamSelectionResources END ==========";
}

void Moonica_qt::initCamSelection()
{
	clearCamSelectionResources();
	
	_frameManager->setAiSetting(_curProject._settings);
	_trackManager->setCriteria(_checkingCriteria);

	_globalStreamingReadyFlag = true;

	ui.radioButton_liveMode->setChecked(true);
	ui.scrollArea_objectImage->hide();
	
	// Create new container + layout
	_camSelectionContainer = new QWidget();
	QVBoxLayout* layout = new QVBoxLayout(_camSelectionContainer);

	int camIndex = 0;

	QStringList camArrangement;
	

	for (int i = 0; i < _curProject._settings.viewSequenceList.size(); i++)
	{
		camArrangement.append(_curProject._settings.viewSequenceList[i]);
	}

	// == progress bar setup == //
	int total = 0;
	for (auto& cA : camArrangement)
	{
		for (auto& cam : _curProject._viewInfoHash)
		{
			if (cA == cam.viewName)
				total++;
		}
	}

	// Create pop-up progress dialog
	// Create progress dialog
	QProgressDialog* progressDialog = new QProgressDialog("Loading cameras...", QString(), 0, total, this);

	// Frameless & no close button
	progressDialog->setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	progressDialog->setWindowModality(Qt::WindowModal); // Blocks main window interaction
	progressDialog->setCancelButton(nullptr); // Remove cancel button
	progressDialog->setMinimumDuration(0);    // Show immediately

	// Customize progress bar color
	QProgressBar* bar = progressDialog->findChild<QProgressBar*>();
	if (bar)
	{
		bar->setStyleSheet(
			"QProgressBar {"
			"  border: 1px solid grey;"
			"  border-radius: 5px;"
			"  text-align: center;"
			"}"
			"QProgressBar::chunk {"
			"  background-color: purple;"
			"  width: 10px;"
			"}"
		);
	}

	progressDialog->show();
	QCoreApplication::processEvents(); // Keep UI responsive

	// Example: updating in loop
	int progress = 0;
	// == progress bar setup == //




	for (auto& cA : camArrangement)
	{
		for (auto& cam : _curProject._viewInfoHash)
		{

			QString camName = cam.viewName;
			if (cA != camName) continue;
		
			QCheckBox* checkBox = new QCheckBox(camName);
			layout->addWidget(checkBox);
		

			connect(checkBox, &QCheckBox::toggled, this, [=](bool checked) {
				if (checked)
				{
					camViewSelectOn(camName);
				}
				else
				{
					camViewSelectOff(camName);
				}
			});
			_camSelectionCheckBoxs.append(checkBox);

			addCameraStream(camName, cam.cctvUrl, camIndex);
			camIndex++;


			if (true)
			{
				QPolygonF poly;
				for (auto& p : cam.perspectivePointVec)
				{
					poly << p;
				}

				QColor polyColor = QColor(_colorPallete[_colorPalleteIndex]);
				_colorPalleteIndex += 5;
				if (_colorPalleteIndex >= _colorPallete.size()) _colorPalleteIndex = 0;

				QPen pen(polyColor);
				pen.setWidth(2);

				// Same color as pen, but with transparency
				QColor fillColor = polyColor;
				fillColor.setAlphaF(0.30); // 80% visible

				QBrush brush(fillColor);

				QGraphicsPolygonItem* polyItem = _floorPlanScene->addPolygon(poly, pen, brush);
				_sceneFloor2PolyVector.append(polyItem);



				progress++;
				progressDialog->setValue(progress);  // keep dialog state correct

				QCoreApplication::processEvents();

			
			}


		}
	}

	progressDialog->close();
	progressDialog->deleteLater();

	layout->addStretch();
	_camSelectionContainer->setLayout(layout);

	ui.scrollArea_camViewSelection->setWidget(_camSelectionContainer);
	ui.scrollArea_camViewSelection->setWidgetResizable(true);

	ui.checkBox_checkGlove->setChecked(_checkingCriteria.checkGlove);
	ui.checkBox_checkSmock->setChecked(_checkingCriteria.checkSmock);
	ui.checkBox_checkMask->setChecked(_checkingCriteria.checkMask);
	ui.checkBox_checkShoe->setChecked(_checkingCriteria.checkShoe);

	ui.checkBox_viewPersonBbox->setChecked(true);
	ui.checkBox_viewPersonStatus->setChecked(true);
	ui.checkBox_viewPersonSkeleton->setChecked(true);
	ui.checkBox_viewObjectBbox->setChecked(true);
	ui.checkBox_viewCheckingArea->setChecked(true);

	ui.spinBox_statusFontSize->setValue(6);
	ui.spinBox_boundingBoxSize->setValue(2);

	if (_curProject._settings.isGlobalViewChecking)
	{
		ui.radioButton_globalTracking->setChecked(true);
	}
	else
	{
		ui.radioButton_singleViewTracking->setChecked(true);
	}
	

}

void Moonica_qt::clearCamView()
{
	// clear camere view grid
	auto gridLayout = reinterpret_cast<QGridLayout*>(
		ui.scrollArea_cameraView->property("gridLayout").value<void*>());

	if (!gridLayout)
	{
		qWarning() << "gridLayout is null!";
		return;
	}

	// Remove all items
	QLayoutItem* item;
	while ((item = gridLayout->takeAt(0)) != nullptr)
	{
		if (item->widget())
		{
			item->widget()->deleteLater();  // safer than delete
		}
		delete item;
	}

	QWidget* oldWidget = ui.scrollArea_camViewSelection->takeWidget();
	if (oldWidget) {
		oldWidget->deleteLater();   // schedules safe deletion
	}
	
	if (_camSelectionContainer)
	{
		qDebug() << "[Cleanup] Clearing camera selection UI";
		ui.scrollArea_camViewSelection->takeWidget();
		_camSelectionContainer->deleteLater();
		_camSelectionContainer = nullptr;
	}
	else
	{
		qDebug() << "[Cleanup] Camera selection UI already null";
	}
	_camSelectionCheckBoxs.clear();

	for (auto it = _frameGrabberHash.begin(); it != _frameGrabberHash.end(); ++it) {
		FrameGrabberThread* thread = it.value();
		if (thread) {
			thread->stopFrameGrabbing();
			thread->quit();   // ask thread to exit
			thread->wait();   // wait until finished
			thread->deleteLater(); // or delete thread; if you're sure it's safe
		}
	}
	_cameraDisplayHash.clear();
	_frameGrabberHash.clear();
}

void Moonica_qt::camViewSelectOn(QString camID, bool isVideoMode)
{
	if (_frameGrabberHash.contains(camID))
	{
		_frameGrabberHash[camID]->setVideoMode(isVideoMode);
		_frameGrabberHash[camID]->start();

	}
	_frameManager->activateCamera(camID);

	cv::Mat homographyMatrix = _curProject._viewInfoHash[camID].homographyMatrix;
	QVector<QPointF> doorPoints = _curProject._viewInfoHash[camID].doorPointVec;
	QVector<QPointF> perspectivePoints = _curProject._viewInfoHash[camID].perspectivePointVec;
	double imageWidth = _curProject._viewInfoHash[camID].viewWidth;
	double imageHeight = _curProject._viewInfoHash[camID].viewHeight;
	bool isDoorView = _curProject._viewInfoHash[camID].isDoorCam;

	/*if (!homographyMatrix.empty())
	{
		_trackManager->addActiveCamera(camID, homographyMatrix, doorPoints, perspectivePoints, imageWidth, imageHeight, isDoorView);
	}*/
	_trackManager->addActiveCamera(camID, homographyMatrix, doorPoints, perspectivePoints, imageWidth, imageHeight, isDoorView);
}

void Moonica_qt::camViewSelectOff(QString camID)
{
	if (_frameGrabberHash.contains(camID))
	{

		_frameGrabberHash[camID]->stopFrameGrabbing();

	}

	_frameManager->deActivateCamera(camID);
	_trackManager->removeActiveCamera(camID);

	
}

void Moonica_qt::camViewSelectOn_all()
{
	for (auto& cam : _curProject._viewInfoHash)
	{
		camViewSelectOn(cam.viewName);
	}
}

void Moonica_qt::camViewSelectOff_all()
{
	for (auto& cam : _curProject._viewInfoHash)
	{

		camViewSelectOff(cam.viewName);
	}
}

void Moonica_qt::removeImageObject(QString globalId)
{
	if (_objectContainers.contains(globalId)) {
		QWidget* container = _objectContainers.value(globalId);

		ui.verticalLayout_objectImage->removeWidget(container);
		container->deleteLater();

		_objectContainers.remove(globalId);
		_gloveLabels_left.remove(globalId);
		_gloveLabels_right.remove(globalId);
		_jumpsuitLabels.remove(globalId);
	}
}

QColor Moonica_qt::getColorForID(int id) {
	// Use a simple hash to generate repeatable RGB color from ID
	int r = 50 + (id * 97) % 206;  // 50–255 range
	int g = 50 + (id * 57) % 206;
	int b = 50 + (id * 23) % 206;

	return QColor(r, g, b); // QColor uses RGB order
}

QString formatTime(double seconds)
{
	int minutes = static_cast<int>(seconds) / 60;
	double sec = fmod(seconds, 60.0);

	return QString("%1:%2")
		.arg(minutes, 2, 10, QChar('0'))
		.arg(QString::number(sec, 'f', 2).rightJustified(5, '0'));
}

void Moonica_qt::updateCameraGraphicView(QString cameraId, QImage frame, std::vector<OnnxResult> oResult, std::vector<OnnxResult> od_Result, double currentSec)
{
	
	if (!_globalStreamingReadyFlag) return;

	if (frame.isNull()) {
		qDebug() << "Null Frame";
		return;
	}
	


	if (false) // render object image
	{

		for (auto it = _objectCamTrackingId.begin(); it != _objectCamTrackingId.end(); ++it)
		{

			const QString& globalId = it.key();
			const QStringList& trackingIds = it.value();

			QImage img = _objectImage[globalId];

			if (img.isNull()) {

				for (auto tId : trackingIds)
				{
					if (tId.split("[@]").size() != 2) continue;
					QString camId = tId.split("[@]")[0];
					QString trackingId = tId.split("[@]")[1];

					if (camId == cameraId)
					{

						for (auto& oR : oResult)
						{

							if (QString::number(oR.tracking_id) == trackingId)
							{

								QRectF objectRect(oR.x1, oR.y1, oR.x2 - oR.x1, oR.y2 - oR.y1);
								// Convert to QRect with integer coordinates
								QRect cropRect = objectRect.toRect();
								// Make sure the crop rectangle is within the frame bounds
								cropRect = cropRect.intersected(frame.rect());
								QImage cropped = frame.copy(cropRect);
								_objectImage[globalId] = cropped;
							}
						}
					}
				}
			}
			else {
				_globalIDImageProcessed.append(globalId);
				if (_globalIDImageProcessed.size() > 300) {
					_globalIDImageProcessed.removeFirst();
				}
				//qDebug() << "Image for globalId" << globalId << "is OK, size:" << img.size();
			}
		}

		int row = 0;
		for (auto it = _objectImage.begin(); it != _objectImage.end(); ++it)
		{

			const QString& globalId = it.key();
			const QImage& img = it.value();
			if (_globalIDImageProcessed.contains(globalId)) continue;


			if (!img.isNull()) {

				QColor borderColor = QColor(_globalIdColor[globalId]);


				QImage resized = img.scaled(100, 100, Qt::IgnoreAspectRatio);
				// Title label
				QLabel* title = new QLabel(globalId, ui.widget_ObjectImage);
				title->setAlignment(Qt::AlignCenter);
				QFont font = title->font();
				font.setPointSize(16);        // increase font size (default ~9–10)
				font.setBold(true);           // optional: make bold
				title->setFont(font);


				// has GLove label
				QLabel* gloveLabel_left = new QLabel("Left Glove Pass:", ui.widget_ObjectImage);
				gloveLabel_left->setAlignment(Qt::AlignLeft);

				// has GLove label
				QLabel* gloveLabel_right = new QLabel("Right Glove Pass:", ui.widget_ObjectImage);
				gloveLabel_right->setAlignment(Qt::AlignLeft);

				// has jumpsui label
				QLabel* jumpSuitLabel = new QLabel("JumpSuit Pass:", ui.widget_ObjectImage);
				jumpSuitLabel->setAlignment(Qt::AlignLeft);

				// store for later access
				_gloveLabels_left.insert(globalId, gloveLabel_left);
				_gloveLabels_right.insert(globalId, gloveLabel_right);
				_jumpsuitLabels.insert(globalId, jumpSuitLabel);

				// Image label
				QLabel* imgLabel = new QLabel(ui.widget_ObjectImage);
				imgLabel->setPixmap(QPixmap::fromImage(resized));
				imgLabel->setScaledContents(true);

				// Stack title + image vertically
				QVBoxLayout* vbox = new QVBoxLayout;
				vbox->addWidget(title);
				vbox->addWidget(imgLabel);
				vbox->addWidget(gloveLabel_left);
				vbox->addWidget(gloveLabel_right);
				vbox->addWidget(jumpSuitLabel);
				vbox->setStretch(1, 1);

				// Wrap into a container widget
				QWidget* container = new QWidget(ui.widget_ObjectImage);
				container->setLayout(vbox);



				QString style = QString(
					"QWidget#container_%1 { "
					"  border: 2px solid rgb(%2,%3,%4); "
					"  border-radius: 5px; "
					"}"
				).arg(globalId)
					.arg(borderColor.red())
					.arg(borderColor.green())
					.arg(borderColor.blue());

				container->setObjectName("container_" + globalId); // unique name
				container->setStyleSheet(style);



				// Place into grid
				ui.verticalLayout_objectImage->addWidget(container, row);
				_objectContainers.insert(globalId, container);

				row++;

			}
		}


	}

	if (true) // ui display
	{
		double fullVideoLength = _fullVideoStreamSecond;
		double countDown = fullVideoLength - currentSec;
		int currentMs = static_cast<int>(currentSec * 1000.0);

		ui.label_videoStreamLeft->setText(formatTime(currentSec));
		ui.label_videoStreamRight->setText(formatTime(countDown));

		// avoid feedback loop if user is dragging
		if (!ui.horizontalSlider_videoStream->isSliderDown())
			ui.horizontalSlider_videoStream->setValue(currentMs);

		QPixmap pixmap = QPixmap::fromImage(frame);

		auto& display = _cameraDisplayHash[cameraId];
		display.frame = frame;
		//_trackManager->onResultReady(cameraId, oResult, od_Result);

		QMainGraphicsView* camView = display.camView;
		QGraphicsPixmapItem* camPixmapItem = display.camPixmapItem;
		QMainGraphicsScene* camScene = display.camScene;
		QVector<QGraphicsRectItem*>& rectItems = display.rectItems;
		QVector<QGraphicsTextItem*>& textItems = display.textItems;
		QVector<QGraphicsLineItem*>& lineItems = display.lineItems;
		QVector<QGraphicsEllipseItem*>& ellipseItems = display.ellipseItems;
		QVector<QGraphicsPolygonItem*>& polygonsItems = display.polygonItems;

		// 1. Update background image
		if (true)
		{
			if (!camPixmapItem) {
				camPixmapItem = camScene->addPixmap(pixmap);
				display.camPixmapItem = camPixmapItem; // store back!
				camView->fitInView(camPixmapItem, Qt::KeepAspectRatio);
			}
			else {
				camPixmapItem->setPixmap(pixmap);
			}
		}


		if (true)
		{
			// 2. Clear old rects + texts
			for (auto rectItem : rectItems) {
				camScene->removeItem(rectItem);
				delete rectItem;
			}
			rectItems.clear();

			for (auto textItem : textItems) {
				camScene->removeItem(textItem);
				delete textItem;
			}
			textItems.clear();

			for (auto lineItem : lineItems) {
				camScene->removeItem(lineItem);
				delete lineItem;
			}
			lineItems.clear();

			for (auto ellipseItem : ellipseItems) {
				camScene->removeItem(ellipseItem);
				delete ellipseItem;
			}
			ellipseItems.clear();

			for (auto polygonItem : polygonsItems) {
				camScene->removeItem(polygonItem);
				delete polygonItem;
			}
			polygonsItems.clear();



			// 3. Add new rects + text
		
			QPen pen(Qt::red);
			pen.setWidth(ui.spinBox_boundingBoxSize->value());
			// drawFLoorPoint
			if (ui.checkBox_viewCheckingArea->isChecked())
			{
				QPolygonF poly;
				for (auto& p : _curProject._viewInfoHash[cameraId].doorPointVec)
				{
					poly << p;
				}

				QBrush brush(QColor(255, 165, 0, 128)); // orange with 50% transparency

				QGraphicsPolygonItem* polyItem = camScene->addPolygon(poly, pen, brush);


				polygonsItems.append(polyItem);
			}

			if (ui.checkBox_viewObjectBbox->isChecked())
			{
				
				for (auto& oR : od_Result)
				{
					QRectF r(oR.x1, oR.y1, oR.x2 - oR.x1, oR.y2 - oR.y1);
					// === Draw bounding box ===
					QGraphicsRectItem* rectItem = camScene->addRect(r, pen);
					rectItems.append(rectItem);

					// Lookup object class name
					QString className = "unknown";
					auto it = classNames.find(oR.obj_id);
					if (it != classNames.end()) {
						className = QString::fromStdString(it->second);
					}

					QString label = QString("%1").arg(className);

					// Create text item
					QFont font("Arial", ui.spinBox_statusFontSize->value(), QFont::Bold);
					QGraphicsTextItem* textItem = camScene->addText(label, font);
					textItem->setZValue(1);
					textItem->setDefaultTextColor(Qt::white);

					QRectF textRect = textItem->boundingRect();
					QGraphicsRectItem* bgRect = camScene->addRect(textRect, QPen(Qt::NoPen), QBrush(Qt::blue));
					bgRect->setZValue(0);
					QPointF pos(r.x(), r.y() + textRect.height() + 5);
					bgRect->setPos(pos);
					textItem->setPos(pos);

					rectItems.append(bgRect);
					textItems.append(textItem);
				}
			}
			

			if (ui.checkBox_viewPersonBbox->isChecked())
			{
				for (auto& oR : oResult) {
					QElapsedTimer t;
					t.start();

					QRectF r(oR.x1, oR.y1, oR.x2 - oR.x1, oR.y2 - oR.y1);


					float leftKneeAngle = _trackManager->computeAngle(oR.keypoints[LEFT_HIP], oR.keypoints[LEFT_KNEE], oR.keypoints[LEFT_ANKLE]);
					float rightKneeAngle = _trackManager->computeAngle(oR.keypoints[RIGHT_HIP], oR.keypoints[RIGHT_KNEE], oR.keypoints[RIGHT_ANKLE]);

					bool isSitting = (leftKneeAngle < 140 || rightKneeAngle < 140);
					//oR.keypoints[Keypoint::LEFT_ANKLE] = _trackManager->CorrectAnklePoint(oR, true);
					//oR.keypoints[Keypoint::RIGHT_ANKLE] = _trackManager->CorrectAnklePoint(oR, false);


					if (isSitting)
					{
						oR.keypoints[Keypoint::LEFT_ANKLE] = _trackManager->CorrectAnklePoint(oR, true);
						oR.keypoints[Keypoint::RIGHT_ANKLE] = _trackManager->CorrectAnklePoint(oR, false);
					}

					//QString positionStatus = isSitting ? "Sitting("+QString::number(oR.accuracy) + ")" : "Standing("+ QString::number(oR.accuracy) + ")";
					QString positionStatus = isSitting ? "Sitting" : "Standing";

					// === Draw bounding box ===
					QPen rectPen(Qt::cyan);
					rectPen.setWidth(ui.spinBox_boundingBoxSize->value());
					QGraphicsRectItem* rectItem = camScene->addRect(r, rectPen);

					rectItems.append(rectItem);

					// Lookup object class name
					QString className = "unknown";
					auto it = classNames.find(oR.obj_id);
					if (it != classNames.end()) {
						className = QString::fromStdString(it->second);
					}


					if (false)
					{
						// Label text;
						QString label = QString("ID:%1, %2").arg(oR.tracking_id).arg(QString::number(oR.accuracy, 'f', 2));
						// Create text item
						QFont font("Arial", 25, QFont::Bold);
						QGraphicsTextItem* textItem = camScene->addText(label, font);
						textItem->setZValue(1);
						textItem->setDefaultTextColor(Qt::white);

						QRectF textRect = textItem->boundingRect();
						QGraphicsRectItem* bgRect = camScene->addRect(textRect, QPen(Qt::NoPen), QBrush(Qt::red));
						bgRect->setZValue(0);
						QPointF pos(r.x(), r.y() - textRect.height() - 5);
						bgRect->setPos(pos);
						textItem->setPos(pos);

						rectItems.append(bgRect);
						textItems.append(textItem);
					}



					if (ui.checkBox_viewPersonStatus->isChecked())
					{
						QString sParentkey = cameraId + "[@]" + QString::number(oR.tracking_id);
						SingleViewParentObject p = _cameraDisplayHash[cameraId].singleParentObjHash[sParentkey];

						//qDebug() << "[##] p cam Id: " << cameraId;
						//qDebug() << "[##] p global Id: " << sParentkey;

						auto yn = [](bool v) {
							return v
								? "<span style='color:#00ff00;'>Yes</span>"
								: "<span style='color:#ff4444;'>No</span>";
							};

						auto pf = [](bool v) {
							return v
								? "<span style='color:#00ff00; font-weight:bold;'>PASS</span>"
								: "<span style='color:#ff4444; font-weight:bold;'>FAIL</span>";
							};
						QString label;

						label += "<div style='color:white;'>";
						label += QString("<b>%1</b><br>").arg(p.globalId);

						if (_checkingCriteria.checkMask)
							label += QString("Mask: %1<br>").arg(yn(p.hasFacemask));

						if (_checkingCriteria.checkGlove)
							label += QString("Glove: %1<br>").arg(yn(p.hasGlove));

						if (_checkingCriteria.checkShoe)
							label += QString("ESD Shoe: %1<br>").arg(yn(p.hasEsdShoe));

						if (_checkingCriteria.checkSmock)
							label += QString("Smock: %1<br>").arg(yn(p.hasSmock));

						// STATUS is usually always shown
						QString personStatus;
						if (p.inCheckingArea)
						{
							personStatus = pf(p.isPass);
						}
						else
						{
							personStatus = "<span style='color:#ffa500; font-weight:bold;'>Not In Zone</span>";
						}
						label += QString("STATUS: %1").arg(personStatus);

						auto* textItem = new QGraphicsTextItem();
						textItem->setHtml(label);
						camScene->addItem(textItem);
						QFont f = textItem->font();
						f.setPointSize(ui.spinBox_statusFontSize->value());
						f.setBold(true);
						textItem->setFont(f);

						/*	QFont font("Arial", 6, QFont::Bold);
							QGraphicsTextItem* textItem = camScene->addText(label, font);*/
						textItem->setZValue(1);
						//textItem->setDefaultTextColor(Qt::white);

						QRectF textRect = textItem->boundingRect();
						QGraphicsRectItem* bgRect = camScene->addRect(textRect, QPen(Qt::NoPen), QBrush(QColor(0, 0, 0, 160)));
						bgRect->setZValue(0);
						QPointF pos(r.x() + r.width(), r.y());
						bgRect->setPos(pos);
						textItem->setPos(pos);

						rectItems.append(bgRect);
						textItems.append(textItem);
					}



					// === Draw keypoints ===

					if (ui.checkBox_viewPersonSkeleton->isChecked())
					{
						QPen kpPen(Qt::green);
						kpPen.setWidth(6);
						for (size_t i = 0; i < oR.keypoints.size(); ++i) {
							const cv::Point2f& kp = oR.keypoints[i];

							if (kp.x > 0 && kp.y > 0) {
								// Draw as small ellipse
								QGraphicsEllipseItem* kpEllipse = camScene->addEllipse(
									kp.x - 3, kp.y - 3, 6, 6, QPen(Qt::NoPen), QBrush(Qt::green));
								ellipseItems.append(kpEllipse);
							}
						}

						float cx = 0;
						float cy = 0;
						if (oR.keypoints.size() >= 17) {
							if (isSitting)
							{
								cx = (oR.keypoints[11].x + oR.keypoints[12].x) / 2.0f;
								cy = (oR.keypoints[15].y + oR.keypoints[16].y) / 2.0f;
							}
							else
							{
								cx = (oR.keypoints[15].x + oR.keypoints[16].x) / 2.0f;
								cy = (oR.keypoints[15].y + oR.keypoints[16].y) / 2.0f;
							}

						}
						QGraphicsEllipseItem* kpEllipse = camScene->addEllipse(
							cx - 5, cy - 5, 10, 10, QPen(Qt::NoPen), QBrush(Qt::green));
						ellipseItems.append(kpEllipse);

						// === OPTIONAL: Draw skeleton lines (COCO 17 format) ===
						static const std::vector<std::pair<int, int>> skeleton = {
							{5,7}, {7,9},   // left arm
							{6,8}, {8,10},  // right arm
							{5,6},          // shoulders
							{11,12},        // hips
							{5,11}, {6,12}, // torso
							{11,13}, {13,15}, // left leg
							{12,14}, {14,16}  // right leg
						};

						QPen linePen(Qt::yellow);
						linePen.setWidth(3);
						for (const auto& conn : skeleton) {
							int p1 = conn.first;
							int p2 = conn.second;
							if (p1 < oR.keypoints.size() && p2 < oR.keypoints.size()) {
								const cv::Point2f& k1 = oR.keypoints[p1];
								const cv::Point2f& k2 = oR.keypoints[p2];
								if (k1.x > 0 && k1.y > 0 && k2.x > 0 && k2.y > 0) {
									QGraphicsLineItem* lineItem = camScene->addLine(k1.x, k1.y, k2.x, k2.y, linePen);
									lineItems.append(lineItem);
								}
							}
						}
					}
				}
			}

		}
	}
	_prevTrackingResult = oResult;
}

void Moonica_qt::updateSingleViewResult(
	const QHash<QString, SingleViewParentObject>& singleViewParent,
	const QHash<QString, std::vector<OnnxResult>>& localOdResult)
{
	if (!_globalStreamingReadyFlag) return;

	for (auto& c : _cameraDisplayHash)
	{
		c.singleParentObjHash.clear();
	}


	for (auto it = singleViewParent.constBegin(); it != singleViewParent.constEnd(); ++it)
	{
		SingleViewParentObject p = it.value();
		_cameraDisplayHash[p.camId].singleParentObjHash.insert(p.globalId, p);
	}


}

void Moonica_qt::updateGlobalCoordinate(const QVector<ParentObject>& trackingObjects, int numberOfPeople)
{
	if (!_globalStreamingReadyFlag) return;

	if (!_floorPlanScene) {
		return;
	}

	ui.label_totalPeople->setText(QString::number(numberOfPeople));

	if (false)
	{
		processObjectImage(trackingObjects);
	}
	

	// Step 1: Mark everything as unused initially
	QSet<QString> activeIds;
	//QSet<QString> sittingIds;
	for (const auto& obj : trackingObjects) {
		activeIds.insert(obj.globalId);
	}

	//Step 2: Remove items that are no longer present
	for (auto it = _floorPlanRectMap.begin(); it != _floorPlanRectMap.end();) {
		QString id = it.key();
		if (!activeIds.contains(id)) {
			// Delete old rect
			_floorPlanScene->removeItem(it.value());
			delete it.value();
			it = _floorPlanRectMap.erase(it);

			// Delete ellipse
			if (_floorPlanEllipseMap.contains(id)) {
				_floorPlanScene->removeItem(_floorPlanEllipseMap[id]);
				delete _floorPlanEllipseMap[id];
				_floorPlanEllipseMap.remove(id);
			}

			// Delete text
			if (_floorPlanTextMap.contains(id)) {
				_floorPlanScene->removeItem(_floorPlanTextMap[id]);
				delete _floorPlanTextMap[id];
				_floorPlanTextMap.remove(id);
			}

			// Delete path
			if (_floorPlanTrailMap.contains(id)) {
				_floorPlanScene->removeItem(_floorPlanTrailMap[id]);
				delete _floorPlanTrailMap[id];
				_floorPlanTrailMap.remove(id);
			}

			// Delete arrow
			if (_floorPlanArrowLineMap.contains(id)) {
				_floorPlanScene->removeItem(_floorPlanArrowLineMap[id]);
				delete _floorPlanArrowLineMap[id];
				_floorPlanArrowLineMap.remove(id);
			}

			// Delete arrow head
			if (_floorPlanArrowHeadMap.contains(id)) {
				_floorPlanScene->removeItem(_floorPlanArrowHeadMap[id]);
				delete _floorPlanArrowHeadMap[id];
				_floorPlanArrowHeadMap.remove(id);
			}
		}
		else {
			++it;
		}
	}



	// Step 3: Update or create items
	for (const ParentObject& object : trackingObjects) {
		QRectF globalRect(object.global_x, object.global_y, object.global_w, object.global_h);

		//int globalId = object.globalId.toInt();
		_objectCamTrackingId[object.globalId] = object.parentIds;

		_colorPalleteIndex += 5;
		if (_colorPalleteIndex > _colorPallete.size())_colorPalleteIndex = 0;
		QColor objectColor = QColor(_colorPallete[_colorPalleteIndex]);

		// --- a) Rectangle ---
		QGraphicsRectItem* rectItem;
		if (_floorPlanRectMap.contains(object.globalId)) {
			rectItem = _floorPlanRectMap[object.globalId];
			rectItem->setRect(globalRect); // update existing
		}
		else {
			rectItem = new QGraphicsRectItem(globalRect);
			QColor rectColor = QColor(_colorPallete[_colorPalleteIndex]);
			_globalIdColor[object.globalId] = _colorPallete[_colorPalleteIndex];
			if (_globalIdColor.size() > 300) {
				auto it = _globalIdColor.begin(); // arbitrary element
				_globalIdColor.erase(it);
			}

			QPen rectPen(rectColor);
			rectPen.setWidth(30);
			rectItem->setPen(rectPen);
			rectItem->setBrush(QBrush(Qt::NoBrush));
			_floorPlanScene->addItem(rectItem);
			_floorPlanRectMap[object.globalId] = rectItem;
		}



		// --- b) Ellipse ---
		QPointF center = globalRect.center();
		QRectF ellipseRect(center.x() - 5, center.y() - 5, 10, 10);
		QGraphicsEllipseItem* ellipseItem;
		if (_floorPlanEllipseMap.contains(object.globalId)) {
			ellipseItem = _floorPlanEllipseMap[object.globalId];
			ellipseItem->setRect(ellipseRect);
		}
		else {
			ellipseItem = new QGraphicsEllipseItem(ellipseRect);
			ellipseItem->setBrush(QBrush(objectColor));
			ellipseItem->setPen(Qt::NoPen);
			_floorPlanScene->addItem(ellipseItem);
			_floorPlanEllipseMap[object.globalId] = ellipseItem;
		}

		// --- c) Text ---
		QString qText = object.globalId;

		if (ui.checkBox_renderDetail->isChecked())
		{
			qText = qText + "\n" + object.eldestChildId + " : " + QString::number(object.selectedChildren[object.eldestChildId], 'f', 3) + "\n";
			for (auto it = object.selectedChildren.begin(); it != object.selectedChildren.end(); ++it)
			{
				const QString& childId = it.key();
				double score = it.value();
				if (childId == object.eldestChildId) continue;
				qText += childId + " : " + QString::number(score, 'f', 3) + "\n";

			}
		}
		
		QColor qColor = objectColor;
		if (!object.isElderFound)
		{
			qColor = Qt::red;
			qText = qText + "\nLost Track";
		}

		QGraphicsTextItem* textItem;
		if (_floorPlanTextMap.contains(object.globalId)) {

			textItem = _floorPlanTextMap[object.globalId];
			textItem->setPos(globalRect.topLeft() + QPointF(5, -20));
			textItem->setPlainText(qText);
			//textItem->setDefaultTextColor(qColor);;
		}
		else {

			textItem = new QGraphicsTextItem(qText);
			textItem->setDefaultTextColor(objectColor);
			QFont font;
			font.setPointSize(75);
			font.setBold(false);
			textItem->setFont(font);
			textItem->setPos(globalRect.topLeft() + QPointF(5, -20));
			_floorPlanScene->addItem(textItem);
			_floorPlanTextMap[object.globalId] = textItem;
		}


		// --- d) Trail ---
		QVector<QPointF>& points = _objectTrailPoints[object.globalId];
		points.append(center);
		if (points.size() > MAX_TRAIL_POINTS) {
			points.removeFirst();
		}

		QPainterPath path(points.first());
		for (int i = 1; i < points.size(); ++i) {
			path.lineTo(points[i]);
		}

		if (_floorPlanTrailMap.contains(object.globalId)) {
			// Update existing trail
			QGraphicsPathItem* trailItem = _floorPlanTrailMap[object.globalId];
			trailItem->setPath(path);
		}
		else {
			// Create new trail
			QGraphicsPathItem* trailItem = new QGraphicsPathItem(path);

			QPen trailPen(objectColor);   // trail matches object color
			trailPen.setWidth(8);         // trail thickness
			trailItem->setPen(trailPen);

			_floorPlanScene->addItem(trailItem);
			_floorPlanTrailMap[object.globalId] = trailItem;
		}


	}
}

void Moonica_qt::forceSetNumberFloorObject()
{
	qDebug() << "forceSetNumberFloorObject";
	int number = ui.spinBox_numberOfPeople->value();
	_trackManager->forceSetTotalFLoorPlanObject(number, ui.checkBox_textCamDebug->isChecked());

	for (auto id : _globalIDImageProcessed)
	{
		removeImageObject(id);
	}
}

void Moonica_qt::processObjectImage(const QVector<ParentObject>& trackingObjects)
{

	for (auto& i : _globalIDImageProcessed)
	{
		bool found = false;

		for (auto& t : trackingObjects)
		{
			if (t.globalId == i)
			{
				found = true;
				break;
			}
		}

		if (!found)
		{
			removeImageObject(i);
		}
	}


	for (auto& parentObj : trackingObjects)
	{
		QString globalId = parentObj.globalId;

		if (_gloveLabels_left.contains(globalId))
		{
			QLabel* label = _gloveLabels_left[globalId];
			if (parentObj.hasGlove_left) {
				label->setText("Left Glove Pass");
				label->setStyleSheet("color: green; font-size: 18px; font-weight: bold;");
			}
			else {
				label->setText("Left Glove Fail");
				label->setStyleSheet("color: red; font-size: 18px; font-weight: bold;");
			}
		}

		if (_gloveLabels_right.contains(globalId))
		{
			QLabel* label = _gloveLabels_right[globalId];
			if (parentObj.hasGlove_right) {
				label->setText("Right Glove Pass");
				label->setStyleSheet("color: green; font-size: 18px; font-weight: bold;");
			}
			else {
				label->setText("Right Glove Fail");
				label->setStyleSheet("color: red; font-size: 18px; font-weight: bold;");
			}
		}

		if (_jumpsuitLabels.contains(globalId))
		{
			QLabel* label = _jumpsuitLabels[globalId];
			if (parentObj.hasJumpsuit) {
				label->setText("Jumpsuit Pass");
				label->setStyleSheet("color: green; font-size: 18px; font-weight: bold;");
			}
			else {
				label->setText("Jumpsuit Fail");
				label->setStyleSheet("color: red; font-size: 18px; font-weight: bold;");
			}
		}
	}
}

void Moonica_qt::recordCams()
{
	if (!_isCamTestRecording)
	{
		_isCamTestRecording = true;
		_recorderThread.startRecording(_frameManager->getActiveCameraIds());
		ui.toolButton_recordCamTest->setText("Stop Record");

		if (_movieRecording->state() == QMovie::NotRunning)
		{
			_movieRecording->start();
		}
		
	}
	else
	{
		_isCamTestRecording = false;
		_recorderThread.stopRecording();
		ui.toolButton_recordCamTest->setText("Start Record");

		if (_movieRecording->state() == QMovie::Running)
		{
			_movieRecording->stop();
		}
	}



}

void Moonica_qt::startStreamVideo()
{
	
	QString videosDir = ui.lineEdit_videoDir->text();

	if (videosDir.isEmpty())
	{
		qWarning() << "No directory selected.";
		return;
	}

	QDir dir(videosDir);
	if (!dir.exists())
	{
		qWarning() << "Directory does not exist:" << videosDir;
		return;
	}

	ui.toolButton_startVideoStream->setChecked(true);
	ui.toolButton_stopVideoStream->setChecked(false);
	// Set filters for only .avi files
	QStringList filters;
	filters << "*.mp4" << "*.avi";

	QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);

	QString videoPath;
	for (const QFileInfo& fileInfo : fileList)
	{
		QString fullPath = fileInfo.absoluteFilePath();
		QString videoName = fileInfo.completeBaseName(); // name without extension
		videoPath = fullPath;
		if (_frameGrabberHash.contains(videoName))
		{
			_frameGrabberHash[videoName]->setVideoMode(true);
			_frameGrabberHash[videoName]->setVideoPath(fullPath);
			camViewSelectOn(videoName, true);
		}
	}

	cv::VideoCapture cap(videoPath.toStdString());

	if (cap.isOpened())
	{
		double fps = cap.get(cv::CAP_PROP_FPS);
		double totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);

		double durationSec = 0.0;

		if (fps > 0)
			durationSec = totalFrames / fps;

		qDebug() << "Duration:" << durationSec << "seconds";

		
		ui.label_videoStreamRight->setText(QString::number(durationSec,'f', 2));
		_fullVideoStreamSecond = durationSec;

		ui.horizontalSlider_videoStream->setRange(0, static_cast<int>(_fullVideoStreamSecond * 1000)); // ms
		ui.horizontalSlider_videoStream->setSingleStep(100);   // 0.1 sec
		ui.horizontalSlider_videoStream->setPageStep(1000);    // 1 sec

		cap.release();
	}


}

void Moonica_qt::stopStreamVideo()
{
	ui.toolButton_startVideoStream->setChecked(false);
	ui.toolButton_stopVideoStream->setChecked(true);
	camViewSelectOff_all();
}

// == old way to render == 

void Moonica_qt::clearOverlayItems(CameraDisplay& cd)
{
	if (!cd.camScene) return;

	auto removeAll = [&](auto& vec)
		{
			for (auto* item : vec)
			{
				if (!item) continue;
				cd.camScene->removeItem(item);
				delete item;
			}
			vec.clear();
		};

	removeAll(cd.rectItems);
	removeAll(cd.textItems);
	removeAll(cd.lineItems);
	removeAll(cd.ellipseItems);
	removeAll(cd.polygonItems);
}

void Moonica_qt::addBBox(CameraDisplay& cd,
	const QRectF& r,
	const QString& label,
	const QPen& pen,
	const QBrush& brush,
	const QColor bgRectColor,
	const QColor txtColor)
{
	QElapsedTimer t;
	t.start();

	if (!cd.camScene) return;
	if (r.width() <= 1.0 || r.height() <= 1.0) return;

	auto* rectItem = cd.camScene->addRect(r, pen, brush);
	cd.rectItems.push_back(rectItem);

	if (!label.isEmpty())
	{
		auto* textItem = new QGraphicsTextItem();
		//textItem->setHtml(label);
		textItem->setPlainText(label);
		cd.camScene->addItem(textItem);

		QFont f = textItem->font();
		f.setPointSize(ui.spinBox_statusFontSize->value());
		f.setBold(true);
		textItem->setFont(f);
		textItem->setDefaultTextColor(txtColor);

		QRectF textRect = textItem->boundingRect();
		const qreal pad = 2.0;

		QRectF bgRect(0, 0,
			textRect.width() + pad * 2.0,
			textRect.height() + pad * 2.0);

		QPointF pos = r.topRight() + QPointF(2, 2);

		auto* bgItem = cd.camScene->addRect(bgRect, Qt::NoPen, QBrush(bgRectColor));
		bgItem->setPos(pos);
		bgItem->setZValue(19);

		textItem->setParentItem(bgItem);
		textItem->setPos(pad, pad);
		textItem->setZValue(20);

		cd.rectItems.push_back(bgItem);
	}

	qDebug() << "addBBox(" << label << ")took" << t.nsecsElapsed() / 1000.0 << "us";
}

QRectF Moonica_qt::toRectF(const OnnxResult& r)
{
	const int x1 = std::min(r.x1, r.x2);
	const int x2 = std::max(r.x1, r.x2);
	const int y1 = std::min(r.y1, r.y2);
	const int y2 = std::max(r.y1, r.y2);
	return QRectF(QPointF(x1, y1), QPointF(x2, y2));
}

void Moonica_qt::drawKeypoints(CameraDisplay& cd,
	const OnnxResult& person,
	const QPen& pen,
	float minKptConf,
	qreal radius)
{
	if (!cd.camScene) return;
	const int n = (int)person.keypoints.size();
	if (n <= 0) return;

	for (int i = 0; i < n; ++i)
	{
		const cv::Point2f& kp = person.keypoints[i];
		if (!std::isfinite(kp.x) || !std::isfinite(kp.y)) continue;
		if (kp.x <= 0 || kp.y <= 0) continue;

		if (!person.keypoint_confidences.empty() &&
			i < (int)person.keypoint_confidences.size() &&
			person.keypoint_confidences[i] < minKptConf)
		{
			continue;
		}

		QRectF e(kp.x - radius, kp.y - radius, radius * 2.0, radius * 2.0);
		auto* item = cd.camScene->addEllipse(e, pen, QBrush(pen.color()));
		cd.ellipseItems.push_back(item);
	}
}

void Moonica_qt::drawSkeletonCOCO17(CameraDisplay& cd,
	const OnnxResult& person,
	const QPen& pen,
	float minKptConf)
{
	if (!cd.camScene) return;
	const int n = (int)person.keypoints.size();
	if (n < 17) return; // need COCO 17

	auto ok = [&](int idx) -> bool {
		if (idx < 0 || idx >= n) return false;
		const auto& p = person.keypoints[idx];
		if (!std::isfinite(p.x) || !std::isfinite(p.y)) return false;
		if (p.x <= 0 || p.y <= 0) return false;

		if (!person.keypoint_confidences.empty() &&
			idx < (int)person.keypoint_confidences.size() &&
			person.keypoint_confidences[idx] < minKptConf)
			return false;

		return true;
		};

	// COCO skeleton edges (17-kpt)
	// 0 nose, 1 leye, 2 reye, 3 lear, 4 rear, 5 lsho, 6 rsho,
	// 7 lelb, 8 relb, 9 lwri, 10 rwri, 11 lhip, 12 rhip,
	// 13 lknee, 14 rknee, 15 lankle, 16 rankle
	static const std::pair<int, int> bones[] = {
		{0,1},{0,2},{1,3},{2,4},        // head
		{5,6},                          // shoulders
		{5,7},{7,9},                    // left arm
		{6,8},{8,10},                   // right arm
		{5,11},{6,12},                  // torso sides
		{11,12},                        // hips
		{11,13},{13,15},                // left leg
		{12,14},{14,16}                 // right leg
	};

	for (const auto& b : bones)
	{
		const int a = b.first;
		const int c = b.second;
		if (!ok(a) || !ok(c)) continue;

		const auto& p1 = person.keypoints[a];
		const auto& p2 = person.keypoints[c];

		auto* line = cd.camScene->addLine(p1.x, p1.y, p2.x, p2.y, pen);
		cd.lineItems.push_back(line);
	}
}

//void Moonica_qt::updateSingleViewResult(
//	const QHash<QString, SingleViewParentObject>& singleViewParent,
//	const QHash<QString, std::vector<OnnxResult>>& localOdResult)
//{
//	
//	// 1) Build camId -> list of persons
//	QHash<QString, QVector<const SingleViewParentObject*>> personsByCam;
//	personsByCam.reserve(singleViewParent.size());
//
//	for (auto it = singleViewParent.constBegin(); it != singleViewParent.constEnd(); ++it)
//	{
//		
//		const SingleViewParentObject& p = it.value();
//		if (!p.isTracking) continue;
//		personsByCam[p.camId].push_back(&p);
//	}
//
//	// 2) For each camera display, clear + draw
//	for (auto camIt = _cameraDisplayHash.begin(); camIt != _cameraDisplayHash.end(); ++camIt)
//	{
//		const QString camId = camIt.key();
//		CameraDisplay& cd = camIt.value();
//
//		QPixmap pixmap = QPixmap::fromImage(cd.frame);
//		QMainGraphicsView* camView = cd.camView;
//		QGraphicsPixmapItem* camPixmapItem = cd.camPixmapItem;
//		QMainGraphicsScene* camScene = cd.camScene;
//		// 1. Update background image
//		if (!camPixmapItem) {
//			camPixmapItem = camScene->addPixmap(pixmap);
//			cd.camPixmapItem = camPixmapItem; // store back!
//			camView->fitInView(camPixmapItem, Qt::KeepAspectRatio);
//		}
//		else {
//			camPixmapItem->setPixmap(pixmap);
//		}
//		if (!cd.camScene) continue;
//
//		
//
//		// Clear previous overlays
//		clearOverlayItems(cd);
//
//		// ---- Pen styles (you can adjust)
//		QPen personPen(QColor(0, 255, 0)); // cyan for person
//		personPen.setWidth(ui.spinBox_boundingBoxSize->value());
//
//		QPen objPen(QColor(0, 255, 255));      // green for PPE objects
//		objPen.setWidth(ui.spinBox_boundingBoxSize->value());
//
//		QPen warnPen(QColor(255, 0, 0));     // red for missing / fail
//		warnPen.setWidth(ui.spinBox_boundingBoxSize->value());
//
//	
//
//		// 2B) Draw object detections for this cam
//		auto odIt = localOdResult.find(camId);
//
//		if (ui.checkBox_viewObjectBbox->isChecked())
//		{
//			if (odIt != localOdResult.end())
//			{
//				const std::vector<OnnxResult>& objs = odIt.value();
//				for (const auto& o : objs)
//				{
//					const QRectF r = toRectF(o);
//
//					// label includes class + confidence
//					QString label = QString("%1 (%2)")
//						.arg(QString::fromStdString(classNames.at(o.obj_id)))
//						.arg(o.accuracy, 0, 'f', 2);
//
//					addBBox(cd, r, label, objPen, Qt::NoBrush, Qt::blue, Qt::white);
//				}
//			}
//		}
//		
//
//		// 2A) Draw persons for this cam
//		if (personsByCam.contains(camId))
//		{
//			const auto& plist = personsByCam[camId];
//			for (const SingleViewParentObject* pPtr : plist)
//			{
//				if (!pPtr) continue;
//				const SingleViewParentObject& p = *pPtr;
//
//				const QRectF pr = toRectF(p.trackingResult);
//
//				auto yn = [](bool v) {
//					return v
//						? "<span style='color:#00ff00;'>Yes</span>"
//						: "<span style='color:#ff4444;'>No</span>";
//					};
//
//				auto pf = [](bool v) {
//					return v
//						? "<span style='color:#00ff00; font-weight:bold;'>PASS</span>"
//						: "<span style='color:#ff4444; font-weight:bold;'>FAIL</span>";
//					};
//
//				// Build label showing PPE status
//				QString label;
//
//				if (ui.checkBox_viewPersonStatus->isChecked())
//				{
//					label += "<div style='color:white;'>";
//					label += QString("<b>%1</b><br>").arg(p.globalId);
//
//					if (_checkingCriteria.checkMask)
//						label += QString("Mask: %1<br>").arg(yn(p.hasFacemask));
//
//					if (_checkingCriteria.checkGlove)
//						label += QString("Glove: %1<br>").arg(yn(p.hasGlove));
//
//					if (_checkingCriteria.checkShoe)
//						label += QString("ESD Shoe: %1<br>").arg(yn(p.hasEsdShoe));
//
//					if (_checkingCriteria.checkSmock)
//						label += QString("Smock: %1<br>").arg(yn(p.hasSmock));
//
//					// STATUS is usually always shown
//					QString personStatus;
//					if (p.inCheckingArea)
//					{
//						personStatus = pf(p.isPass);
//					}
//					else
//					{
//						personStatus = "<span style='color:#ffa500; font-weight:bold;'>Not In Zone</span>";
//					}
//					label += QString("STATUS: %1").arg(personStatus);
//				}
//				
//				
//
//				// If you want pass/fail highlight:
//				const bool pass = p.isPass;
//
//				if (ui.checkBox_viewPersonBbox->isChecked())
//				{
//					label = "human";
//					addBBox(cd, pr, label, pass ? personPen : warnPen);
//				}
//			
//
//
//				QPen kptPen(QColor(255, 255, 0));   // yellow points/lines
//				kptPen.setWidth(2);
//
//				const float minKptConf = 0.30f;
//
//				if (ui.checkBox_viewPersonSkeleton->isChecked())
//				{
//					drawSkeletonCOCO17(cd, p.trackingResult, kptPen, minKptConf);
//					drawKeypoints(cd, p.trackingResult, kptPen, minKptConf, 3.0);
//				}
//				
//			}
//		}
//
//		// If you want, also update the view
//		// (usually not required if scene updates automatically)
//		if (cd.camView)
//			cd.camView->viewport()->update();
//
//
//		if (ui.checkBox_viewCheckingArea->isChecked())
//		{
//			QPen pen1(Qt::red);
//			pen1.setWidth(2);
//			QPolygonF poly;
//			for (auto& p : _curProject._viewInfoHash[camId].doorPointVec)
//			{
//				poly << p;
//			}
//			QBrush brush(QColor(255, 165, 0, 80)); // orange with 50% transparency
//
//			QGraphicsPolygonItem* polyItem = cd.camScene->addPolygon(poly, pen1, brush);
//
//			cd.polygonItems.append(polyItem);
//		}
//	}
//}
