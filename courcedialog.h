#ifndef COURCEDIALOG_H
#define COURCEDIALOG_H

#include <QDialog>

namespace Ui {
class CourceDialog;
}

class CourceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CourceDialog(QWidget *parent = nullptr);
    ~CourceDialog();
signals:
    void sendCourceInfo(QString name,int count);
private slots:
    void on_pushButton_start_clicked();

private:
    Ui::CourceDialog *ui;
};

#endif // COURCEDIALOG_H
