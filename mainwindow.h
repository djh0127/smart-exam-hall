#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QImage>
#include <map>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FaceWorker;
class QThread;

struct PosAndName{
  int column;
  int row;
  QString stuName;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void recvCourceInfo(QString name,int stuCount);

private slots:
    void on_pushButton_start_clicked();

    void on_pushButton_exit_clicked();

    void on_pushButton_information_clicked();

    void updateFrame(const QImage &img, bool hasFace);

    void onEnrollFinished(bool ok, const QString &msg, int label, const QImage &faceImg);

    void onPredictFinished(int label, double confidence);

signals:
    void startCamera(int cameraIndex);
    void enrollRequested(int label);
    void predictRequested();

private:
    void changeLabelImg(const QImage &face, const PosAndName &value, int flag);
    bool pickSeat(PosAndName &pos);
    void setBusy(bool busy);

private:
    Ui::MainWindow *ui;
    FaceWorker *m_worker;
    QThread *m_thread;
    int stuCount;
    bool m_busy;
    bool m_hasFace;
    QString m_pendingName;
    std::map<int,PosAndName> map_students;
};
#endif // MAINWINDOW_H
