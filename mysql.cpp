#include "mysql.h"
#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QVariant>

MySql::MySql()
    : db(QSqlDatabase::addDatabase("QSQLITE"))
{
    db.setDatabaseName("exam.db");
    if (!db.open())
    {
        qDebug() << "数据库打开失败：" << db.lastError().text();
        return;
    }
    qDebug() << "数据库打开成功";

    // WAL + NORMAL 同步：大幅降低写盘阻塞，避免插入时界面抖动
    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL;");
    pragma.exec("PRAGMA synchronous=NORMAL;");

    QSqlQuery query(db);
    QString sql = R"(create table IF NOT EXISTS stu_info(
        stu_label integer primary key autoincrement,
        stu_name varchar(20),
        start_time datetime,
        stop_time datetime
    ))";
    bool ok = query.exec(sql);
    if (ok)
        qDebug() << "创建表成功";
    else
        qDebug() << "创建表失败：" << query.lastError().text();

    m_qInsert = QSqlQuery(db);
    m_qInsert.prepare("INSERT into stu_info (stu_name,start_time,stop_time) VALUES (?,datetime('now','localtime'),NULL);");

    m_qUpdate = QSqlQuery(db);
    m_qUpdate.prepare("UPDATE stu_info set stop_time=datetime('now','localtime') where stu_label=?;");

    m_qSelect = QSqlQuery(db);
    m_qSelect.prepare("select * from stu_info;");
}

MySql::MySql(const MySql &)
    : db(QSqlDatabase())
{
}

MySql& MySql::operator=(const MySql&)
{
    return *this;
}

bool MySql::insertStudent(QString name)
{
    m_qInsert.bindValue(0, name);
    bool ret = m_qInsert.exec();
    if (!ret)
        qDebug() << "insert失败详情：" << m_qInsert.lastError().text();
    return ret;
}

int MySql::getStudentLabel()
{
    QSqlQuery q(db);
    if (!q.exec("SELECT last_insert_rowid()"))
    {
        qDebug() << "getStudentLabel 失败：" << q.lastError().text();
        return -1;
    }
    int label = -1;
    if (q.next())
        label = q.value(0).toInt();
    return label;
}

bool MySql::updateStopTime(int label)
{
    m_qUpdate.bindValue(0, label);
    bool ret = m_qUpdate.exec();
    if (!ret)
        qDebug() << "updateStopTime失败：" << m_qUpdate.lastError().text();
    return ret;
}

void MySql::selectStuAllInfo(vector<stuData> &v)
{
    m_qSelect.setForwardOnly(true);
    if (!m_qSelect.exec())
    {
        qDebug() << "查询失败：" << m_qSelect.lastError().text();
        return;
    }
    while (m_qSelect.next())
    {
        stuData data(m_qSelect.value(0).toInt(),
                     m_qSelect.value(1).toString(),
                     m_qSelect.value(2).toDateTime(),
                     m_qSelect.value(3).toDateTime());
        v.push_back(data);
    }
}
