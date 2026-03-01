#include "Moonica_qt.h"
#include <QtWidgets/QApplication>
#include "Windows.h"
#include <cstdio>

#include "MessageQue.h"
#include "QFrameless.h"


TMessageQue<FrameInfo> g_imageQueue;


static void Console()
{
	FreeConsole();
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	freopen("CON", "w", stderr);
	freopen("CON", "r", stdin);
}

int main(int argc, char *argv[])
{
	Console();
    QApplication app(argc, argv);
	/*QFile file(":/ResultViewer/Resources/styleSheet.qss");
	file.open(QFile::ReadOnly);
	QString styleSheet = QLatin1String(file.readAll());
	app.setStyleSheet(styleSheet);*/

    Moonica_qt w;
	FrameLess f(w.window(), &w);
	QList<QScreen*> screens = QGuiApplication::screens();

	if (screens.size() > 1) {
		QScreen* screen = screens.at(1); // Assuming the second screen is at index 1

		// Get the geometry of the second screen
		QRect screenGeometry = screen->geometry();
		w.setGeometry(screenGeometry);

		HWND hWndConsole = GetConsoleWindow();
		MoveWindow(hWndConsole, screenGeometry.x(), screenGeometry.y(), 1000, 1000, TRUE);
	}

	w.showFullScreen();


    return app.exec();
}
