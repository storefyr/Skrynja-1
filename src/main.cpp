#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQuickView>
#include <QQmlContext>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <QSettings>
#include "acpclient.h"

static QFile g_logFile(QStringLiteral("/tmp/skrynja-1-app.log"));

static void logToFile(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    if (g_logFile.isOpen()) {
        g_logFile.write(QDateTime::currentDateTime().toString(Qt::ISODate).toUtf8());
        g_logFile.write(" ");
        g_logFile.write(msg.toUtf8());
        g_logFile.write("\n");
        g_logFile.flush();
    }
    fprintf(stderr, "%s\n", msg.toUtf8().constData());
    fflush(stderr);
}

int main(int argc, char *argv[])
{
    g_logFile.open(QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(logToFile);

    QGuiApplication *app = SailfishApp::application(argc, argv);
    app->setOrganizationName(QStringLiteral("org.skrynja"));
    app->setApplicationName(QStringLiteral("Skrynja-1"));

    bool selvtest = false;
    for (int i = 1; i < argc; i++)
        if (QLatin1String(argv[i]) == QLatin1String("--selvtest"))
            selvtest = true;

    QSettings settings;
    const QString savedSession = settings.value(QStringLiteral("acp/sessionId")).toString();

    AcpClient acp;
    acp.setSelvtest(selvtest);
    QObject::connect(&acp, &AcpClient::sessionIdChanged, [&acp]() {
        QSettings s;
        s.setValue(QStringLiteral("acp/sessionId"), acp.sessionId());
    });
    acp.start(AcpClient::defaultAgentPath(), QStringLiteral("/home/defaultuser"), savedSession);

    QQuickView *view = SailfishApp::createView();
    view->rootContext()->setContextProperty(QStringLiteral("acp"), &acp);
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-skrynja-1.qml")));
    view->show();

    if (selvtest) {
        QObject::connect(&acp, &AcpClient::promptDone, &acp, [&acp]() {
            QTimer::singleShot(800, &acp, [&acp]() { acp.finishSelvtest(); });
        });
    }

    return app->exec();
}
