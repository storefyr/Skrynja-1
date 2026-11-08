#include <sailfishapp.h>
#include <QGuiApplication>
#include <QQuickView>
#include <QQuickItem>
#include <QQmlContext>
#include <QFile>
#include <QDateTime>
#include <QTimer>
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

    AcpClient acp;
    acp.setSelvtest(selvtest);
    acp.start(AcpClient::defaultAgentPath(), QStringLiteral("/home/defaultuser"));

    QQuickView *view = SailfishApp::createView();
    view->rootContext()->setContextProperty(QStringLiteral("acp"), &acp);
    view->setSource(SailfishApp::pathTo(QStringLiteral("qml/harbour-skrynja-1.qml")));
    view->show();

    if (selvtest) {
        QObject::connect(&acp, &AcpClient::promptDone, &acp, [view, &acp]() {
            QTimer::singleShot(800, &acp, [view, &acp]() {
                QObject *root = view->rootObject();
                const QVariant ui = root->property("uiToolCalls");
                qDebug() << "[selvtest] root uiToolCalls=" << ui << " root=" << root;
                acp.finishSelvtest(ui.isValid() ? ui.toInt() : -1);
            });
        });
    }

    return app->exec();
}
