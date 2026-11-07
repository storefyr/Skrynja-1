#include <QtTest>
#include <QCoreApplication>

class TestSkeleton : public QObject
{
    Q_OBJECT
private slots:
    void placeholder() { QVERIFY(true); }
};

QTEST_APPLESS_MAIN(TestSkeleton)
#include "tst_main.moc"
