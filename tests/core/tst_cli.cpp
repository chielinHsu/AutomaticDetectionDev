// 命令列：aoi.exe 的 --backend 參數（組裝根的行為，從外面用真的行程驗證）。
// 沒有編進程式的 back end 要「明確失敗」：結束碼 2、stderr 有原因與可用的 back end；預設與 fake 要能啟動。
#include <QtTest>
#include <QProcess>
#include <QProcessEnvironment>

namespace {

QProcessEnvironment offscreen() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QT_QPA_FONTDIR"), QStringLiteral(AOI_FONT_DIR));
    return env;
}

}  // namespace

class TstCli : public QObject {
    Q_OBJECT

private slots:
    void unavailableBackendsFailClearly_data() {
        QTest::addColumn<QString>("id");
        QTest::addColumn<QString>("mustMention");
        QTest::newRow("opencv") << "opencv" << "step S07";
        QTest::newRow("halcon") << "halcon" << "step S08";
        QTest::newRow("unknown id") << "turbo" << "no back end with the id 'turbo'";
    }
    void unavailableBackendsFailClearly() {
        QFETCH(QString, id);
        QFETCH(QString, mustMention);
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=") + id});
        QVERIFY2(process.waitForFinished(20000), "the program must refuse to start, not keep running");
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        const QString err = QString::fromUtf8(process.readAllStandardError());
        QVERIFY2(err.contains(mustMention), qPrintable(err));
        QVERIFY2(err.contains(QStringLiteral("Available back ends: fake")), qPrintable(err));
    }

    void theBackendOptionAcceptsASpaceSeparatedValueToo() {
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend"), QStringLiteral("halcon")});
        QVERIFY(process.waitForFinished(20000));
        QCOMPARE(process.exitCode(), 2);
    }

    void aBadThemeStillFailsWithExitCodeTwoAndAMessage() {
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--theme=neon")});
        QVERIFY(process.waitForFinished(20000));
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(QString::fromUtf8(process.readAllStandardError()).contains(QStringLiteral("Unknown theme 'neon'")));
    }

    void theFakeBackendAndTheDefaultStartAndKeepRunning_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("default") << QStringList{};
        QTest::newRow("--backend=fake") << QStringList{QStringLiteral("--backend=fake")};
    }
    void theFakeBackendAndTheDefaultStartAndKeepRunning() {
        QFETCH(QStringList, arguments);
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), arguments);
        QVERIFY(process.waitForStarted(20000));
        QVERIFY2(!process.waitForFinished(2500), "the program must still be running (it shows the window and waits)");
        QCOMPARE(process.state(), QProcess::Running);
        process.kill();
        process.waitForFinished(10000);
    }
};

QTEST_GUILESS_MAIN(TstCli)
#include "tst_cli.moc"
