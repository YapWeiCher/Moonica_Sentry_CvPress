#include "RecorderThread.h"
#include <QDebug>

RecorderThread::RecorderThread(QObject* parent)
    : QThread(parent)
{
}

RecorderThread::~RecorderThread()
{
    stopRecording();
    wait();
}

bool RecorderThread::openNewSegment(const QString& camId, bool force)
{
    if (!_camStateHash.contains(camId)) return false;

    auto& st = _camStateHash[camId];

    // Close old file
    if (st.writer.isOpened()) {
        st.writer.release();
    }

    // New filename: camId_YYYYMMDD_HHMMSS_partNN.mp4
    const QDateTime now = QDateTime::currentDateTime();
    st.segmentStart = now;

    QString ts = now.toString("yyyyMMdd_HHmmss");
    QString part = QString("part%1").arg(st.segmentIndex, 2, 10, QChar('0'));

    QString newTimeDir = _recordedDir + "/" + ts + "/";
    PathManager::makeDir(newTimeDir);
    st.currentPath = newTimeDir + camId + ".mp4";

    // TODO: If frame size differs per camera, store per-cam size/fps
    bool ok = st.writer.open(
        st.currentPath.toStdString(),
        cv::VideoWriter::fourcc('m', 'p', '4', 'v'),
        7,                          // fps
        cv::Size(1280, 720),        // size
        true
    );

    if (!ok) {
        qCritical() << "Recorder: Failed to open segment:" << st.currentPath;
        return false;
    }

    st.segmentIndex++;
    return true;
}

bool RecorderThread::startRecording(const QStringList& camIDs)
{
    if (_recording) return false;

    _maxQueueSize = 50;

    QUuid uuid = QUuid::createUuid();
    _recordedDir = PathManager::_recordedVideoDir + "/" + uuid.toString() + "/";
    PathManager::makeDir(_recordedDir);

    _camStateHash.clear();

    for (const auto& camId : camIDs)
    {
        CamSegmentState st;
        st.segmentIndex = 0;
        _camStateHash.insert(camId, st);

        if (!openNewSegment(camId, /*force*/true)) {
            qCritical() << "Recorder: Failed to open first segment for" << camId;
            return false;
        }
    }

    _recording = true;

    if (!isRunning()) {
        _running = true;
        start();
    }
    return true;
}

void RecorderThread::stopRecording()
{
    if (!_recording) return;

    _recording = false;
    _running = false;
    _cond.wakeAll();

    for (auto it = _camStateHash.begin(); it != _camStateHash.end(); ++it) {
        if (it.value().writer.isOpened())
            it.value().writer.release();
    }
}

void RecorderThread::enqueueFrame(const cv::Mat& frame, QString camId)
{
    if (!_recording || frame.empty())
        return;

    QMutexLocker locker(&_mutex);

    // drop oldest if queue is full (real-time behavior)
    if ((int)_queue.size() >= _maxQueueSize) {
        _queue.pop();
    }
    FrameItem fItem;
    fItem.camID = camId;
    fItem.frame = frame;

    _queue.push(fItem);
    _cond.wakeOne();
}

void RecorderThread::run()
{
    qDebug() << "Recorder thread started";

    while (_running || !_queue.empty())
    {
        FrameItem fItem;
        {
            QMutexLocker locker(&_mutex);
            if (_queue.empty()) {
                _cond.wait(&_mutex, 5);
                continue;
            }
            fItem = _queue.front();
            _queue.pop();
        }

        if (!_camStateHash.contains(fItem.camID))
            continue;

        auto& st = _camStateHash[fItem.camID];

        // Rotate if > 1 hour
        if (st.segmentStart.isValid()) {
            qint64 elapsed = st.segmentStart.secsTo(QDateTime::currentDateTime());
            if (elapsed >= _segmentSeconds) {
                openNewSegment(fItem.camID, /*force*/true);
            }
        }
        else {
            // safety: if segmentStart not set, open first segment
            openNewSegment(fItem.camID, /*force*/true);
        }

        if (st.writer.isOpened()) {
            st.writer.write(fItem.frame);
        }
    }

    clearQueue();

    // release all writers
    for (auto it = _camStateHash.begin(); it != _camStateHash.end(); ++it) {
        if (it.value().writer.isOpened())
            it.value().writer.release();
    }

    qDebug() << "Recorder thread finished";
}

void RecorderThread::clearQueue()
{
    QMutexLocker locker(&_mutex);
    while (!_queue.empty())
        _queue.pop();
}