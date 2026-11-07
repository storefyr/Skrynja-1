#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQuickView>

int main(int argc, char *argv[])
{
    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName(QStringLiteral("org.skrynja"));
    app->setApplicationName(QStringLiteral("Skrynja-1"));

    QQuickView *view = SailfishApp::createView();
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-skrynja-1.qml")));
    view->show();

    return app->exec();
}
