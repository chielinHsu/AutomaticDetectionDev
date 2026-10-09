// 命令列：AiInspTool.exe 的 --backend 參數（組裝根的行為，從外面用真的行程驗證）。
// 沒有編進程式的 back end 要「明確失敗」：結束碼 2、stderr 有原因與可用的 back end；預設與 fake 要能啟動。
// ADR-021：沒給 --backend 時用記住的那一個（使用者的設定檔；測試用 AOI_SETTINGS_DIR 指到暫存資料夾，不碰使用者的設定）。
#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QTemporaryDir>

#ifdef AOI_WITH_HALCON
#include "app/CompositionRoot.h"
#include "application/backend/SearchEngine.h"
#include "infrastructure/backend_halcon/HalconLicense.h"
#endif

namespace {

// 設定檔放在 settingsDir（空 ＝ 每次一個新的空資料夾）：程式不讀、不寫使用者真正的設定，前一次啟動記住的也不影響下一次。
QProcessEnvironment offscreen(const QString& settingsDir = QString()) {
    static QTemporaryDir root;
    static int launches = 0;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
    env.insert(QStringLiteral("QT_QPA_FONTDIR"), QStringLiteral(AOI_FONT_DIR));
    env.insert(QStringLiteral("AOI_SETTINGS_DIR"),
               settingsDir.isEmpty() ? root.filePath(QString::number(++launches)) : settingsDir);
    return env;
}

// 程式的設定檔（QSettings 的 INI、使用者範圍：<資料夾>/<組織>/<程式>.ini）。
QString settingsFile(const QTemporaryDir& dir) { return dir.filePath(QStringLiteral("AOI/AutoCadCreated.ini")); }

void remember(const QTemporaryDir& dir, const QString& backend) {
    QSettings settings(settingsFile(dir), QSettings::IniFormat);
    settings.setValue(QStringLiteral("backend/last"), backend);
    settings.sync();
}

QString remembered(const QTemporaryDir& dir) {
    return QSettings(settingsFile(dir), QSettings::IniFormat).value(QStringLiteral("backend/last")).toString();
}

// 程式一直在跑（視窗）時讀 stderr，直到出現 text 或逾時；回傳讀到的全部。
QString stderrUntil(QProcess& process, const QString& text, int timeoutMs = 15000) {
    QString err;
    QElapsedTimer clock;
    clock.start();
    while (!err.contains(text) && clock.elapsed() < timeoutMs && process.state() == QProcess::Running) {
        process.waitForReadyRead(200);
        err += QString::fromUtf8(process.readAllStandardError());
    }
    return err;
}

void stop(QProcess& process) {
    process.kill();
    process.waitForFinished(10000);
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
    // 2026-10-09：第一次失敗之後，授權回來就能用，不必重新啟動程式。「授權不在」用 HALCONROOT 指到空資料夾模擬（HALCON 找不到授權檔，
    // 錯誤 2036；實測 HALCON 在行程裡初始化失敗之後一直記得這個錯誤，改回 HALCONROOT 也一樣）。組裝根先用另一個行程檢查，所以這個行程的
    // HALCON 沒被碰過；改回 HALCONROOT（像插上 dongle）之後再建立就成功。要在這個行程碰 HALCON 之前跑：放在 HALCON 的測試最前面。
    void aHalconFailureDoesNotStickOnceTheLicenseIsBack() {
        if (aoi::infra::halconProbedInProcess()) QSKIP("this process has touched HALCON already; this test has to run before the others");
        const aoi::app::HalconLicenseCheck now = aoi::app::checkHalconLicenseInSeparateProcess();
        QVERIFY2(now.ran, "AiInspTool.exe --probe-halcon-license has to run next to this test");
        if (!now.usable) QSKIP(qPrintable(QStringLiteral("HALCON is not usable here: ") + QString::fromStdString(now.reason)));
        const QByteArray realRoot = qgetenv("HALCONROOT");
        QTemporaryDir empty;
        QVERIFY(empty.isValid());
        qputenv("HALCONROOT", QDir::toNativeSeparators(empty.path()).toLocal8Bit());
        const aoi::BackendRegistry registry = aoi::app::makeBackendRegistry();
        const aoi::Result<aoi::SearchEnginePtr> unplugged = aoi::makeSearchEngine(registry, "halcon");
        qputenv("HALCONROOT", realRoot);  // 先還原（下面的 QVERIFY 失敗時會直接返回）
        QVERIFY(!unplugged.ok());
        QVERIFY2(unplugged.error().message.find("2036") != std::string::npos, unplugged.error().message.c_str());
        QVERIFY2(!aoi::infra::halconProbedInProcess(), "this process must not touch HALCON while the license is missing");
        const aoi::Result<aoi::SearchEnginePtr> pluggedIn = aoi::makeSearchEngine(registry, "halcon");
        QVERIFY2(pluggedIn.ok(), pluggedIn.ok() ? "" : pluggedIn.error().message.c_str());
        QVERIFY(aoi::infra::halconProbedInProcess());
    }

    // --probe-halcon-license：組裝根用的內部選項（不開視窗），第一行 usable／unusable，第二行原因。
    void theLicenseProbeOptionAnswersWithoutAWindow() {
        QProcess process;
        process.setProcessEnvironment(offscreen());
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--probe-halcon-license")});
        QVERIFY(process.waitForFinished(30000));
        const QStringList lines = QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        QVERIFY(!lines.isEmpty());
        const QString verdict = lines.first().trimmed();
        QVERIFY2(verdict == QLatin1String("usable") || verdict == QLatin1String("unusable"), qPrintable(verdict));
        QCOMPARE(process.exitCode(), verdict == QLatin1String("usable") ? 0 : 1);
        if (verdict == QLatin1String("unusable")) QVERIFY(lines.size() >= 2);  // 有原因
    }

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

    // 授權檔寫 SESSIONS=1（MVTec_HALCON 那一行）：一個行程初始化著 HALCON（例如我們的程式換到別的 back end 之後，HALCON 仍在行程裡），
    // 別的程式還拿不拿得到授權？（2026-10-09）這個測試行程自己先初始化 HALCON，再開兩個 --backend=halcon 的程式同時跑：
    // 三個行程都要拿到授權（拿不到的程式會以結束碼 2 結束）。這台機器不能用 HALCON 時跳過並說原因。
    void severalProgramsCanUseHalconAtTheSameTime() {
        const aoi::infra::HalconLicenseVerdict verdict = aoi::infra::judgeHalconLicense(aoi::infra::halconLicense());
        if (!verdict.usable) QSKIP(qPrintable(QStringLiteral("HALCON is not usable here: ") + QString::fromStdString(verdict.reason)));
        QProcess first;
        QProcess second;
        first.setProcessEnvironment(offscreen());
        second.setProcessEnvironment(offscreen());
        first.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=halcon")});
        QVERIFY(first.waitForStarted(20000));
        const bool firstExited = first.waitForFinished(6000);  // 授權檢查約 1.5 秒：之後它就初始化著 HALCON
        second.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=halcon")});
        QVERIFY(second.waitForStarted(20000));
        const bool secondExited = second.waitForFinished(6000);
        const bool firstExitedLater = !firstExited && first.state() != QProcess::Running;
        for (QProcess* p : {&first, &second}) {
            if (p->state() == QProcess::Running) {
                p->kill();
                p->waitForFinished(10000);
            }
        }
        const QString err1 = QString::fromUtf8(first.readAllStandardError());
        const QString err2 = QString::fromUtf8(second.readAllStandardError());
        QVERIFY2(!firstExited && !firstExitedLater, qPrintable(QStringLiteral("the first program lost or never got HALCON: ") + err1));
        QVERIFY2(!secondExited, qPrintable(QStringLiteral("the second program could not get HALCON while the first one ran: ") + err2));
        // 這台機器（用 runtime 授權、SDK 的 dongle 沒插）兩個都印出授權的提醒：只有拿到授權的程式才會印。
        for (const aoi::infra::HalconLicenseNotice& n : verdict.notices) {
            QVERIFY2(err1.contains(QString::fromStdString(n.missingId)), qPrintable(err1));
            QVERIFY2(err2.contains(QString::fromStdString(n.missingId)), qPrintable(err2));
        }
        // 這個測試行程自己也還拿著
        QVERIFY(aoi::infra::judgeHalconLicense(aoi::infra::probeHalconLicense()).usable);
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

    // ---- ADR-021：記住上次用的 back end（使用者 2026-10-08 決定）----

    // 命令列指定、而且啟動成功的，記下來（下次沒給 --backend 時用它）。
    void anExplicitBackendIsRemembered() {
        QTemporaryDir settings;
        QVERIFY(settings.isValid());
        QProcess process;
        process.setProcessEnvironment(offscreen(settings.path()));
        process.start(QStringLiteral(AOI_EXE_PATH), {QStringLiteral("--backend=fake")});
        QVERIFY(process.waitForStarted(20000));
        QVERIFY2(!process.waitForFinished(2500), "the program keeps running");
        stop(process);
        QCOMPARE(remembered(settings), QStringLiteral("fake"));
    }

    // 記住的不能用（例如 dongle 沒插、或 id 過期）：不結束程式，留在 fake，原因寫到 stderr；記住的值不被改掉。
    void aRememberedBackendThatCannotStartLeavesTheProgramRunningOnFake() {
        QTemporaryDir settings;
        QVERIFY(settings.isValid());
        remember(settings, QStringLiteral("turbo"));
        QProcess process;
        process.setProcessEnvironment(offscreen(settings.path()));
        process.start(QStringLiteral(AOI_EXE_PATH), QStringList{});
        QVERIFY(process.waitForStarted(20000));
        const QString err = stderrUntil(process, QStringLiteral("Still using 'fake'"));
        const bool running = process.state() == QProcess::Running;
        stop(process);
        QVERIFY2(running, qPrintable(QStringLiteral("the program must keep running; stderr: ") + err));
        QVERIFY2(err.contains(QStringLiteral("Switching to the remembered back end 'turbo'")), qPrintable(err));
        QVERIFY2(err.contains(QStringLiteral("Cannot switch to the 'turbo' back end")), qPrintable(err));
        QCOMPARE(remembered(settings), QStringLiteral("turbo"));
    }

#ifdef AOI_WITH_OPENCV
    // 沒給 --backend：視窗出現之後在背景換成記住的那一個。
    void theRememberedBackendIsUsedWhenNoneIsGiven() {
        QTemporaryDir settings;
        QVERIFY(settings.isValid());
        remember(settings, QStringLiteral("opencv"));
        QProcess process;
        process.setProcessEnvironment(offscreen(settings.path()));
        process.start(QStringLiteral(AOI_EXE_PATH), QStringList{});
        QVERIFY(process.waitForStarted(20000));
        const QString err = stderrUntil(process, QStringLiteral("Switched to the 'opencv' back end."));
        const bool running = process.state() == QProcess::Running;
        stop(process);
        QVERIFY2(running, qPrintable(err));
        QVERIFY2(err.contains(QStringLiteral("Switched to the 'opencv' back end.")), qPrintable(err));
        QCOMPARE(remembered(settings), QStringLiteral("opencv"));
    }
#endif

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
