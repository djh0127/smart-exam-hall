#include "faceworker.h"

#include <QTimer>
#include <QFile>
#include <QDebug>

FaceWorker::FaceWorker(QObject *parent)
    : QObject(parent)
    , m_timer(nullptr)
    , m_detectScale(0.5)
    , m_threshold(150.0)
    , m_hasFace(false)
{
}

FaceWorker::~FaceWorker()
{
    stop();
    m_cap.release();
}

void FaceWorker::start(int cameraIndex)
{
    if (!m_cascadeFile.isEmpty() && !m_classifier.load(m_cascadeFile.toStdString()))
        qWarning() << "级联分类器加载失败:" << m_cascadeFile;

    loadRecognizer();

    if (!m_cap.isOpened()) {
        m_cap.open(cameraIndex);
        if (m_cap.isOpened()) {
            // 限制分辨率：检测耗时与像素数成正比，这是最直接的提速手段
            m_cap.set(cv::CAP_PROP_FRAME_WIDTH,  640);
            m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            m_cap.set(cv::CAP_PROP_FPS, 30);
        } else {
            qWarning() << "摄像头打开失败, index =" << cameraIndex;
        }
    }

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &FaceWorker::processFrame);
    }
    m_timer->start(30);   // 约 33FPS
}

void FaceWorker::stop()
{
    if (m_timer)
        m_timer->stop();
}

void FaceWorker::loadRecognizer()
{
    if (!m_modelFile.isEmpty() && QFile::exists(m_modelFile))
        m_recognizer = cv::face::FaceRecognizer::load<cv::face::LBPHFaceRecognizer>(m_modelFile.toStdString());
    else
        m_recognizer = cv::face::LBPHFaceRecognizer::create();

    if (!m_recognizer.empty())
        m_recognizer->setThreshold(m_threshold);
}

void FaceWorker::processFrame()
{
    if (!m_cap.isOpened())
        return;
    if (!m_cap.read(m_frame) || m_frame.empty())
        return;

    cv::flip(m_frame, m_frame, 1);

    cv::Mat gray;
    cv::cvtColor(m_frame, gray, CV_BGR2GRAY);

    // 缩小后再检测，速度提升约 1/(scale^2) 倍
    double usedScale = 1.0;
    if (m_frame.cols > 400 && m_detectScale > 0 && m_detectScale < 1.0) {
        usedScale = m_detectScale;
        cv::resize(gray, gray, cv::Size(), usedScale, usedScale, cv::INTER_LINEAR);
    }

    std::vector<cv::Rect> rects;
    if (!m_classifier.empty())
        m_classifier.detectMultiScale(gray, rects, 1.2, 3, 0, cv::Size(40, 40));

    if (usedScale != 1.0) {
        const double inv = 1.0 / usedScale;
        for (size_t i = 0; i < rects.size(); ++i) {
            cv::Rect &r = rects[i];
            r = cv::Rect(cvRound(r.x * inv), cvRound(r.y * inv),
                         cvRound(r.width * inv), cvRound(r.height * inv));
        }
    }

    m_faceRects.swap(rects);
    m_hasFace = !m_faceRects.empty();

    if (m_hasFace)
        cv::rectangle(m_frame, m_faceRects[m_faceRects.size() - 1], cv::Scalar(255, 0, 0), 3);

    emit frameReady(toQImage(m_frame), m_hasFace);
}

bool FaceWorker::currentFace(cv::Mat &face)
{
    if (!m_hasFace || m_faceRects.empty() || m_frame.empty())
        return false;

    const cv::Rect &src = m_faceRects[m_faceRects.size() - 1];
    // 与图像区域求交，避免 ROI 越界崩溃
    cv::Rect roi = src & cv::Rect(0, 0, m_frame.cols, m_frame.rows);
    if (roi.width <= 0 || roi.height <= 0)
        return false;

    face = m_frame(roi).clone();
    return !face.empty();
}

void FaceWorker::enroll(int label)
{
    if (m_recognizer.empty()) {
        emit enrollFinished(false, QStringLiteral("识别器未初始化"), label, QImage());
        return;
    }

    cv::Mat face;
    if (!currentFace(face)) {
        emit enrollFinished(false, QStringLiteral("未识别到有效人脸"), -1, QImage());
        return;
    }

    QImage faceImg = toQImage(face);

    cv::Mat gray;
    cv::cvtColor(face, gray, CV_BGR2GRAY);
    cv::resize(gray, gray, cv::Size(100, 100));

    m_trainFaces.push_back(gray);
    m_trainLabels.push_back(label);

    try {
        // LBPH 必须从全部样本重新训练，因此这里不可避免较慢，
        // 但已运行在子线程中，不会阻塞界面
        m_recognizer->update(m_trainFaces, m_trainLabels);
        if (!m_modelFile.isEmpty())
            m_recognizer->save(m_modelFile.toStdString());
    } catch (const cv::Exception &e) {
        emit enrollFinished(false, QStringLiteral("模型训练失败：%1").arg(e.what()), label, QImage());
        return;
    }

    emit enrollFinished(true, QString(), label, faceImg);
}

void FaceWorker::predict()
{
    if (m_recognizer.empty()) {
        emit predictFinished(-1, 0.0);
        return;
    }

    cv::Mat face;
    if (!currentFace(face)) {
        emit predictFinished(-1, 0.0);
        return;
    }

    cv::Mat gray;
    cv::cvtColor(face, gray, CV_BGR2GRAY);
    cv::resize(gray, gray, cv::Size(100, 100));

    int label = -1;
    double confidence = 0.0;
    try {
        m_recognizer->predict(gray, label, confidence);
    } catch (const cv::Exception &) {
        label = -1;
    }

    emit predictFinished(label, confidence);
}

QImage FaceWorker::toQImage(const cv::Mat &mat)
{
    if (mat.empty())
        return QImage();
    // 必须深拷贝：mat 底层数据会在下一帧被复用
    return QImage(mat.data, mat.cols, mat.rows,
                  static_cast<int>(mat.step),
                  QImage::Format_BGR888).copy();
}
