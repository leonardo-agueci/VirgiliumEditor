//
// Created by pinoOgni on 10/08/20.
//

#ifndef PERSONALPAGE_H
#define PERSONALPAGE_H

#include <QMainWindow>
#include <QDateTime>
#include <QTimer>
#include <QFont>
#include <QMessageBox>
#include <QDebug>
#include "../clientstuff.h"

namespace Ui {
class PersonalPage;
}

class PersonalPage : public QMainWindow
{
    Q_OBJECT

public:
    explicit PersonalPage(QWidget *parent = nullptr);
    ~PersonalPage();
    void databaseManagement(QString);

private slots:
    void on_logout_clicked();
    void on_newFile_clicked();
    void on_filename_returnPressed();
    void on_tableWidget_cellDoubleClicked(int row, int column);
    void on_actionChange_Password_triggered();
    void on_actionDefault_triggered();
    void on_actionDark_triggered();
    void on_actionLight_triggered();
    void newFile(QString,QString);

    void time_label();

    void receiveData(QString,ClientStuff*);

    void getInfoUser();
    void getFilesOwner();
    void getUserFiles();

    void getAllData();

    void getInfoUser2(UserMessage& u);
    void getFilesOwner2(_int row,std::vector<FilesMessage>& filesMessage);
    void getUserFiles2(_int row,std::vector<FilesMessage>& filesMessage);
    void isFileCreated(bool);
    void getAllData2(UserMessage& , _int,std::vector<FilesMessage>&,_int,std::vector<FilesMessage>&);

    void on_tableWidget_2_cellDoubleClicked(int row, int column);
signals:
    void Want2Close();
    void sendData(ClientStuff *,QString); //send client and password to changepassworddialog

    //send client, email and filename to deleteOrRename dialog
    void sendData_2(ClientStuff *,QString,QString);
    void sendData_3(ClientStuff *,QString,QString,QString);


private:
    Ui::PersonalPage *ui;
    QString email;
    //I dunno
    QTimer *timer;
    QTimer *updateTimer;

    ClientStuff *client;

};

#endif //PERSONALPAGE_H