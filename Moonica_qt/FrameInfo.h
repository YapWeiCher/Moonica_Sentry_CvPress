#pragma once
//#include "mil.h"
//#include "PostAcquisitionTask.h"
#include <opencv2/opencv.hpp>
#include <QString>

enum class ICAM_pixelFormat {
	RGB8, Mono8, BayerGB8, BayerRG8
};

struct FrameInfo {
	FrameInfo();
	~FrameInfo();
	FrameInfo(const FrameInfo& other);
	FrameInfo& operator=(const FrameInfo& other);

	//buffer
	//MIL_ID pImage = M_NULL;
	//MIL_ID pHeightMap = M_NULL;
	//std::vector<double> profiles;
	cv::Mat frame;


	//generic info
	int	bufferSize = 0;
	int	width = 0;
	int	height = 0;
	int channel = 1;
	ICAM_pixelFormat pixelFormat;
	uint64_t timeStamp;
	QString type = "";

	//linkage
	QString cameraID = "";
	QString viewID = "";

	QString baseOpticID = ""; //for cases where optic was split
	QString opticID = "";

	QString stitchID = "";
	int index = -1;
	int row = 0;
	int col = 0;

	//PostAcquisitionTask postTask;
};