#include "courcedialog.h"
#include "ui_courcedialog.h"
#include "mainwindow.h"
CourceDialog::CourceDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CourceDialog)
{
    ui->setupUi(this);
    for(int i=30;i<=50;i++)
    {
        ui->comboBox->addItem(QString::number(i));
    }
}

CourceDialog::~CourceDialog()
{
    delete ui;
}

void CourceDialog::on_pushButton_start_clicked()
{
    QString name=ui->lineEdit_name->text();
    int stuCount=ui->comboBox->currentText().toInt();
    MainWindow* mw=new MainWindow();
    mw->setAttribute(Qt::WA_DeleteOnClose);   // 关闭新窗口时自动释放内存
    connect(this,&CourceDialog::sendCourceInfo,mw,&MainWindow::recvCourceInfo);
    mw->show();
    emit sendCourceInfo(name,stuCount);
    this->close();
}


