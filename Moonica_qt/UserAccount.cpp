#include "UserAccount.h"




UserAccount::UserAccount(QDialog* parent)
	: QDialog(parent)
{
	ui.setupUi(this);
	qDebug() << "PathManagement::_userDataBasePath: " << PathManager::_dbUserAccountPath;
	if (!_sqliteDatabase.open(PathManager::_dbUserAccountPath))
	{
		qDebug() << "Open account database unsuccessful";
		reject();
	}
	else
	{
		qDebug() << "Open account database successful";

	}

	// get all account info
	_sqliteDatabase.getAllAccountInfo(_accountInfoVector);

	qDebug() << "_accountInfoVector size: " << _accountInfoVector.size();

	initWidget();
	refreshTableWidgetLoginAccount();

	connctSignalSlot();

	ui.label_4->hide();
	ui.lineEdit_loginPassword->hide();

}

void UserAccount::closeEvent(QCloseEvent* event)
{
	if (_curMode == LOGIN)
	{
		if (QMessageBox::Yes == QMessageBox(QMessageBox::Information,
			"Exit", "Are you sure to EXIT Result Viewer?"
			, QMessageBox::Yes | QMessageBox::No).exec())
		{
			event->accept();
		}
		else
		{
			event->ignore();
		}
	}

}

void UserAccount::setMode(User_Dialog_Mode mode, AccessLevel prevAccessLevel)
{
	_curMode = mode;

	ui.lineEdit_loginUserName->clear();
	ui.lineEdit_loginPassword->clear();
	ui.lineEdit_createAccUserName->clear();
	ui.lineEdit_createAccPassword->clear();

	if (_curMode == LOGIN)
	{
		ui.stackedWidget_userPage->setCurrentIndex(0);
		ui.pushButton_createUser->hide();
	}
	else if (_curMode == CREATE_ACCOUNT)
	{
		ui.stackedWidget_userPage->setCurrentIndex(1);
		ui.pushButton_cancelCreateAccount->hide();
	}

	_prevLoginAccessLevel = prevAccessLevel;
}

void UserAccount::connctSignalSlot()
{
	// login page
	QObject::connect(ui.comboBox_logInAccessLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
		ui.lineEdit_loginUserName->clear();
		ui.lineEdit_loginPassword->clear();
		if (ui.comboBox_logInAccessLevel->currentText() == "Admin")_curLoginAccessLevel = AccessLevel::ADMIN;
		else if (ui.comboBox_logInAccessLevel->currentText() == "Engineer")_curLoginAccessLevel = AccessLevel::ENGINEER;
		else if (ui.comboBox_logInAccessLevel->currentText() == "Operator")
		{
			ui.lineEdit_loginPassword->setText("-");
			ui.label_4->hide();
			ui.lineEdit_loginPassword->hide();
			_curLoginAccessLevel = AccessLevel::OPERATOR;
		}

		refreshTableWidgetLoginAccount();
		});

	connect(ui.pushButton_createUser, &QPushButton::clicked, this, [=]() {
		ui.stackedWidget_userPage->setCurrentIndex(1);
		});

	connect(ui.pushButton_logIn, SIGNAL(clicked()), this, SLOT(login()));
	connect(ui.tableWidget_loginAccount, SIGNAL(cellDoubleClicked(int, int)), this, SLOT(doubleClickedUserName(int, int)));


	// create account page
	connect(ui.pushButton_cancelCreateAccount, &QPushButton::clicked, this, [=]() {
		ui.stackedWidget_userPage->setCurrentIndex(0);
		});
	connect(ui.pushButton_createAccount, SIGNAL(clicked()), this, SLOT(createAccount()));
	QObject::connect(ui.comboBox_createAccAccessLevel, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](int index) {
		if (ui.comboBox_createAccAccessLevel->currentText() == "Admin")
		{
			_curLoginAccessLevel = AccessLevel::ADMIN;
			ui.label_7->show();
			ui.lineEdit_createAccPassword->show();
		}
		else if (ui.comboBox_createAccAccessLevel->currentText() == "Engineer")
		{
			_curLoginAccessLevel = AccessLevel::ENGINEER;
			ui.label_7->show();
			ui.lineEdit_createAccPassword->show();
		}
		else if (ui.comboBox_createAccAccessLevel->currentText() == "Operator")
		{
			_curLoginAccessLevel = AccessLevel::OPERATOR;
			ui.label_7->hide();
			ui.lineEdit_createAccPassword->hide();

		}

		refreshTableWidgetLoginAccount();
		});



}

