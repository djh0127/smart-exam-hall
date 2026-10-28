#ifndef MYSQL_H
#define MYSQL_H
#include <QSqlDatabase>
#include <QSqlQuery>
#include <vector>
#include <QDateTime>
using namespace std;
struct stuData
{
    int label;
    QString name;
    QDateTime start;
    QDateTime stop;
    stuData(int label,QString name,QDateTime start,QDateTime stop):label(label),name(name),start(start),stop(stop){}
};

class MySql
{
private:
    MySql();
    MySql(const MySql&);
    MySql& operator=(const MySql&);
    QSqlDatabase db;

    QSqlQuery m_qInsert;   // 预处理语句复用，避免每次访问都重新 prepare
    QSqlQuery m_qUpdate;
    QSqlQuery m_qSelect;

public:
    // 首次调用时才构造（此时 QApplication 一定已存在），避免静态初始化顺序问题
    static MySql& getSql()
    {
        static MySql sql;
        return sql;
    }
    bool insertStudent(QString name);
    int getStudentLabel();
    bool updateStopTime(int label);
    void selectStuAllInfo(vector<stuData>& v);
};
#endif // MYSQL_H
