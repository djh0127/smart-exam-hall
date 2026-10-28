#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "mysql.h"
#include "faceworker.h"

#include <QGridLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMessageBox>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMetaObject>
#include <QDebug>
#include <cstdlib>
#include <ctime>

static const char* kCascadeFile =
        "D:/opencv/opencv3.4-install/install/etc/haarcascades/haarcascade_frontalface_alt2.xml";
static const char* kModelFile = "face.xml";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_worker(nullptr)
    , m_thread(nullptr)
    , stuCount(0)
    , m_busy(false)
    , m_hasFace(false)
{
    ui->setupUi(this);
    MySql::getSql();

    // 只设置一次，不必每帧调用
    ui->label_camera->setScaledContents(true);

    srand(time(0));   // 只播种一次

    m_worker = new FaceWorker;
    m_worker->setCascadeFile(QString(kCascadeFile));
    m_worker->setModelFile(QString(kModelFile));
    m_worker->setThreshold(150);

    m_thread = new QThread(this);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &FaceWorker::frameReady,       this, &MainWindow::updateFrame);
    connect(m_worker, &FaceWorker::enrollFinished,   this, &MainWindow::onEnrollFinished);
    connect(m_worker, &FaceWorker::predictFinished,  this, &MainWindow::onPredictFinished);

    // 跨线程自动排队执行，耗时操作全部在子线程完成
    connect(this, &MainWindow::startCamera,      m_worker, &FaceWorker::start);
    connect(this, &MainWindow::enrollRequested,  m_worker, &FaceWorker::enroll);
    connect(this, &MainWindow::predictRequested, m_worker, &FaceWorker::predict);

    m_thread->start();
    emit startCamera(0);
}

MainWindow::~MainWindow()
{
    // 阻塞式调用，保证在线程结束前释放摄像头
    QMetaObject::invokeMethod(m_worker, "stop", Qt::BlockingQueuedConnection);
    m_thread->quit();
    m_thread->wait();
    delete m_worker;      // 线程已停止，可安全释放
    m_worker = nullptr;
    delete ui;
}

