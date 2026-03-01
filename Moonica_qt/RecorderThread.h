#pragma once

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <queue>
#include <atomic>
#include <QUuid>
#include <QDateTime>
#include<PathManager.h>
#include <opencv2/opencv.hpp>

class RecorderThread : public QThread
{
    Q_OBJECT

public:
     RecorderThread(QObject* parent = nullptr);
    ~RecorderThread() ;

    // lifecycle
 /*   bool startRecording(const QString& filePath,
        double fps,
        const cv::Size& frameSize,
        int maxQueueSize = 50);*/

    bool startRecording(const QStringList& camIDs);

    void stopRecording();
    bool openNewSegment(const QString& camId, bool force);
    // called from grabber thread (QueuedConnection)
public slots:
    void enqueueFrame(const cv::Mat& frame, QString camId);

protected:
    void run() override;

private:
    void clearQueue();

private:
    struct FrameItem
    {
        cv::Mat frame;
        QString camID;   // camera ID
    };

    struct CamSegmentState
    {
        cv::VideoWriter writer;
        QDateTime segmentStart;   // when this file started
        int segmentIndex = 0;     // optional (0,1,2...)
        QString currentPath;
    };

    std::queue<FrameItem> _queue;
    QMutex _mutex;
    QWaitCondition _cond;

   // QHash<QString, cv::VideoWriter> _writerHash;


    std::atomic<bool> _running{ false };
    std::atomic<bool> _recording{ false };

    int _maxQueueSize{ 50 };


    QHash<QString, CamSegmentState> _camStateHash;
    qint64 _segmentSeconds = 3600;   // 1 hour
    QString _recordedDir;            // base dir for this recording session

};