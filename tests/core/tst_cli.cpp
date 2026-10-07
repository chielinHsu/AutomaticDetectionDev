// 命令列：aoi.exe 的 --backend 參數（組裝根的行為，從外面用真的行程驗證）。
// 沒有編進程式的 back end 要「明確失敗」：結束碼 2、stderr 有原因與可用的 back end；預設與 fake 要能啟動。
#include <QtTest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#ifdef AOI_WITH_HALCON
#include "infrastructure/backend_halcon/HalconLicense.h"
#endif

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
#ifndef AOI_WITH_OPENCV
        // S07 起 OpenCV back end 有實作；沒有編進這個程式時，訊息說怎麼編進來。
        QTest::newRow("opencv") << "opencv" << "AOI_WITH_OPENCV=ON";
#endif
#ifndef AOI_WITH_HALCON
        // S08 起 HALCON back end 有實作；沒有編進程式時，訊息說怎麼編進來。
        QTest::newRow("halcon") << "halcon" << "AOI_WITH_HALCON=ON";
#endif
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
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend"), QStringLiteral("turbo")});
        QVERIFY(process.waitForFinished(20000));
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(QString::fromUtf8(process.readAllStandardError()).contains(QStringLiteral("'turbo'")));
    }

#ifdef AOI_WITH_HALCON
    // ---- S08：HALCON 編進程式時的優雅降級（從外面用真的行程驗證；halconcpp.dll 是延遲載入的）----

    // 這台機器有授權：--backend=halcon 要能啟動；授權種類不對（runtime 授權、SDK 的 dongle 沒插）時 stderr 有提醒。
    // 沒有授權的機器：要明確失敗（結束碼 2、原因）。哪一種由 HALCON 自己說（halconLicense），兩種都是確定的結果。
    void halconStartsWhenLicensedAndFailsClearlyOtherwise() {
        const aoi::infra::HalconLicenseVerdict verdict = aoi::infra::judgeHalconLicense(aoi::infra::halconLicense());
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=halcon")});
        QVERIFY(process.waitForStarted(20000));
        if (verdict.usable) {
            // 注意：QVERIFY2 的訊息在通過時也會求值，所以 stderr 只在最後讀一次（讀過就沒了）。
            const bool exited = process.waitForFinished(8000);
            if (!exited) {
                process.kill();
                process.waitForFinished(10000);
            }
            const QString err = QString::fromUtf8(process.readAllStandardError());
            QVERIFY2(!exited, qPrintable(QStringLiteral("the program must keep running; stderr: ") + err));
            for (const aoi::infra::HalconLicenseNotice& n : verdict.notices) {
                QVERIFY2(err.contains(QString::fromStdString(n.missingId)), qPrintable(QStringLiteral("stderr: '%1'").arg(err)));
            }
        } else {
            QVERIFY(process.waitForFinished(20000));
            QCOMPARE(process.exitCode(), 2);
            QVERIFY2(QString::fromUtf8(process.readAllStandardError()).contains(QString::fromStdString(verdict.reason)), "the reason is printed");
        }
    }

    // 找不到授權（HALCONROOT 指到空資料夾，HALCON 找不到授權檔）：結束碼 2，訊息有 HALCON 的錯誤碼與原因，不當掉。
    void halconWithoutALicenseFailsClearly() {
        QTemporaryDir emptyRoot;
        QVERIFY(emptyRoot.isValid());
        QProcessEnvironment env = offscreen();
        env.insert(QStringLiteral("HALCONROOT"), emptyRoot.path());
        QProcess process;
        process.setProcessEnvironment(env);
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=halcon")});
        QVERIFY2(process.waitForFinished(30000), "the program must refuse to start, not keep running");
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
        const QString err = QString::fromUtf8(process.readAllStandardError());
        QVERIFY2(err.contains(QStringLiteral("HALCON has no valid license")), qPrintable(err));
        QVERIFY2(err.contains(QStringLiteral("Available back ends: fake")), qPrintable(err));
    }

    // 找不到 HALCON 的 DLL（PATH 上拿掉 MVTec 的資料夾）：halcon 登記成不可用；--backend=halcon 明確失敗，
    // 其他 back end 照常啟動（延遲載入：沒有 HALCON 的電腦也開得起來）。
    void withoutTheHalconRuntimeTheProgramStillStarts() {
        QProcessEnvironment env = offscreen();
        QStringList path = env.value(QStringLiteral("PATH")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
        path.removeIf([](const QString& p) { return p.contains(QStringLiteral("MVTec"), Qt::CaseInsensitive); });
        env.insert(QStringLiteral("PATH"), path.join(QLatin1Char(';')));

        QProcess halcon;
        halcon.setProcessEnvironment(env);
        halcon.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=halcon")});
        QVERIFY(halcon.waitForFinished(20000));
        QCOMPARE(halcon.exitStatus(), QProcess::NormalExit);
        QCOMPARE(halcon.exitCode(), 2);
        const QString err = QString::fromUtf8(halcon.readAllStandardError());
        QVERIFY2(err.contains(QStringLiteral("halconcpp.dll")), qPrintable(err));

        QProcess fake;
        fake.setProcessEnvironment(env);
        fake.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=fake")});
        QVERIFY(fake.waitForStarted(20000));
        const bool exited = fake.waitForFinished(2500);
        if (!exited) {
            fake.kill();
            fake.waitForFinished(10000);
        }
        QVERIFY2(!exited, qPrintable(QStringLiteral("fake must start without HALCON; stderr: ") + QString::fromUtf8(fake.readAllStandardError())));
    }
#endif

    void aBadThemeStillFailsWithExitCodeTwoAndAMessage() {
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--theme=neon")});
        QVERIFY(process.waitForFinished(20000));
        QCOMPARE(process.exitCode(), 2);
        QVERIFY(QString::fromUtf8(process.readAllStandardError()).contains(QStringLiteral("Unknown theme 'neon'")));
    }

    void availableBackendsStartAndKeepRunning_data() {
        QTest::addColumn<QStringList>("arguments");
        QTest::newRow("default") << QStringList{};
        QTest::newRow("--backend=fake") << QStringList{QStringLiteral("--backend=fake")};
#ifdef AOI_WITH_OPENCV
        QTest::newRow("--backend=opencv") << QStringList{QStringLiteral("--backend=opencv")};  // S07
#endif
    }
    void availableBackendsStartAndKeepRunning() {
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
