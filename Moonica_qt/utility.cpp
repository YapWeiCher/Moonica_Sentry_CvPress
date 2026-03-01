
#include "Moonica_qt.h"


cv::Mat Moonica_qt::QImage_to_cvMat(const QImage& image)
{
	cv::Mat mat;
	switch (image.format())
	{
	case QImage::Format_ARGB32: case QImage::Format_RGB32: case QImage::Format_ARGB32_Premultiplied:
		mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
		cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR); break;
	case QImage::Format_RGB888:
		mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
		cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR); break;
	default:
		QImage temp = image.convertToFormat(QImage::Format_RGB888);
		mat = cv::Mat(temp.height(), temp.width(), CV_8UC3, (void*)temp.constBits(), temp.bytesPerLine());
		cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR); break;
	}
	return mat.clone();
}

QImage Moonica_qt::cvMat_to_QImage(const cv::Mat& mat)
{
	if (mat.type() == CV_8UC3)
	{
		cv::Mat rgb; cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
		return QImage((const uchar*)rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
	}
	return QImage();
}



bool Moonica_qt::birdEyeCalibration(cv::Mat& homography)
{
	int camViewPointNum = 0;
	int floorViewPointNum = 0;

	if (!_dragPolygonROI_camView.isEmpty())camViewPointNum = _dragPolygonROI_camView[0]->getPolygon().count();
	if (!_dragPolygonROI_floorView.isEmpty())floorViewPointNum = _dragPolygonROI_floorView[0]->getPolygon().count();

	qDebug() << "_dragPolygonROI_camView.size(): " << camViewPointNum;
	qDebug() << "_dragPolygonROI_floorView.size(): " << floorViewPointNum;

	if ((_dragPolygonROI_camView.size() > 0 && _dragPolygonROI_floorView.size() > 0)
		&& camViewPointNum == floorViewPointNum)
	{
		qDebug() << "testBirdEyes";
		std::vector<cv::Point2f> imagePoints;
		if (_dragPolygonROI_camView.size() > 0)
		{
			QPolygon poly = _dragPolygonROI_camView[0]->getPolygon();


			imagePoints.reserve(poly.size());

			for (int i = 0; i < poly.size(); ++i)
			{
				QPoint pt = poly.at(i);
				imagePoints.emplace_back(static_cast<float>(pt.x()),
					static_cast<float>(pt.y()));
			}
		}


		std::vector<cv::Point2f> floorPoints;
		if (_dragPolygonROI_floorView.size() > 0)
		{
			QPolygon poly = _dragPolygonROI_floorView[0]->getPolygon();


			floorPoints.reserve(poly.size());

			for (int i = 0; i < poly.size(); ++i)
			{
				QPoint pt = poly.at(i);
				floorPoints.emplace_back(static_cast<float>(pt.x()),
					static_cast<float>(pt.y()));
			}
		}


		cv::Mat H = cv::findHomography(imagePoints, floorPoints);
		homography = H;
		return true;
	}
	else
	{
		QMessageBox::warning(this, tr("Calibration failed!"),
			tr("Points of Image not equal to floor points"));
		return false;
	}
	
}

QPointF Moonica_qt::tranformPointToFloorView(QPointF pt, cv::Mat H)
{
	// Convert QPointF to cv::Point2f
	std::vector<cv::Point2f> imagePoints;
	imagePoints.push_back(cv::Point2f(pt.x(), pt.y()));

	// Apply perspective transform using homography
	std::vector<cv::Point2f> floorPoints;
	cv::perspectiveTransform(imagePoints, floorPoints, H);

	// Convert back to QPointF
	return QPointF(floorPoints[0].x, floorPoints[0].y);
}





