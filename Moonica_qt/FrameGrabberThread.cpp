#include "FrameGrabberThread.h"


FrameGrabberThread::FrameGrabberThread(QString camId, QString url)
{
    _camId = camId;
    _url = url;
   
    if (_camType == CAMERA_TYPE::INDUSTRIAL_CAM)
    {
        iCam_iniIndustrialCamera();
    }
}

FrameGrabberThread::~FrameGrabberThread()
{

}



QString FrameGrabberThread::getCamId()
{
    return _camId;
}

void FrameGrabberThread::setVideoPath(QString videoPath)
{
    _videoPath = videoPath;

}

void FrameGrabberThread::setVideoMode(bool videoMode)
{
    _isVideoMode = videoMode;
}

void FrameGrabberThread::setSetting(QString url)
{
	_url = url;
}

void FrameGrabberThread::setCurrentSecond(double cursecond)
{
    _currentSecond = cursecond;
}

void FrameGrabberThread::run()
{
    _is_running_frame = true;

    if (_isVideoMode)
    {
        startFrameGrabbing();
    }
    else if (_camType == CAMERA_TYPE::INDUSTRIAL_CAM)
    {
        //startFrameGrabbing_industrialCam();
        iCam_startGrab();      
    }
}


void FrameGrabberThread::startFrameGrabbing()
{
    qDebug() << "URL: " << _url;
    if (!_isVideoMode)
    {
        if (!_cap.open(_url.toStdString(), cv::CAP_FFMPEG))
        {
            qCritical("VideoWorker Error: Could not open the CCTV stream.");
            return;
        }
    }
    else
    {
        _cap.open(_videoPath.toStdString());
    }
  

    cv::Mat frame;
    //const int delay = 60; // ori 120
    const int delay = 120; // ori 120

    if (_isVideoMode)
    {
        _cap.set(cv::CAP_PROP_POS_MSEC, _currentSecond * 1000.0);
    }
    while (_is_running_frame)
    {
        if (_cap.read(frame) && !frame.empty()) {
           
            double currentMs = _cap.get(cv::CAP_PROP_POS_MSEC);
            double currentSec = currentMs / 1000.0;

            emit frameReady(frame, _camId, currentSec);
        }
        else
        {
            // disconnected
            if (_isVideoMode)
            {
          

                QThread::msleep(1000);  // show it for 1 sec
                break;
            }
            else
            {
                qWarning("CCTV: Connection lost, attempting reconnect...");
                _cap.release();
                _cap.open(_url.toStdString(), cv::CAP_FFMPEG);
            }

        }

        if(_isVideoMode)QThread::msleep(delay);
    }
    _cap.release();
}

void FrameGrabberThread::stopFrameGrabbing()
{
    _is_running_frame = false;
}





// -- cam

void FrameGrabberThread::iCam_iniIndustrialCamera()
{
    QString path = "C:/Moonica/"+ _camId+".json";
    qDebug() << "Ini Camera: "<< path;
    QJsonObject obj;

    if (!jsonHelper::loadJson(path, obj)) {
        ct::logger::error("[CAM] Failed to load camera.json");
        return;
    }

    qDebug() << 1;
    if (obj.contains("Camera")) {
        auto cams = obj["Camera"].toArray();
        qDebug() << 2;
        for (auto doc : cams) {
            qDebug() << 3;
            auto obj = doc.toObject();

            auto id = jsonHelper::getString(obj, "ID");
            auto api = jsonHelper::getString(obj, "API");
            auto sn = jsonHelper::getString(obj, "Serial_Number");
            auto configPath = jsonHelper::getString(obj, "Config_File");
            auto skey = jsonHelper::getInteger(obj, "Security_Key");

     

            if (!iCam_verifyKey(sn, skey))
            {
                qDebug() << "[CAM] verify key failed";
                continue;
            }

            if (!iCam_create(api))
            {
                qDebug() << "[CAM] create failed";
                continue;
            }

            if (!iCam_connect(sn))
            {
                qDebug() << "[CAM] connect failed";
                continue;
            }

            QString camConfigPath = "C:/Moonica/" + configPath;
            if (!iCam_loadCameraSettingConfig(camConfigPath))
            {

                qDebug() << "[CAM] loadConfig failed: C:/Moonica/" << camConfigPath;
                continue;
            }

           
        }
    }
}

bool FrameGrabberThread::iCam_loadCameraSettingConfig(QString path)
{
    if (!iCam_valid()) return false;
    auto ret = _iCam->loadConfig(path);
    if (!ret) ct::logger::error("Fail to load camera config: %s", path.toStdString().c_str());
    ct::logger::info("[CAM] Loaded config: %s, %s", _camId.toStdString().c_str(), path.toStdString().c_str());
    return ret;
}

bool FrameGrabberThread::iCam_verifyKey(QString sn, int key)
{
    int securityKey = 1;
    int notDigit = 0;

    for (auto x : sn)
    {
        int value = x.digitValue();

        if (value == 0) continue;

        if (value == -1)
        {
            notDigit++;
        }
        else
        {
            securityKey *= value;
        }
    }

    securityKey -= notDigit;

    if (key != securityKey)
    {
        ct::logger::error("[CAM] Invalid camera key. Expected key: %d, Receive: %d", securityKey, key);
        qDebug() << QString("[CAM] Invalid camera key. Expected key: %1, Receive: %2").arg(securityKey).arg(key);
        return false;
    }

    return true;
}
void FrameGrabberThread::iCam_frameReady(FrameInfo fInfo)
{
   

    emit frameReady(fInfo.frame, _camId, _currentSecond);
}

bool FrameGrabberThread::iCam_create(QString api)
{
    if (api == "IRayple") {
        _iCam = new CAM_IRayple();
        
        qRegisterMetaType<FrameInfo>("FrameInfo");
        connect(_iCam, &CAM_IRayple::iCam_frameReady,
            this, &FrameGrabberThread::iCam_frameReady,
            Qt::QueuedConnection);
    }

    else {
        ct::logger::error("[CAM] Failed to create camera: %s", api.toStdString().c_str());
        return false;
    }


    ct::logger::info("[CAM] Created camera: %s", api.toStdString().c_str());
    return true;
}

bool FrameGrabberThread::iCam_connect(QString sn)
{
    if (!iCam_valid()) return false;
    auto ret = _iCam->connect(sn);
    if (!ret) ct::logger::error("Failed to connect camera: %s", _camId.toStdString().c_str());
    ct::logger::info("[CAM] Connected to: %s", _camId.toStdString().c_str());
    return ret;
}

bool FrameGrabberThread::iCam_disconnect()
{
    if (!iCam_valid()) return false;
    auto ret = _iCam->disconnect();
    if (!ret) ct::logger::error("Failed to disconnect camera: %s", _camId.toStdString().c_str());
    ct::logger::info("[CAM] Discconected from: %s", _camId.toStdString().c_str());
    return ret;
}

bool FrameGrabberThread::iCam_startGrab()
{
    if (!iCam_valid()) return false;

    qDebug() << "Serial Number: " << _iCam->getSerialNumber();
    auto ret = _iCam->startGrab();
    if (!ret) ct::logger::error("Failed to start grabbing: %s", _camId.toStdString().c_str());
    return ret;
}

bool FrameGrabberThread::iCam_stopGrab()
{
    if (!iCam_valid()) return false;
    auto ret = _iCam->stopGrab();
    if (!ret) ct::logger::error("Failed to stop grabbing: %s", _camId.toStdString().c_str());
    return ret;
}

bool FrameGrabberThread::iCam_valid()
{
    if (_iCam == nullptr) return false;
    else return true;

}