void UserAccount::initWidget()
{
	ui.comboBox_logInAccessLevel->addItems(_accessLevelList);
	if (ui.comboBox_logInAccessLevel->currentText() == "Admin")_curLoginAccessLevel = AccessLevel::ADMIN;
	else if (ui.comboBox_logInAccessLevel->currentText() == "Engineer")_curLoginAccessLevel = AccessLevel::ENGINEER;
	else if (ui.comboBox_logInAccessLevel->currentText() == "Operator") _curLoginAccessLevel = AccessLevel::OPERATOR;

	ui.comboBox_createAccAccessLevel->addItems(_accessLevelList);

	_movieCctv = new QMovie(":/ResultViewer/Icon/cctv-ezgif.com-gif-maker.gif");
	connect(_movieCctv, &QMovie::frameChanged, this, [=](int frame) {
		ui.toolButton_movieCctv->setIcon(QIcon(_movieCctv->currentPixmap()));
		});

	if (_movieCctv->loopCount() != -1) connect(_movieCctv, SIGNAL(finished()), _movieCctv, SLOT(start()));

	_movieCctv->start();


}
void UserAccount::doubleClickedUserName(int row, int col)
{

	if (_curLoginAccessLevel != AccessLevel::OPERATOR)
	{
		ui.label_4->show();
		ui.lineEdit_loginPassword->show();
	}
	else
	{
		ui.lineEdit_loginPassword->setText("-");
		ui.label_4->hide();
		ui.lineEdit_loginPassword->hide();
		_curLoginAccessLevel = AccessLevel::OPERATOR;

	}
	ui.lineEdit_loginUserName->setText(ui.tableWidget_loginAccount->item(row, col)->text());
}

void UserAccount::refreshTableWidgetLoginAccount()
{
	ui.tableWidget_loginAccount->blockSignals(true);

	//int selectedRow = ui.tableWidget_loginAccount->currentRow();
	//qDebug() << "SelectedRow: " << selectedRow;

	ui.tableWidget_loginAccount->clear();

	ui.tableWidget_loginAccount->setRowCount(0);

	QStringList horizontalLabel = { "User Name" };

	int col = horizontalLabel.size();
	int row = 0;


	//ui.tableWidget_voTag->setRowCount(row);
	ui.tableWidget_loginAccount->setColumnCount(col);

	for (int i = 0; i < _accountInfoVector.size(); i++)
	{
		QString userName = _accountInfoVector[i].userName;
		AccessLevel userlevel = _accountInfoVector[i].accessLevel;

		if (_curLoginAccessLevel != userlevel) continue;
		// filter accesslevel 

		ui.tableWidget_loginAccount->insertRow(row);


		ui.tableWidget_loginAccount->setItem(row, 0, new QTableWidgetItem(userName));
		ui.tableWidget_loginAccount->item(row, 0)->setFlags(ui.tableWidget_loginAccount->item(row, 0)->flags() & ~Qt::ItemIsEditable);


		row++;
	}

	ui.tableWidget_loginAccount->sortItems(0, Qt::AscendingOrder);
	ui.tableWidget_loginAccount->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	ui.tableWidget_loginAccount->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
	ui.tableWidget_loginAccount->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);

	ui.tableWidget_loginAccount->setHorizontalHeaderLabels(horizontalLabel);


	ui.tableWidget_loginAccount->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui.tableWidget_loginAccount, &QTableWidget::customContextMenuRequested, [this](const QPoint& pos) {
		QTableWidgetItem* item = ui.tableWidget_loginAccount->itemAt(pos);
		if (item)
		{
		
			QMenu contextMenu;
			QAction deleteAction("Delete", &contextMenu);
			connect(&deleteAction, &QAction::triggered, [this, item]() {
				QString userName = item->text();



				if (_prevLoginAccessLevel == AccessLevel::ADMIN)
				{
					if (QMessageBox::Yes == QMessageBox(QMessageBox::Information,
						"Remove User", "Are you sure to remove user: " + userName + " ?"
						, QMessageBox::Yes | QMessageBox::No).exec())
					{
						AccountInfo a;
						a.userName = userName;
						_sqliteDatabase.removeAccountInfo(a);
						_accountInfoVector.clear();
						_sqliteDatabase.getAllAccountInfo(_accountInfoVector);
						refreshTableWidgetLoginAccount();
					}
				}
				else
				{
					QMessageBox::information(
						this,
						"Delete cancelled",
						"Only Admin can delete user account!"
					);
				}

				});
			contextMenu.addAction(&deleteAction);
			contextMenu.exec(ui.tableWidget_loginAccount->mapToGlobal(pos));


		}

		});

	ui.tableWidget_loginAccount->blockSignals(false);
}



