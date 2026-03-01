#pragma once


#include <QString>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlDriver>
#include <QVariantList>
#include <QVector>
#include <QDate>
#include <QObject>
#include <QtWidgets/QMainWindow>

#include "CommonStruct.h"


class DbUserAccount : public QMainWindow
{
	Q_OBJECT
public:
	DbUserAccount();
	~DbUserAccount();

	bool open(const QString& path);

	bool insertAccontInfo(AccountInfo accInfo);
	bool getAccountInfo(AccountInfo receivedAccInfo, AccountInfo& authenticatedAccInfo, bool accountExisted = false);
	bool removeAccountInfo(AccountInfo accInfo);
	bool getAllAccountInfo(QVector<AccountInfo>& accountInfoVector);

public slots:



signals:
	

private:

	QSqlDatabase _userDb;
};
