#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("M1Launcher");
    app.setOrganizationName("M1Project");

    // Fusion スタイルを適用してモダンで一貫した UI にする
    if (QStyleFactory::keys().contains("Fusion")) {
        app.setStyle(QStyleFactory::create("Fusion"));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