AccountInfo UserAccount::getCurrentAccount()
{
	return _loginAccount;
}

void UserAccount::login()
{
	if (ui.lineEdit_loginUserName->text().isEmpty())
	{
		QMessageBox::warning(this, tr("No UserName selected"),
			"Please select user account to login");
		return;
	}
	else
	{
		QString userName = ui.lineEdit_loginUserName->text();
		QString password = ui.lineEdit_loginPassword->text();

		bool sucLogIn = checkUserPassword(userName, password);
		if (sucLogIn)
		{
			AccountInfo aInfo;
			aInfo.userName = userName;
			aInfo.password = password;

			_sqliteDatabase.getAccountInfo(aInfo, aInfo);
			_loginAccount = aInfo;


			accept();
		}
		else
		{
			QMessageBox::warning(this, tr("Incorrect Password"),
				"Incorrect Password");
			return;
		}
	}
}
void UserAccount::createAccount()
{
	AccountInfo lAccount;
	QString userName;
	QString userID;
	QString password;
	AccessLevel accessLevel;


	userName = ui.lineEdit_createAccUserName->text();
	// check if user exist
	bool userExist = checkUserExist(userName);
	if (userName.isEmpty())
	{
		QMessageBox::warning(this, tr("No UserName input"),
			"Please enter username, password and access level to create a proper account!");
		return;
	}
	if (userExist)
	{

		QMessageBox::warning(this, tr("User Exist"),
			"UserName: " + userName + " has already existed, please try other UserName");
		return;
	}
	else
	{

		std::string id;
		uint64_t microseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
		id = std::to_string(microseconds_since_epoch);
		userID = QString::fromStdString(id);

		if (ui.comboBox_createAccAccessLevel->currentText() == "Admin")accessLevel = AccessLevel::ADMIN;
		else if (ui.comboBox_createAccAccessLevel->currentText() == "Engineer")accessLevel = AccessLevel::ENGINEER;
		else if (ui.comboBox_createAccAccessLevel->currentText() == "Operator") accessLevel = AccessLevel::OPERATOR;

		if (accessLevel != AccessLevel::OPERATOR)
		{
			password = ui.lineEdit_createAccPassword->text().simplified();
			if (password.isEmpty())
			{
				QMessageBox::warning(this, tr("No password setup"),
					"Please set up password to create a proper account");
				return;
			}
		}
		else
		{
			password = "-";
		}


		lAccount.userID = userID;
		lAccount.userName = userName;
		lAccount.password = password;
		lAccount.accessLevel = accessLevel;

		if (_sqliteDatabase.insertAccontInfo(lAccount))
		{
			QMessageBox::information(this, tr("Acccount setup succesfully"),
				"Success");
			ui.stackedWidget_userPage->setCurrentIndex(0);
			_accountInfoVector.clear();
			_sqliteDatabase.getAllAccountInfo(_accountInfoVector);
			refreshTableWidgetLoginAccount();

			ui.comboBox_logInAccessLevel->setCurrentIndex(ui.comboBox_logInAccessLevel->findText(ui.comboBox_createAccAccessLevel->currentText()));
			return;
		}
		else
		{
			QMessageBox::warning(this, tr("Failed to setup account"),
				"Failed");
			return;
		}

	}
}


bool UserAccount::checkUserExist(QString userName)
{
	for (int i = 0; i < _accountInfoVector.size(); i++)
	{
		if (_accountInfoVector[i].userName == userName)
		{
			return true;
		}
	}
	return false;
}

bool UserAccount::checkUserPassword(QString userName, QString password)
{
	for (int i = 0; i < _accountInfoVector.size(); i++)
	{
		if (_accountInfoVector[i].userName == userName)
		{
			if (_accountInfoVector[i].password == password)
			{
				return true;
			}
		}
	}
	return false;
}

QStringList UserAccount::getAdminPassword()
{
	QStringList adminPasswords;
	for (int i = 0; i < _accountInfoVector.size(); i++)
	{
		if (_accountInfoVector[i].accessLevel == ADMIN)
		{
			adminPasswords.append(_accountInfoVector[i].password);
		}
	}
	return adminPasswords;
}

UserAccount::~UserAccount()
{

}
