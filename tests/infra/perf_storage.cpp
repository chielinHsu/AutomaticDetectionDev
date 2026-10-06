// S06 的效能量測（label perf；計畫 docs/plan/S06-image-storage.md 的「效能檢查」）：
//   IMG-<來源>-<格式>-<大小>   影像載入（經 IImageSource：讀檔＋解碼＋轉成正規格式），5 MP 與 25 MP；附每次載入的複製次數
//   CONV-*                     各轉換的成本與複製次數（零複製的包裝、要複製的轉換）
//   RCP-SAVE／LOAD-<n>         配方存檔、讀檔，n ＝ 500、5 000 個元件；附檔案大小
//   RCP-SAVER-*                背景存檔：呼叫端（UI）花的時間，與交出到寫完的時間
//   LIB-*                      樣本庫 1 000 筆：存一筆（整份重寫）、重新開啟、找別名
// 這些都沒有預算（perf/budgets.json 沒有 S06 的指標；U4 由 perf_ui_baseline 量），只記錄、不當失敗條件。
// 方法（docs/perf-test-plan.md）：熱機後連續量，回報中位數與 p95；25 MP 的載入太慢，次數較少（見每列的 n）。
// 結果寫到 perf/results/storage_<時間>.json（含機器指紋）。
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <QtTest>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <vector>

#include "../ui/MeasurementHygiene.h"
#include "InfraTestSupport.h"
#include "RecipeTestSupport.h"
#include "application/document/RecipeSaver.h"
#include "infrastructure/image_qt/QtImageSource.h"
#include "infrastructure/image_qt/QtImages.h"
#include "infrastructure/storage/JsonLibraryStore.h"
#include "infrastructure/storage/JsonRecipeStore.h"
#include "infrastructure/threading/ThreadPoolExecutor.h"
#include "presentation/dev/FakeScene.h"
#include "synthetic_board.h"
#ifdef AOI_WITH_OPENCV
#include <fstream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"
#endif

using namespace aoi;
using namespace aoi::test;
using Clock = std::chrono::steady_clock;

namespace {

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const auto rank = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
    return v[std::min(v.size(), std::max<std::size_t>(1, rank)) - 1];
}

QString runCommand(const QString& program, const QStringList& args) {
    QProcess p;
    p.setWorkingDirectory(QStringLiteral(AOI_SOURCE_DIR));
    p.start(program, args);
    if (!p.waitForFinished(5000)) return {};
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

QJsonObject machineFingerprint(const QString& hygiene) {
    QJsonObject m;
    QSettings cpu(QStringLiteral("HKEY_LOCAL_MACHINE\\HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0"), QSettings::NativeFormat);
    m["cpu"] = cpu.value(QStringLiteral("ProcessorNameString"), QStringLiteral("unknown")).toString().trimmed();
    m["logicalCores"] = QThread::idealThreadCount();
#ifdef Q_OS_WIN
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) m["ramGB"] = std::round(static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024 * 1024) * 10) / 10;
#endif
    m["os"] = QSysInfo::prettyProductName() + QStringLiteral(" (kernel ") + QSysInfo::kernelVersion() + QLatin1Char(')');
    m["qt"] = QString::fromLatin1(qVersion());
#ifdef AOI_WITH_OPENCV
    m["opencv"] = QStringLiteral(CV_VERSION);
#endif
#ifdef _MSC_FULL_VER
    m["compiler"] = QStringLiteral("MSVC %1").arg(_MSC_FULL_VER);
