#include <opencv2/opencv.hpp>
#include <iostream>

//enum class LightState
//{
//    Unknown,
//    Green,
//    Yellow
//};
//
//LightState detectTowerLightState(const cv::Mat& frame, const cv::Rect& roi)
//{
//    if (frame.empty() || roi.x < 0 || roi.y < 0 ||
//        roi.x + roi.width > frame.cols ||
//        roi.y + roi.height > frame.rows)
//    {
//        return LightState::Unknown;
//    }
//
//    cv::Mat lightRoi = frame(roi).clone();
//
//    cv::GaussianBlur(lightRoi, lightRoi, cv::Size(5, 5), 0);
//
//    cv::Mat hsv;
//    cv::cvtColor(lightRoi, hsv, cv::COLOR_BGR2HSV);
//
//    cv::Mat maskGreen, maskYellow;
//
//    cv::Scalar lowerGreen(35, 80, 80);
//    cv::Scalar upperGreen(85, 255, 255);
//
//    cv::Scalar lowerYellow(20, 80, 80);
//    cv::Scalar upperYellow(35, 255, 255);
//
//    cv::inRange(hsv, lowerGreen, upperGreen, maskGreen);
//    cv::inRange(hsv, lowerYellow, upperYellow, maskYellow);
//
//    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
//    cv::morphologyEx(maskGreen, maskGreen, cv::MORPH_OPEN, kernel);
//    cv::morphologyEx(maskGreen, maskGreen, cv::MORPH_CLOSE, kernel);
//
//    cv::morphologyEx(maskYellow, maskYellow, cv::MORPH_OPEN, kernel);
//    cv::morphologyEx(maskYellow, maskYellow, cv::MORPH_CLOSE, kernel);
//
//    int greenCount = cv::countNonZero(maskGreen);
//    int yellowCount = cv::countNonZero(maskYellow);
//
//    double area = static_cast<double>(lightRoi.rows * lightRoi.cols);
//    double greenRatio = greenCount / area;
//    double yellowRatio = yellowCount / area;
//
//    const double minRatio = 0.05; // adjust based on your light size in ROI
//
//    if (greenRatio < minRatio && yellowRatio < minRatio)
//        return LightState::Unknown;
//
//    if (greenRatio > yellowRatio)
//        return LightState::Green;
//    else
//        return LightState::Yellow;
//}
//
//int main()
//{
//    cv::VideoCapture cap(0);
//    if (!cap.isOpened())
//    {
//        std::cout << "Failed to open camera\n";
//        return -1;
//    }
//
//    cv::Rect roi(100, 100, 80, 150); // example ROI
//    LightState prevState = LightState::Unknown;
//
//    while (true)
//    {
//        cv::Mat frame;
//        cap >> frame;
//        if (frame.empty())
//            break;
//
//        LightState currState = detectTowerLightState(frame, roi);
//
//        if (prevState == LightState::Green && currState == LightState::Yellow)
//            std::cout << "GREEN -> YELLOW\n";
//        else if (prevState == LightState::Yellow && currState == LightState::Green)
//            std::cout << "YELLOW -> GREEN\n";
//
//        prevState = currState;
//
//        cv::rectangle(frame, roi, cv::Scalar(255, 0, 0), 2);
//
//        std::string text = "Unknown";
//        if (currState == LightState::Green) text = "Green";
//        else if (currState == LightState::Yellow) text = "Yellow";
//
//        cv::putText(frame, text, cv::Point(roi.x, roi.y - 10),
//            cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);
//
//        cv::imshow("Tower Light Detection", frame);
//
//        int key = cv::waitKey(30);
//        if (key == 27)
//            break;
//    }
//
//    return 0;
//}