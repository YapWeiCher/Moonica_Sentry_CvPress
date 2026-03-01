#pragma once

#include <QWidget>
#include <QDialog>
#include <QShortcut>
#include <QCloseEvent>
#include <QMenu>
#include "ui_UserAccount.h"
#include "DbUserAccount.h"
#include "CommonStruct.h" 
#include "PathManager.h" 
#include <QMessageBox>
#include <QMovie>

class UserAccount : public QDialog
{
	Q_OBJECT

public:
	 UserAccount(QDialog *parent = Q_NULLPTR);
	~UserAccount();
	void closeEvent(QCloseEvent *bar);
	void connctSignalSlot();
	void initWidget();
	void refreshTableWidgetLoginAccount();
	void setMode(User_Dialog_Mode mode, AccessLevel prevAccessLevel);

	bool checkUserExist(QString userName);
	bool checkUserPassword(QString userName, QString password);
	AccountInfo getCurrentAccount();

	QStringList getAdminPassword();

private:
	Ui::UserAccount ui;
	QMovie* _movieCctv;
	
	AccountInfo _loginAccount;
	User_Dialog_Mode _curMode;
	QVector<AccountInfo> _accountInfoVector;

	AccessLevel _curLoginAccessLevel;
	AccessLevel _prevLoginAccessLevel = AccessLevel::OPERATOR;

	QStringList _accessLevelList = {
		"Admin",
		"Engineer",
		"Operator"
	};
	
	DbUserAccount _sqliteDatabase;
public slots:
	void login();
	void createAccount();
	void doubleClickedUserName(int, int);


};


