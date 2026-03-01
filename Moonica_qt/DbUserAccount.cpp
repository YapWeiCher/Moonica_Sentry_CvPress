#include "DbUserAccount.h"

DbUserAccount::DbUserAccount()
{

}

DbUserAccount::~DbUserAccount()
{

}

bool DbUserAccount::open(const QString& path)
{
	qDebug() << "USER ACCOUNT PATH: " << path;

	_userDb = QSqlDatabase::addDatabase("QSQLITE", "databaseConnection");
	_userDb.setDatabaseName(path);
	
	bool success = _userDb.open();

	if (success) {

		QString createUserTableQuery = "CREATE TABLE IF NOT EXISTS USER_ACCOUNT_DATA "
			"(USER_ID TEXT,USER_NAME TEXT,PASSWORD TEXT, ACCESS_LEVEL INT)";

		QSqlQuery query(_userDb);
		query.prepare(createUserTableQuery);
		success = query.exec();
		if (!success) {
			qDebug() << "Failed to create user table";
		}


	}

	return success;

}

bool DbUserAccount::insertAccontInfo(AccountInfo accInfo)
{
	bool flag = true;

	if (_userDb.isOpen())
	{
		QSqlQuery query(_userDb);

		query.prepare("INSERT INTO USER_ACCOUNT_DATA "
			"(USER_ID, USER_NAME, PASSWORD, ACCESS_LEVEL) "
			"VALUES (:userID, :userName, :password, :accessLevel)");
		query.bindValue(":userID", accInfo.userID);
		query.bindValue(":userName", accInfo.userName);
		query.bindValue(":password", accInfo.password);
		query.bindValue(":accessLevel", accInfo.accessLevel);


		// Execute the query and check for errors
		if (!query.exec()) {

			flag = false;
		}
		else
		{
			flag = true;
		}
	}
	else
	{
		flag = false;

	}

	return flag;
}
bool DbUserAccount::getAccountInfo(AccountInfo receivedAccInfo, AccountInfo& authenticatedAccInfo, bool accountExisted)
{
	QSqlQuery query(_userDb);
	query.prepare("SELECT USER_ID, USER_NAME ,PASSWORD, ACCESS_LEVEL FROM USER_ACCOUNT_DATA WHERE USER_NAME = :userName AND PASSWORD= :password ");
	query.bindValue(":userName", receivedAccInfo.userName);
	query.bindValue(":password", receivedAccInfo.password);



	if (query.exec()) {
		while (query.next()) {
			authenticatedAccInfo.userID = query.value("USER_ID").toString();
			authenticatedAccInfo.userName = query.value("USER_NAME").toString();
			authenticatedAccInfo.password = query.value("PASSWORD").toString();
			authenticatedAccInfo.accessLevel = static_cast<AccessLevel>(query.value("ACCESS_LEVEL").toInt());
			accountExisted = true;

		}
		return true;
	}
	else
	{
		return false;
	}
}
bool DbUserAccount::removeAccountInfo(AccountInfo accInfo)
{
	QString userNameToDelete = accInfo.userName;
	if (userNameToDelete.isEmpty()) return false;
	QString deleteRowQuery = "DELETE FROM USER_ACCOUNT_DATA WHERE USER_NAME = :userName";

	QSqlQuery query(_userDb);
	query.prepare(deleteRowQuery);
	query.bindValue(":userName", userNameToDelete);

	if (query.exec()) {
		qDebug() << "Row deleted successfully";
		return true;
	}
	else {
		qDebug() << "Error deleting row:" << query.lastError().text();
		return false;
	}
}

bool DbUserAccount::getAllAccountInfo(QVector<AccountInfo>& accountInfoVector)
{
	QSqlQuery query(_userDb);
	query.prepare("SELECT USER_ID, USER_NAME ,PASSWORD, ACCESS_LEVEL FROM USER_ACCOUNT_DATA ");

	if (query.exec()) {
		while (query.next()) {
			AccountInfo authenticatedAccInfo;
			authenticatedAccInfo.userID = query.value("USER_ID").toString();
			authenticatedAccInfo.userName = query.value("USER_NAME").toString();
			authenticatedAccInfo.password = query.value("PASSWORD").toString();
			authenticatedAccInfo.accessLevel = static_cast<AccessLevel>(query.value("ACCESS_LEVEL").toInt());

			accountInfoVector.append(authenticatedAccInfo);

		}
		return true;
	}
	else
	{
		return false;
	}
}