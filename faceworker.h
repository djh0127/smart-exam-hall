#ifndef FACEWORKER_H
#define FACEWORKER_H

#include <QObject>
#include <QImage>
#include <QString>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/face.hpp>

class QTimer;

/*
 * 摄像头采集 / 人脸检测 / LBPH 训练与保存 全部放在独立线程中执行，
 * UI 线程只负责接收已经处理好的图像与结果信号，避免界面卡顿。
 */
class FaceWorker : public QObject
{
    Q_OBJECT

public:
    explicit FaceWorker(QObject *parent = nullptr);
    ~FaceWorker();

    void setCascadeFile(const QString &file)  { m_cascadeFile = file; }
    void setModelFile(const QString &file)    { m_modelFile = file; }
    void setThreshold(double threshold)       { m_threshold = threshold; }
    void setDetectScale(double scale)         { m_detectScale = scale; }

signals:
    // img 为深拷贝，可安全跨线程使用；hasFace 表示当前帧是否检测到人脸
    void frameReady(const QImage &img, bool hasFace);
    void enrollFinished(bool ok, const QString &msg, int label, const QImage &faceImg);
    void predictFinished(int label, double confidence);

public slots:
    void start(int cameraIndex);
    void stop();
    void enroll(int label);   // 采集当前人脸并训练/保存模型
    void predict();           // 识别当前人脸

private slots:
    void processFrame();

private:
    void loadRecognizer();
    bool currentFace(cv::Mat &face);
    static QImage toQImage(const cv::Mat &mat);

private:
    cv::VideoCapture m_cap;
    QTimer *m_timer;
    cv::CascadeClassifier m_classifier;
    cv::Ptr<cv::face::FaceRecognizer> m_recognizer;

    cv::Mat m_frame;                     // 最近一帧（含框）
    std::vector<cv::Rect> m_faceRects;   // 最近一次检测结果

    std::vector<cv::Mat> m_trainFaces;
    std::vector<int>     m_trainLabels;

    double  m_detectScale;   // 检测前缩小比例，<=400px 宽时不缩放
    QString m_cascadeFile;
    QString m_modelFile;
    double  m_threshold;
    bool    m_hasFace;
};

#endif // FACEWORKER_H