void MainWindow::updateFrame(const QImage &img, bool hasFace)
{
    m_hasFace = hasFace;
    if (img.isNull())
        return;
    ui->label_camera->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::recvCourceInfo(QString name, int stuCount)
{
    this->stuCount = stuCount;
    ui->label_examname->setText(name + "考试");
    for (int i = 0; i < stuCount; i++)
    {
        QLabel* label = new QLabel(this);
        label->setFrameShape(QFrame::Box);
        label->setPixmap(QPixmap(":/head.png"));
        label->setScaledContents(true);
        label->setAlignment(Qt::AlignCenter);
        label->setObjectName("0");
        ui->gridLayout->addWidget(label, i / 6, i % 6);
    }
}

void MainWindow::setBusy(bool busy)
{
    m_busy = busy;
    ui->pushButton_start->setEnabled(!busy);
    ui->pushButton_exit->setEnabled(!busy);
    if (busy)
        statusBar()->showMessage(QStringLiteral("正在识别/训练模型，请稍候..."));
    else
        statusBar()->clearMessage();
}

void MainWindow::on_pushButton_start_clicked()
{
    if (m_busy)
        return;

    QString name = ui->lineEdit->text().trimmed();
    if (name.isEmpty())
    {
        QMessageBox::information(this, "error", "请输入考生姓名");
        return;
    }
    if (stuCount <= 0 || static_cast<int>(map_students.size()) >= stuCount)
    {
        QMessageBox::information(this, "提示", "座位已满");
        return;
    }
    if (!m_hasFace)
    {
        QMessageBox::information(this, "error", "未识别到有效人脸");
        return;
    }

    // 数据库操作在本地 SQLite 上耗时极小，留在界面线程顺序执行即可
    bool ok = MySql::getSql().insertStudent(name);
    if (!ok)
    {
        QMessageBox::information(this, "error", "数据库插入失败");
        return;
    }
    int label = MySql::getSql().getStudentLabel();
    if (label == -1)
    {
        QMessageBox::information(this, "error", "数据库标签异常");
        return;
    }

    setBusy(true);
    m_pendingName = name;
    qDebug() << "录入学生的标签为" << label;
    emit enrollRequested(label);
}

void MainWindow::onEnrollFinished(bool ok, const QString &msg, int label, const QImage &faceImg)
{
    setBusy(false);

    if (!ok)
    {
        QMessageBox::information(this, "error", msg);
        return;
    }

    PosAndName pos;
    pos.stuName = m_pendingName;
    if (!pickSeat(pos))
    {
        QMessageBox::information(this, "提示", "座位已满");
        return;
    }

    changeLabelImg(faceImg, pos, 0);
    map_students.insert(std::map<int, PosAndName>::value_type(label, pos));
    ui->lineEdit->clear();
    QMessageBox::information(this, "提示", "录入成功请开始考试");
}

void MainWindow::on_pushButton_exit_clicked()
{
    if (m_busy)
        return;
    if (!m_hasFace)
    {
        QMessageBox::information(this, "提示", "未找到学生");
        return;
    }
    setBusy(true);
    emit predictRequested();
}

void MainWindow::onPredictFinished(int label, double confidence)
{
    setBusy(false);

    if (label < 0 || map_students.count(label) == 0)
    {
        QMessageBox::information(this, "提示", "未匹配人员信息");
        return;
    }

    PosAndName pos = map_students[label];
    qDebug() << "学生姓名" << pos.stuName << "置信度" << confidence;
    int choose = QMessageBox::information(this, "提示", pos.stuName + "确定结束考试吗",
                                          QMessageBox::Ok | QMessageBox::Cancel);
    if (choose == QMessageBox::Ok)
    {
        MySql::getSql().updateStopTime(label);
        changeLabelImg(QImage(), pos, 1);   // 释放座位并显示结束考试
        map_students.erase(label);
    }
}

bool MainWindow::pickSeat(PosAndName &pos)
{
    QList<QPair<int, int> > freeSeats;
    for (int i = 0; i < stuCount; i++)
    {
        int row = i / 6;
        int column = i % 6;
        QLayoutItem *item = ui->gridLayout->itemAtPosition(row, column);
        if (!item || !item->widget())
            continue;
        if (item->widget()->objectName() == "1")   // 已占用
            continue;
        freeSeats.append(qMakePair(row, column));
    }
    if (freeSeats.isEmpty())
        return false;

    const QPair<int, int> &seat = freeSeats.at(rand() % freeSeats.size());
    pos.row = seat.first;
    pos.column = seat.second;
    return true;
}

void MainWindow::changeLabelImg(const QImage &face, const PosAndName &value, int flag)
{
    QLayoutItem *item = ui->gridLayout->itemAtPosition(value.row, value.column);
    if (!item || !item->widget())
        return;
    QLabel *label = qobject_cast<QLabel *>(item->widget());
    if (label == nullptr)
        return;

    if (flag == 1)
    {
        label->setPixmap(QPixmap());          // 先清掉图像，保证文字能显示出来
        label->setText(value.stuName + "结束考试");
        label->setObjectName("0");            // 座位释放
        return;
    }

    label->setObjectName("1");                // 座位占用
    if (!face.isNull())
        label->setPixmap(QPixmap::fromImage(face));
}

void MainWindow::on_pushButton_information_clicked()
{
    vector<stuData> infos;
    MySql::getSql().selectStuAllInfo(infos);

    QFile file("data.csv");
    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate))
        return;
    QTextStream stream(&file);

    QList<QString> titles = {"标签", "姓名", "开始考试时间", "结束考试时间"};
    for (auto t : titles)
        stream << t << ",";
    stream << '\n';

    for (auto temp : infos)
    {
        stream << temp.label << ","
               << temp.name << ","
               << temp.start.toString("yyyy-MM-dd hh:mm:ss") << ","
               << temp.stop.toString("yyyy-MM-dd hh:mm:ss");
        stream << '\n';
    }
    file.close();
    QMessageBox::information(this, "提示", "导出完成");
}
