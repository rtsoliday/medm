#include <QApplication>
#include <QMainWindow>
#include <QAction>
#include <QMenuBar>

// Entry point
int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // High-DPI is on by default in Qt6
#else
    // Opt-in for sensible DPI scaling on Qt5
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QApplication app(argc, argv);

    QMainWindow win;
    win.setWindowTitle("Blank Qt GUI");

    // Optional: a tiny "File → Quit" so you can close with Alt+F, Q
    auto *fileMenu = win.menuBar()->addMenu("&File");
    auto *quitAct = fileMenu->addAction("&Quit");
    quitAct->setShortcut(QKeySequence::Quit);
    QObject::connect(quitAct, &QAction::triggered, &app, &QApplication::quit);

    win.resize(640, 400);
    win.show();
    return app.exec();
}