#endif
    m["buildType"] = QStringLiteral(AOI_BUILD_TYPE);
    const QString head = runCommand(QStringLiteral("git"), {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    const QString dirty = runCommand(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain")});
    m["commit"] = head.isEmpty() ? QStringLiteral("unknown") : head + (dirty.isEmpty() ? QString() : QStringLiteral("+uncommitted"));
    m["machineName"] = QSysInfo::machineHostName();
    m["measurementHygiene"] = hygiene;
    return m;
}

volatile double g_sink = 0.0;

// 很快的呼叫（零複製的包裝）：每個樣本是 calls 次呼叫的平均，單位 µs。
std::vector<double> timeMicros(const std::function<void()>& fn, int calls, int runs) {
    for (int k = 0; k < calls; ++k) fn();
    std::vector<double> out;
    for (int r = 0; r < runs; ++r) {
        const auto t0 = Clock::now();
        for (int k = 0; k < calls; ++k) fn();
        out.push_back(std::chrono::duration<double, std::micro>(Clock::now() - t0).count() / calls);
    }
    return out;
}

std::vector<double> timeRuns(const std::function<void()>& fn, int warmup, int runs) {
    for (int k = 0; k < warmup; ++k) fn();
    std::vector<double> out;
    out.reserve(static_cast<std::size_t>(runs));
    for (int k = 0; k < runs; ++k) {
        const auto t0 = Clock::now();
        fn();
        out.push_back(std::chrono::duration<double, std::milli>(Clock::now() - t0).count());
    }
    return out;
}

struct Size {
    const char* tag;
    int w, h;
};
constexpr Size k5MP{"5MP", 2592, 1944};
constexpr Size k25MP{"25MP", 5792, 4320};  // 25.0 MP

// 灰階的合成板子（元件陣列＋雜訊＋模糊），填滿整張影像。
ImageView grayBoard(const Size& s) {
    SyntheticBoardSpec spec;
    spec.width = s.w;
    spec.height = s.h;
    spec.cols = (s.w - 80) / 60;
    spec.rows = (s.h - 80) / 40;
    spec.pitchXPx = 60.0;
    spec.pitchYPx = 40.0;
    spec.originXPx = 50.0;
    spec.originYPx = 40.0;
    spec.bodyWidthPx = 36.0;
    spec.bodyHeightPx = 18.0;
    spec.noiseAmplitude = 6;
    spec.blurRadius = 1;
    spec.missingFraction = 0.05;
    return makeSyntheticBoard(spec).image;
}

QImage gray16Image(const ImageView& gray8) {
    QImage img(gray8.width, gray8.height, QImage::Format_Grayscale16);
    for (int y = 0; y < gray8.height; ++y) {
        auto* row = reinterpret_cast<quint16*>(img.scanLine(y));
        const std::uint8_t* src = gray8.data + static_cast<std::size_t>(y) * gray8.strideBytes;
        for (int x = 0; x < gray8.width; ++x) row[x] = static_cast<quint16>(src[x] * 257 + (x & 63));  // 用到低位元
    }
    return img;
}

}  // namespace

class PerfStorage : public QObject {
    Q_OBJECT

    struct Metric {
        QString id, label, unit;
        std::vector<double> samples;
        QString note;
    };
    std::vector<Metric> metrics_;
    QString hygiene_;
    QDir images_;

    void add(const QString& id, const QString& label, const QString& unit, std::vector<double> samples, const QString& note = {}) {
        qInfo().noquote() << QStringLiteral("%1  median %2  p95 %3 %4  (n=%5) %6")
                                 .arg(id, -30)
                                 .arg(percentile(samples, 0.5), 0, 'f', 3)
                                 .arg(percentile(samples, 0.95), 0, 'f', 3)
                                 .arg(unit)
                                 .arg(samples.size())
                                 .arg(note);
        metrics_.push_back({id, label, unit, std::move(samples), note});
    }

    // 經 source 載入 path：熱機、量時間、記一次載入的複製次數。
    void measureLoad(IImageSource& source, const QString& sourceTag, const QString& path, const QString& formatTag, const Size& size) {
        const std::string p = utf8(path);
        const bool big = size.w * size.h > 10'000'000;
        const CopyWatch watch;
        const Result<ImageView> first = source.load(p);
        QVERIFY2(first.ok(), first.ok() ? "" : first.error().message.c_str());
        const std::uint64_t copies = watch.copies();
        const QString format = QString::fromLatin1(toString(first.value().format).data());
        const std::vector<double> ms = timeRuns([&] { g_sink = g_sink + source.load(p).value().width; }, big ? 1 : 3, big ? 5 : 15);
        add(QStringLiteral("IMG-%1-%2-%3").arg(sourceTag, formatTag, QString::fromLatin1(size.tag)),
            QStringLiteral("load %1 %2 through the %3 image source (read + decode + canonical format)").arg(QString::fromLatin1(size.tag), formatTag, sourceTag),
            QStringLiteral("ms"), ms,
            QStringLiteral("file %1 MB; gives %2; %3 copies per load").arg(static_cast<double>(QFileInfo(path).size()) / 1e6, 0, 'f', 1).arg(format).arg(copies));
    }

    QString imagePath(const QString& name) const { return images_.filePath(name); }

private slots:
    void initTestCase() {
        hygiene_ = applyMeasurementHygiene();
        images_ = QDir(QDir::current().filePath(QStringLiteral("perf-images-s06")));
        QVERIFY(images_.mkpath(QStringLiteral(".")));
        // 測試影像只產生一次（之後的執行重用），量的是解碼，不是寫檔。
        for (const Size& s : {k5MP, k25MP}) {
            const QString tag = QString::fromLatin1(s.tag);
            const aoi::dev::FakeBoard color = aoi::dev::makeFakeBoard(s.w, s.h);
            const ImageView gray = grayBoard(s);
            QVERIFY(gray.valid());
            const QImage colorImage = color.toQImage();
            const QImage grayImage = infra::wrapAsQImage(gray);
            struct File {
                QString name;
                const QImage* image;
                const char* format;
            };
            const QImage g16 = gray16Image(gray);
            for (const File& f : {File{QStringLiteral("color-%1.bmp").arg(tag), &colorImage, "BMP"},
                                  File{QStringLiteral("color-%1.png").arg(tag), &colorImage, "PNG"},
                                  File{QStringLiteral("color-%1.jpg").arg(tag), &colorImage, "JPG"},
                                  File{QStringLiteral("gray-%1.png").arg(tag), &grayImage, "PNG"},
                                  File{QStringLiteral("gray16-%1.png").arg(tag), &g16, "PNG"}}) {
                if (!QFile::exists(imagePath(f.name))) QVERIFY(f.image->save(imagePath(f.name), f.format));
            }
#ifdef AOI_WITH_OPENCV
            const auto writeTiff = [&](const QString& name, const cv::Mat& m) {
                if (QFile::exists(imagePath(name))) return true;
                std::vector<uchar> bytes;
                if (!cv::imencode(".tif", m, bytes)) return false;
                return writeBytes(imagePath(name), QByteArray(reinterpret_cast<const char*>(bytes.data()), static_cast<qsizetype>(bytes.size())));
            };
            const cv::Mat colorMat(s.h, s.w, CV_8UC4, const_cast<std::uint8_t*>(color.view().data), static_cast<std::size_t>(color.view().strideBytes));
            cv::Mat bgr;
            cv::cvtColor(colorMat, bgr, cv::COLOR_BGRA2BGR);
            QVERIFY(writeTiff(QStringLiteral("color-%1.tif").arg(tag), bgr));
            QVERIFY(writeTiff(QStringLiteral("gray-%1.tif").arg(tag), infra::borrowAsMat(gray).mat));
            const cv::Mat g16Mat(g16.height(), g16.width(), CV_16UC1, const_cast<uchar*>(g16.constBits()), static_cast<std::size_t>(g16.bytesPerLine()));
            QVERIFY(writeTiff(QStringLiteral("gray16-%1.tif").arg(tag), g16Mat));
#endif
        }
    }

    // ---- 影像載入 ----
    void imageLoading() {
        infra::QtImageSource qt;
        for (const Size& s : {k5MP, k25MP}) {
            const QString tag = QString::fromLatin1(s.tag);
            measureLoad(qt, QStringLiteral("qt"), imagePath(QStringLiteral("color-%1.bmp").arg(tag)), QStringLiteral("BMP"), s);
            measureLoad(qt, QStringLiteral("qt"), imagePath(QStringLiteral("color-%1.png").arg(tag)), QStringLiteral("PNG"), s);
            measureLoad(qt, QStringLiteral("qt"), imagePath(QStringLiteral("color-%1.jpg").arg(tag)), QStringLiteral("JPEG"), s);
            measureLoad(qt, QStringLiteral("qt"), imagePath(QStringLiteral("gray-%1.png").arg(tag)), QStringLiteral("grayPNG"), s);
            measureLoad(qt, QStringLiteral("qt"), imagePath(QStringLiteral("gray16-%1.png").arg(tag)), QStringLiteral("gray16PNG"), s);
        }
#ifdef AOI_WITH_OPENCV
        infra::OpenCvImageSource ocv;
        for (const Size& s : {k5MP, k25MP}) {
            const QString tag = QString::fromLatin1(s.tag);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("color-%1.bmp").arg(tag)), QStringLiteral("BMP"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("color-%1.png").arg(tag)), QStringLiteral("PNG"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("color-%1.jpg").arg(tag)), QStringLiteral("JPEG"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("gray-%1.png").arg(tag)), QStringLiteral("grayPNG"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("gray16-%1.png").arg(tag)), QStringLiteral("gray16PNG"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("color-%1.tif").arg(tag)), QStringLiteral("TIFF"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("gray-%1.tif").arg(tag)), QStringLiteral("grayTIFF"), s);
            measureLoad(ocv, QStringLiteral("opencv"), imagePath(QStringLiteral("gray16-%1.tif").arg(tag)), QStringLiteral("gray16TIFF"), s);
        }
#else
        qInfo("AOI_WITH_OPENCV is OFF: the OpenCV source and TIFF are not measured in this build");
#endif
    }

    // ---- 轉換：零複製的包裝與要複製的轉換（5 MP）----
    void conversions() {
        const aoi::dev::FakeBoard color = aoi::dev::makeFakeBoard(k5MP.w, k5MP.h);
        const QImage rgb32 = color.toQImage();
        {
            const CopyWatch w;
            const std::vector<double> us = timeMicros([&] { g_sink = g_sink + infra::fromQImage(rgb32).width; }, 1000, 30);
            add(QStringLiteral("CONV-qt-RGB32-wrap"), QStringLiteral("QImage RGB32 -> ImageView (zero copy), per call"), QStringLiteral("us"), us,
                QStringLiteral("%1 copies in %2 calls").arg(w.copies()).arg(31000));
        }
        {
            const ImageView v = color.view();
            const CopyWatch w;
            const std::vector<double> us = timeMicros([&] { g_sink = g_sink + infra::wrapAsQImage(v).width(); }, 1000, 30);
            add(QStringLiteral("CONV-qt-view-wrap"), QStringLiteral("ImageView Bgrx32 -> QImage (zero copy), per call"), QStringLiteral("us"), us,
                QStringLiteral("%1 copies in %2 calls").arg(w.copies()).arg(31000));
        }
        {
            QImage indexed = rgb32.convertToFormat(QImage::Format_Grayscale8).convertToFormat(QImage::Format_Indexed8);
            QList<QRgb> table;
            for (int i = 0; i < 256; ++i) table.append(qRgb(i, i, i));
            indexed.setColorTable(table);
            const CopyWatch w;
            const std::vector<double> ms = timeRuns([&] { g_sink = g_sink + infra::fromQImage(indexed).width; }, 3, 15);
            add(QStringLiteral("CONV-qt-Indexed8-to-Gray8"), QStringLiteral("QImage Indexed8 (gray table) -> Gray8 (one copy)"),
                QStringLiteral("ms"), ms, QStringLiteral("%1 copies in %2 calls").arg(w.copies()).arg(18));
        }
        {
            const QImage rgb888 = rgb32.convertToFormat(QImage::Format_RGB888);
            const CopyWatch w;
            const std::vector<double> ms = timeRuns([&] { g_sink = g_sink + infra::toCanonicalQImage(rgb888).width(); }, 3, 15);
            add(QStringLiteral("CONV-qt-RGB888-to-RGB32"), QStringLiteral("QImage RGB888 -> RGB32 (one copy, canonical format)"),
                QStringLiteral("ms"), ms, QStringLiteral("%1 copies in %2 calls").arg(w.copies()).arg(18));
        }
#ifdef AOI_WITH_OPENCV
        {
            cv::Mat bgr;
            cv::cvtColor(infra::borrowAsMat(color.view()).mat, bgr, cv::COLOR_BGRA2BGR);
            const std::vector<double> ms = timeRuns([&] {
                cv::Mat bgrx;
                cv::cvtColor(bgr, bgrx, cv::COLOR_BGR2BGRA);
                g_sink = g_sink + bgrx.cols;
            }, 3, 15);
            add(QStringLiteral("CONV-cv-BGR-to-BGRx"), QStringLiteral("cv::Mat 8UC3 -> 8UC4 (the copy the OpenCV source makes for color files)"),
                QStringLiteral("ms"), ms);
            const CopyWatch w;
            const std::vector<double> wrap = timeMicros([&] {
                g_sink = g_sink + infra::fromMat(bgr).width + infra::borrowAsMat(color.view()).mat.cols;
            }, 1000, 30);
            add(QStringLiteral("CONV-cv-wrap"), QStringLiteral("cv::Mat -> ImageView plus ImageView -> cv::Mat (zero copy), per pair"),
                QStringLiteral("us"), wrap, QStringLiteral("%1 copies in %2 calls").arg(w.copies()).arg(31000));
        }
#endif
    }

    // ---- 配方 ----
    void recipes() {
        QTemporaryDir dir = makeTempDir();
        infra::JsonRecipeStore store;
        for (const int n : {500, 5000}) {
            const Recipe recipe = richRecipe(n);
            const std::string path = utf8(dir.filePath(QStringLiteral("r%1.aoi.json").arg(n)));
            const std::vector<double> save = timeRuns([&] { QVERIFY(store.save(recipe, path).ok()); }, 5, 30);
            const double mb = static_cast<double>(QFileInfo(QString::fromStdString(path)).size()) / 1e6;
            add(QStringLiteral("RCP-SAVE-%1").arg(n), QStringLiteral("save a recipe with %1 components (serialize + atomic write)").arg(n),
                QStringLiteral("ms"), save, QStringLiteral("file %1 MB").arg(mb, 0, 'f', 2));
            const std::vector<double> load = timeRuns([&] { g_sink = g_sink + static_cast<double>(store.load(path).value().board.components.size()); }, 5, 30);
            add(QStringLiteral("RCP-LOAD-%1").arg(n), QStringLiteral("load a recipe with %1 components (read + parse + check)").arg(n),
                QStringLiteral("ms"), load, QStringLiteral("file %1 MB").arg(mb, 0, 'f', 2));
        }

        // 背景存檔：呼叫端（UI 執行緒）只付「取快照（複製 Recipe）＋交出」的時間；寫檔在 worker 上。
        const Recipe recipe = richRecipe(5000);
        infra::ThreadPoolExecutor executor(2);
        RecipeSaver saver(store, executor);
        const std::string path = utf8(dir.filePath(QStringLiteral("bg.aoi.json")));
        std::vector<double> call, done;
        for (int k = 0; k < 25; ++k) {
            std::mutex m;
            std::condition_variable cv;
            bool finished = false;
            const auto t0 = Clock::now();
            Recipe snapshot = recipe;  // UI 取快照
            saver.save(std::move(snapshot), path, [&](const RecipeSaver::Outcome&) {
                std::lock_guard<std::mutex> lock(m);
                finished = true;
                cv.notify_all();
            });
            const auto t1 = Clock::now();
            std::unique_lock<std::mutex> lock(m);
            cv.wait(lock, [&] { return finished; });
            const auto t2 = Clock::now();
            if (k >= 5) {
                call.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
                done.push_back(std::chrono::duration<double, std::milli>(t2 - t0).count());
            }
        }
        add(QStringLiteral("RCP-SAVER-CALL-5000"), QStringLiteral("background save, time on the caller thread (snapshot copy + hand over)"),
            QStringLiteral("ms"), call);
        add(QStringLiteral("RCP-SAVER-DONE-5000"), QStringLiteral("background save, hand over to written"), QStringLiteral("ms"), done);
    }

    // ---- 樣本庫 ----
    void library() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("library.json")));
        {
            infra::JsonLibraryStore store(path);
            for (int i = 0; i < 1000; ++i) {
                ComponentType t = richType("x", 1 + i % 5);
                t.partNumber = "PN-" + std::to_string(i);
                t.aliases = {"ALIAS-" + std::to_string(i), "SOP" + std::to_string(i % 40)};
                QVERIFY(store.save(t).ok());
            }
        }
        const double mb = static_cast<double>(QFileInfo(QString::fromStdString(path)).size()) / 1e6;
        infra::JsonLibraryStore store(path);
        ComponentType edited = *store.find("PN-500").value();
        const std::vector<double> save = timeRuns([&] {
            edited.name += "x";
            QVERIFY(store.save(edited).ok());
        }, 3, 15);
        add(QStringLiteral("LIB-SAVE-1000"), QStringLiteral("save one entry into a 1000-entry library (the whole file is rewritten)"),
            QStringLiteral("ms"), save, QStringLiteral("file %1 MB").arg(mb, 0, 'f', 2));
        const std::vector<double> open = timeRuns([&] {
            infra::JsonLibraryStore fresh(path);
            g_sink = g_sink + static_cast<double>(fresh.loadAll().value().size());
        }, 3, 15);
        add(QStringLiteral("LIB-OPEN-1000"), QStringLiteral("open a 1000-entry library (read + parse + index)"), QStringLiteral("ms"), open);
        const std::vector<double> find = timeRuns([&] { g_sink = g_sink + (store.find("alias-999").value() ? 1.0 : 0.0); }, 5, 30);
        add(QStringLiteral("LIB-FIND-1000"), QStringLiteral("find by alias in a 1000-entry library (after it is loaded)"), QStringLiteral("ms"), find);
    }

    void cleanupTestCase() {
        QJsonArray rows;
        for (const Metric& m : metrics_) {
            QJsonObject o;
            o["id"] = m.id;
            o["label"] = m.label;
            o["unit"] = m.unit;
            o["n"] = static_cast<int>(m.samples.size());
            o["median"] = percentile(m.samples, 0.5);
            o["p95"] = percentile(m.samples, 0.95);
            o["max"] = m.samples.empty() ? 0.0 : *std::max_element(m.samples.begin(), m.samples.end());
            if (!m.note.isEmpty()) o["note"] = m.note;
            rows.append(o);
        }
        QJsonObject root;
        root["schema"] = 1;
        root["step"] = "S06";
        root["note"] = "Image loading, conversions, recipe and library storage. No budget covers these; U4 is measured by perf_ui_baseline. Numbers compare only on this machine.";
        root["machine"] = machineFingerprint(hygiene_);
        root["metrics"] = rows;
        const QString dir = QStringLiteral(AOI_SOURCE_DIR "/perf/results");
        QDir().mkpath(dir);
        const QString path = dir + QStringLiteral("/storage_") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")) +
#ifdef AOI_WITH_OPENCV
                             QStringLiteral("-opencv") +
#endif
                             QStringLiteral(".json");
        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(root).toJson());
            qInfo().noquote() << "wrote" << path;
        }
    }
};

QTEST_MAIN(PerfStorage)
#include "perf_storage.moc"
