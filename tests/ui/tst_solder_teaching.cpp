#include <QAction>
#include <QApplication>
#include <QDoubleSpinBox>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTest>
#include <algorithm>
#include <chrono>
#include <thread>

#include "app/CompositionRoot.h"
#include "application/interaction/SolderTeachingSession.h"
#include "infrastructure/storage/JsonRecipeStore.h"
#include "infrastructure/threading/QtThreadPoolExecutor.h"
#include "presentation/canvas/ImageCanvas.h"
#include "presentation/i18n/Translations.h"
#include "presentation/solder/SolderTeachingWindow.h"
#include "presentation/theme/ThemeManager.h"

using namespace aoi;
using namespace aoi::ui;

namespace {
QString makeImage(const QString& path) {
    QImage image(180,120,QImage::Format_RGB32); image.fill(Qt::black);
    QPainter p(&image); p.fillRect(QRect(20,20,12,10),Qt::white); p.fillRect(QRect(60,20,14,12),Qt::white);
    p.fillRect(QRect(100,60,16,12),Qt::white); p.fillRect(QRect(150,80,1,1),Qt::white); p.end();
    return image.save(path) ? path : QString();
}
QString makeSampleBoard(const QString& path) {
    QImage image(220,150,QImage::Format_RGB32); image.fill(Qt::black);
    QPainter p(&image);
    for (const QPoint origin : {QPoint(20,20),QPoint(90,20),QPoint(20,90)}) {
        p.fillRect(QRect(origin,QSize(16,12)),Qt::white);
        p.fillRect(QRect(origin+QPoint(8,0),QSize(8,6)),Qt::black);
    }
    // 同亮度但形狀不同的干擾物、較大的亮塊與雜點。
    p.fillRect(QRect(90,90,16,12),Qt::white);
    p.fillRect(QRect(155,25,30,24),Qt::white);
    p.fillRect(QRect(180,110,1,1),Qt::white);
    p.end();
    return image.save(path) ? path : QString();
}
class LateSampleSearcher final : public ISocketSampleSearcher {
public:
    Result<std::vector<SocketSampleMatch>> search(const SocketSampleRequest& request,const CancelToken&) const override {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::vector<SocketSampleMatch>{{request.samples.front(),0.99}};
    }
};
class ForbiddenSampleSearcher final : public ISocketSampleSearcher {
public:
    mutable int calls=0;
    Result<std::vector<SocketSampleMatch>> search(const SocketSampleRequest&,const CancelToken&) const override {
        ++calls; return makeError(ErrorCode::Internal,"CAD region mode must not call NCC sample search.");
    }
};
class LateDetector final : public ISolderRegionDetector {
public:
    Result<std::vector<Rect>> detect(const ImageView&,const Rect&,const SolderDetectionOptions&,const CancelToken&) const override {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        return std::vector<Rect>{{20,20,12,10}};
    }
};

QString makeSocketStructureBoard(const QString& path,bool pads=true) {
    QImage image(480,250,QImage::Format_RGB32); image.fill(QColor(30,30,30));
    QPainter p(&image);
    for (int row=0;row<3;++row) {
        const int y=50+row*60;
        for (int x : {100,250}) {
            p.fillRect(QRect(x,y,20,12),QColor(120+row*20,120+row*20,120+row*20));
            p.fillRect(QRect(x+2,y+2,10+row*2,7),Qt::white);
        }
        if (pads) {
            p.fillRect(QRect(35,y-9,48,30),QColor(240,240,240));
            p.fillRect(QRect(282,y-9,48,30),QColor(240,240,240));
        }
    }
    p.fillRect(QRect(380,60,20,12),Qt::white); // 同尺寸亮物件，但沒有 Socket 鄰接結構。
    p.end(); return image.save(path)?path:QString();
}
}

class SolderTeachingTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { QVERIFY(Translations::install(QStringLiteral("zh_TW"))); }

    void regionCadRequiresNoSamplesAndRejectsUnrelatedBrightObjects() {
        auto generator=app::makeSocketCadGenerator(); if (!generator) QSKIP("OpenCV disabled.");
        QTemporaryDir dir; auto images=app::makeImageSource();
        auto image=images->load(makeSocketStructureBoard(dir.filePath(QStringLiteral("socket-structure.png"))).toStdString()); QVERIFY(image);
        SocketCadRequest request{image.value(),{0,0,480,250},{},true};
        auto cad=generator->generate(request,CancelToken{});
        QVERIFY2(cad,qPrintable(QString::fromStdString(cad?"":cad.error().message)));
        QCOMPARE(cad.value().placements.size(),std::size_t(6));
        QCOMPARE(cad.value().profiles.size(),std::size_t(2));
        for (const auto& profile : cad.value().profiles) { QCOMPARE(profile.bodyPx.w,24.0); QCOMPARE(profile.bodyPx.h,16.0); }
        for (const auto& c : cad.value().placements) QVERIFY(c.pose.x<350);
        request.region={95,45,30,25};
        auto one=generator->generate(request,CancelToken{}); QVERIFY(one); QCOMPARE(one.value().placements.size(),std::size_t(1));
        request.region={0,0,480,250}; request.frameSize={30,18};
        auto specified=generator->generate(request,CancelToken{}); QVERIFY(specified);
        for (const auto& profile : specified.value().profiles) { QCOMPARE(profile.bodyPx.w,30.0); QCOMPARE(profile.bodyPx.h,18.0); }
        auto noPads=images->load(makeSocketStructureBoard(dir.filePath(QStringLiteral("not-socket.png")),false).toStdString()); QVERIFY(noPads);
        request.image=noPads.value(); auto rejected=generator->generate(request,CancelToken{}); QVERIFY(rejected); QVERIFY(rejected.value().placements.empty());
    }

    void realRegionCadProbe() {
        const QString path=QString::fromLocal8Bit(qgetenv("AOI_SOCKET_IMAGE")); if (path.isEmpty()) QSKIP("Real Socket image not set.");
        auto generator=app::makeSocketCadGenerator(); if (!generator) QSKIP("OpenCV disabled.");
        auto images=app::makeImageSource(); auto image=images->load(path.toStdString()); QVERIFY(image);
        for (const Rect region : {Rect{1100,120,900,550},Rect{0,0,static_cast<double>(image.value().width),static_cast<double>(image.value().height)}}) {
            auto cad=generator->generate({image.value(),region,{},true},CancelToken{}); QVERIFY(cad);
            qInfo("Region CAD: %zu placements, %zu profiles, evaluated %zu",cad.value().placements.size(),cad.value().profiles.size(),cad.value().evaluated);
            for (const auto& p : cad.value().profiles) qInfo("CAD profile %s %.1fx%.1f",p.id.c_str(),p.bodyPx.w,p.bodyPx.h);
            QVERIFY(!cad.value().placements.empty());
        }
    }

    void regionCadUiHasNoTeachingAndPersistsSharedProfiles() {
        auto generator=app::makeSocketCadGenerator(); if (!generator) QSKIP("OpenCV disabled.");
        QTemporaryDir dir; auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        auto forbidden=std::make_shared<ForbiddenSampleSearcher>();
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.setSampleSearcher(forbidden); w.setCadGenerator(generator); w.show();
        QVERIFY(w.openImage(makeSocketStructureBoard(dir.filePath(QStringLiteral("structure.png")))));
        w.canvas()->fit(); QCoreApplication::processEvents();
        QSignalSpy done(&w,&SolderTeachingWindow::detectionFinished);
        const auto a=w.canvas()->mapFromScene(QPointF(1,1)),b=w.canvas()->mapFromScene(QPointF(479,249));
        QTest::mousePress(w.canvas()->viewport(),Qt::LeftButton,Qt::NoModifier,a);
        QTest::mouseMove(w.canvas()->viewport(),b);
        QTest::mouseRelease(w.canvas()->viewport(),Qt::LeftButton,Qt::NoModifier,b);
        QTRY_COMPARE_WITH_TIMEOUT(done.size(),qsizetype(1),10000); QVERIFY(done.front().front().toBool());
        QCOMPARE(forbidden->calls,0); QVERIFY(w.session().samples().empty());
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(6));
        QCOMPARE(w.session().recipe().board.types.size(),std::size_t(2));
        for (const auto& t : w.session().recipe().board.types) {
            QCOMPARE(t.groups.size(),std::size_t(2)); QVERIFY(t.groups[1].kind==BoxGroupKind::Window);
        }
        const auto first=w.session().recipe().board.components.front();
        int other=0; for (const auto& c : w.session().recipe().board.components) if (c.id!=first.id && c.typeId==first.typeId) other=c.id;
        QVERIFY(other>0); const auto old=*w.session().box(first.id);
        w.onComponentResized(first.id,QRectF(old.x,old.y,old.w+6,old.h));
        QCOMPARE(w.session().box(first.id)->w,30.0); QCOMPARE(w.session().box(other)->w,24.0);
        w.searchWholeImage(); QTRY_COMPARE_WITH_TIMEOUT(done.size(),qsizetype(2),10000);
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(6));
        QCOMPARE(w.session().box(first.id)->w,30.0); // 人工修改保留，重搜不重疊加框。
        QCOMPARE(w.session().cadFrameSize()->w,24.0); // 單框複本不能變成全文件的新規格。
        QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved); const auto path=dir.filePath(QStringLiteral("CAD配方.json"));
        w.saveRecipe(path); QTRY_COMPARE(saved.size(),qsizetype(1)); QVERIFY(saved.front().front().toBool());
        QVERIFY(w.openRecipe(path)); QCOMPARE(w.session().recipe().board.components.size(),std::size_t(6));
        QCOMPARE(w.session().box(first.id)->w,30.0); QCOMPARE(forbidden->calls,0);
    }

    void realRegionCadUi() {
        const QString path=QString::fromLocal8Bit(qgetenv("AOI_SOCKET_IMAGE")); if (path.isEmpty()) QSKIP("Real image not set.");
        auto generator=app::makeSocketCadGenerator(); if (!generator) QSKIP("OpenCV disabled.");
        auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.setCadGenerator(generator); w.show(); QVERIFY(w.openImage(path));
        QSignalSpy done(&w,&SolderTeachingWindow::detectionFinished);
        w.onRegionDrawn(QRectF(1100,120,900,550),DraftMode::Region);
        QTRY_COMPARE_WITH_TIMEOUT(done.size(),qsizetype(1),15000); QVERIFY(done.front().front().toBool());
        QVERIFY(w.session().samples().empty());
        const auto spec=w.session().cadFrameSize(); QVERIFY(spec);
        const auto smallCount=w.session().recipe().board.components.size(); QVERIFY(smallCount>10);
        w.searchWholeImage(); QTRY_COMPARE_WITH_TIMEOUT(done.size(),qsizetype(2),30000); QVERIFY(done.back().front().toBool());
        const auto& board=w.session().recipe().board;
        QVERIFY(board.components.size()>smallCount); QVERIFY(board.types.size()<=2);
        for (const auto& c : board.components) { QCOMPARE(w.session().box(c.id)->w,spec->w); QCOMPARE(w.session().box(c.id)->h,spec->h); }
        qInfo("Region CAD UI: %zu instances, %zu shared types, body %.1fx%.1f, zero samples",board.components.size(),board.types.size(),spec->w,spec->h);
        const auto screenshot=QString::fromLocal8Bit(qgetenv("AOI_REGION_CAD_SCREENSHOT"));
        if (!screenshot.isEmpty()) {
            QTest::qWait(100); QVERIFY(w.grab().save(screenshot));
            w.canvas()->zoomAt(QPointF(w.canvas()->viewport()->rect().center()),6.0);
            w.canvas()->centerOn(QPointF(1550,450)); QTest::qWait(100);
            QString detail=screenshot; detail.replace(QLatin1String(".png"),QLatin1String("-detail.png"));
            QVERIFY(w.grab().save(detail));
        }
        const auto savedCount=board.components.size();
        QTemporaryDir dir; QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved); const auto target=dir.filePath(QStringLiteral("region-cad.json"));
        w.saveRecipe(target); QTRY_COMPARE_WITH_TIMEOUT(saved.size(),qsizetype(1),10000); QVERIFY(saved.front().front().toBool());
        QVERIFY(w.openRecipe(target)); QCOMPARE(w.session().recipe().board.components.size(),savedCount);
    }

    void regionReplacementIndependentEditingAndHistory() {
        SolderTeachingSession s; s.openImage("socket.png",300,200,0);
        QVERIFY(s.addBox({220,20,12,10}));  // 手動框在選區外。
        QVERIFY(s.replaceRegion({0,0,200,150},{{20,20,12,10},{60,20,14,12}}));
        QCOMPARE(s.recipe().board.components.size(),std::size_t(3));
        const int first=s.recipe().board.components[1].id, second=s.recipe().board.components[2].id;
        QVERIFY(s.resize(first,{21,22,18,16})); QCOMPARE(s.box(second)->w,14.0);
        QVERIFY(s.move({first},{2,3})); QCOMPARE(s.box(first)->x,23.0);
        QVERIFY(!s.move({first},{-100,0})); QCOMPARE(s.box(first)->x,23.0);
        QVERIFY(s.undo()); QCOMPARE(s.box(first)->x,21.0); QVERIFY(s.redo()); QCOMPARE(s.box(first)->x,23.0);
        QVERIFY(!s.replaceRegion({0,0,200,150},{})); QCOMPARE(s.recipe().board.components.size(),std::size_t(3));
        QVERIFY(s.replaceRegion({0,0,200,150},{{20,20,12,10},{60,20,14,12}}));
        QCOMPARE(s.recipe().board.components.size(),std::size_t(3));
        QVERIFY(s.addBox({120,20,10,10}));
        QVERIFY(s.replaceRegion({0,0,200,150},{{120,20,10,10},{60,20,14,12}}));
        QCOMPARE(s.recipe().board.components.size(),std::size_t(3)); // 手動畫的同位置不疊自動框。
        s.markSaved(); QVERIFY(!s.dirty()); QVERIFY(s.undo()); QVERIFY(s.dirty());
        QVERIFY(s.redo()); QVERIFY(!s.dirty());
    }

    void boundedHistoryStillTracksSavedState() {
        SolderTeachingSession s; s.openImage("socket.png",300,200,0); QVERIFY(s.addBox({10,10,10,10}));
        s.markSaved();
        for (int i=0;i<70;++i) QVERIFY(s.move({1},{1,0}));
        int undone=0; while (s.undo()) ++undone;
        QCOMPARE(undone,64); QVERIFY(s.dirty());
        for (int i=0;i<64;++i) QVERIFY(s.redo());
        QCOMPARE(s.box(1)->x,80.0);
    }

    void failedLoadPreservesDocument() {
        SolderTeachingSession s; s.openImage("old.png",100,100,0); QVERIFY(s.addBox({10,10,20,20}));
        Recipe invalid=s.recipe(); invalid.board.components[0].pose.x=-100;
        QVERIFY(!s.load(invalid)); QCOMPARE(s.box(1)->x,10.0);
        invalid=s.recipe(); invalid.board.components[0].pose.angleDeg=45;
        QVERIFY(!s.load(invalid)); QCOMPARE(s.recipe().board.name,std::string("old.png"));
    }

    void detectorCoordinatesNoiseAndCancellation() {
        auto detector=app::makeSolderRegionDetector(); if (!detector) QSKIP("OpenCV disabled in this build.");
        QTemporaryDir dir; auto images=app::makeImageSource();
        auto image=images->load(makeImage(dir.filePath(QStringLiteral("socket.png"))).toStdString()); QVERIFY(image);
        SolderDetectionOptions o; o.threshold=128; o.padding=2;
        auto boxes=detector->detect(image.value(),{10,10,120,80},o,CancelToken{}); QVERIFY(boxes);
        QCOMPARE(boxes.value().size(),std::size_t(3)); QCOMPARE(boxes.value()[0].x,18.0); QCOMPARE(boxes.value()[0].w,16.0);
        CancelSource cancelled; cancelled.cancel();
        auto result=detector->detect(image.value(),{10,10,120,80},o,cancelled.token()); QVERIFY(!result);
        QVERIFY(result.error().code == ErrorCode::Cancelled);
        o.maxBoxes=1; QVERIFY(!detector->detect(image.value(),{10,10,120,80},o,CancelToken{}));
        o.maxBoxes=5000; o.minArea=500; o.maxArea=1000;
        auto empty=detector->detect(image.value(),{10,10,120,80},o,CancelToken{}); QVERIFY(empty); QVERIFY(empty.value().empty());
    }

    void canvasDetectEditSaveReload() {
        auto detector=app::makeSolderRegionDetector(); if (!detector) QSKIP("OpenCV disabled in this build.");
        QTemporaryDir dir; const auto path=makeImage(dir.filePath(QStringLiteral("socket.png"))); QVERIFY(!path.isEmpty());
        auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,detector,executor,theme); w.show(); QVERIFY(w.openImage(path));
        w.canvas()->fit(); QCoreApplication::processEvents();
        QSignalSpy found(&w,&SolderTeachingWindow::detectionFinished);
        w.detectRegion({10,10,120,80});
        QTRY_COMPARE_WITH_TIMEOUT(found.size(),qsizetype(1),5000);
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(3));
        const int id=w.session().recipe().board.components.front().id;
        w.select({id});
        w.findChild<QDoubleSpinBox*>(QStringLiteral("w"))->setValue(22);
        w.findChild<QPushButton*>(QStringLiteral("applyButton"))->click();
        QCOMPARE(w.session().box(id)->w,22.0);
        w.onComponentsMoved({id},{2,3}); const Rect edited=*w.session().box(id);
        QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved);
        const auto recipePath=dir.filePath(QStringLiteral("配方.json")); w.saveRecipe(recipePath);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(),qsizetype(1),5000); QVERIFY(saved.front().front().toBool());
        QVERIFY(!w.session().dirty());
        w.select({id}); w.deleteSelected(); QCOMPARE(w.session().recipe().board.components.size(),std::size_t(2));
        QVERIFY(w.openRecipe(recipePath)); QCOMPARE(w.session().recipe().board.components.size(),std::size_t(3));
        QCOMPARE(w.session().box(id)->x,edited.x); QCOMPARE(w.session().box(id)->w,22.0);
        QVERIFY(!w.openImage(dir.filePath(QStringLiteral("missing.png"))));
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(3));
        QImage wrong(80,60,QImage::Format_RGB32); wrong.fill(Qt::black);
        const auto wrongPath=dir.filePath(QStringLiteral("wrong.png")); QVERIFY(wrong.save(wrongPath));
        QVERIFY(!w.openRecipe(recipePath,wrongPath));
        QCOMPARE(w.session().box(id)->w,22.0);
        w.onComponentsMoved({id},{1,0}); QVERIFY(w.session().dirty());
        w.saveRecipe(dir.path()); QTRY_COMPARE_WITH_TIMEOUT(saved.size(),qsizetype(2),5000);
        QVERIFY(!saved.back().front().toBool());
        QVERIFY(w.session().dirty());
    }

    void cancelledOldResultCannotChangeNewImage() {
        QTemporaryDir dir; const auto path=makeImage(dir.filePath(QStringLiteral("socket.png")));
        auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,std::make_shared<LateDetector>(),executor,theme);
        QVERIFY(w.openImage(path)); w.detectRegion({10,10,120,80}); QVERIFY(w.busy());
        w.cancelDetection(); QVERIFY(!w.busy()); QVERIFY(w.openImage(path)); QTest::qWait(250);
        QVERIFY(w.session().recipe().board.components.empty()); QVERIFY(!w.session().dirty());
    }

    void manualEditingWithoutDetector() {
        QTemporaryDir dir; const auto path=makeImage(dir.filePath(QStringLiteral("socket.png")));
        auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); QVERIFY(w.openImage(path));
        QVERIFY(w.findChild<QAction*>(QStringLiteral("actionRegion"))->isEnabled());
        QVERIFY(!w.findChild<QAction*>(QStringLiteral("actionSearch"))->isEnabled());
        w.onRegionDrawn(QRectF(20,20,20,12),DraftMode::Sample);
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(1));
        QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved);
        w.saveRecipe(dir.filePath(QStringLiteral("manual.json")));
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(),qsizetype(1),5000); QVERIFY(saved.front().front().toBool());
    }

    void samplesProduceUniformMatchesWithoutBrightDistractors() {
        auto engine=app::makeSocketSampleSearcher(); if (!engine) QSKIP("OpenCV disabled.");
        QTemporaryDir dir; auto images=app::makeImageSource();
        auto image=images->load(makeSampleBoard(dir.filePath(QStringLiteral("patterns.png"))).toStdString()); QVERIFY(image);
        SocketSampleRequest request{image.value(),{{18,18,20,16},{87,17,22,18},{18,18,20,16}},0.90};
        auto found=engine->search(request,CancelToken{}); QVERIFY2(found,qPrintable(QString::fromStdString(found ? "" : found.error().message)));
        QCOMPARE(found.value().size(),std::size_t(3));
        for (const auto& m : found.value()) {
            QCOMPARE(m.rect.w,20.0); QCOMPARE(m.rect.h,16.0); QVERIFY(m.score>=0.90);
            const bool target=(std::abs(m.rect.x-18)<1 && (std::abs(m.rect.y-18)<1 || std::abs(m.rect.y-88)<1)) ||
                              (std::abs(m.rect.x-88)<1 && std::abs(m.rect.y-18)<1);
            QVERIFY2(target,"matched a bright distractor or duplicate position");
        }
        request.samples={{180,120,16,16}};
        QVERIFY(!engine->search(request,CancelToken{}));
        request.samples={{18,18,20,16}}; CancelSource cancelled; cancelled.cancel();
        auto stopped=engine->search(request,cancelled.token()); QVERIFY(!stopped);
        QVERIFY(stopped.error().code==ErrorCode::Cancelled);
    }

    void teachWithMouseThenF5AndRestoreSamples() {
        auto engine=app::makeSocketSampleSearcher(); if (!engine) QSKIP("OpenCV disabled.");
        QTemporaryDir dir; auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.setSampleSearcher(engine); w.show();
        QVERIFY(w.openImage(makeSampleBoard(dir.filePath(QStringLiteral("patterns.png")))));
        w.canvas()->fit(); QCoreApplication::processEvents();
        QSignalSpy found(&w,&SolderTeachingWindow::detectionFinished);
        const auto from=w.canvas()->mapFromScene(QPointF(18,18)), to=w.canvas()->mapFromScene(QPointF(38,34));
        QTest::mousePress(w.canvas()->viewport(),Qt::LeftButton,Qt::NoModifier,from);
        QTest::mouseMove(w.canvas()->viewport(),to);
        QTest::mouseRelease(w.canvas()->viewport(),Qt::LeftButton,Qt::NoModifier,to);
        QCOMPARE(w.session().samples().size(),std::size_t(1)); QCOMPARE(found.size(),qsizetype(0));
        w.findChild<QAction*>(QStringLiteral("actionSearch"))->trigger();
        QTRY_COMPARE_WITH_TIMEOUT(found.size(),qsizetype(1),10000); QVERIFY(found.front().front().toBool());
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(4)); // 一個樣本 + 三個結果。
        const auto size=w.session().samples().front();
        for (const auto& c : w.session().recipe().board.components) if (c.origin==ObjectOrigin::Auto) {
            QCOMPARE(w.session().box(c.id)->w,size.w); QCOMPARE(w.session().box(c.id)->h,size.h);
        }
        w.findChild<QDoubleSpinBox*>(QStringLiteral("similarity"))->setValue(95.0);
        QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved);
        const auto recipe=dir.filePath(QStringLiteral("samples.json")); w.saveRecipe(recipe);
        QTRY_COMPARE(saved.size(),qsizetype(1)); QVERIFY(saved.front().front().toBool());
        QVERIFY(w.openRecipe(recipe)); QCOMPARE(w.session().samples().size(),std::size_t(1));
        QCOMPARE(w.session().recipe().params.minScore,0.95);
        w.searchWholeImage(); QTRY_COMPARE_WITH_TIMEOUT(found.size(),qsizetype(2),10000);
        QCOMPARE(w.session().recipe().board.components.size(),std::size_t(4)); // 重搜不累加。
    }

    void cancelledSampleSearchDoesNotChangeNewImage() {
        QTemporaryDir dir; auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.setSampleSearcher(std::make_shared<LateSampleSearcher>());
        const auto path=makeSampleBoard(dir.filePath(QStringLiteral("patterns.png"))); QVERIFY(w.openImage(path));
        w.onRegionDrawn(QRectF(18,18,20,16),DraftMode::Region); w.searchWholeImage(); QVERIFY(w.busy());
        w.cancelDetection(); QVERIFY(w.openImage(path)); QTest::qWait(250);
        QVERIFY(w.session().recipe().board.components.empty());
    }

    void realSocketImageSmoke() {
        const QString path=QString::fromLocal8Bit(qgetenv("AOI_SOCKET_IMAGE"));
        if (path.isEmpty()) QSKIP("Set AOI_SOCKET_IMAGE to exercise a real image; no accuracy ground truth is assumed.");
        auto engine=app::makeSocketSampleSearcher(); if (!engine) QSKIP("OpenCV disabled in this build.");
        auto images=app::makeImageSource(); infra::JsonRecipeStore recipes; infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.setSampleSearcher(engine); w.show(); QVERIFY(w.openImage(path));
        QSignalSpy found(&w,&SolderTeachingWindow::detectionFinished);
        const auto start=std::chrono::steady_clock::now();
        w.onRegionDrawn(QRectF(1248,135,44,24),DraftMode::Region);
        w.onRegionDrawn(QRectF(1513,140,45,25),DraftMode::Region);
        w.onRegionDrawn(QRectF(3755,120,44,24),DraftMode::Region); // 右側反向外觀也教一個代表樣本。
        w.searchWholeImage(); QTRY_COMPARE_WITH_TIMEOUT(found.size(),qsizetype(1),60000);
        QVERIFY(found.front().front().toBool());
        std::size_t matches=0; bool lowerHalf=false, rightSide=false;
        for (const auto& c : w.session().recipe().board.components) if (c.origin==ObjectOrigin::Auto) {
            ++matches; const auto r=w.session().box(c.id); QVERIFY(r);
            QCOMPARE(r->w,44.0); QCOMPARE(r->h,24.0);
            lowerHalf=lowerHalf || r->y > w.session().recipe().board.imageHeight/2;
            rightSide=rightSide || r->x > w.session().recipe().board.imageWidth*0.65;
            QVERIFY(c.score && *c.score>=0.90);
        }
        QVERIFY(matches>10); QVERIFY(lowerHalf); QVERIFY(rightSide);
        qInfo("Socket sample whole-image smoke: %zu components, %.1f ms UI round trip",w.session().recipe().board.components.size(),
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
        const auto screen=QString::fromLocal8Bit(qgetenv("AOI_SOCKET_SCREENSHOT"));
        w.select({w.session().recipe().board.components.front().id});
        if (!screen.isEmpty()) { QTest::qWait(150); QVERIFY(w.grab().save(screen)); }
        QTemporaryDir dir; QSignalSpy saved(&w,&SolderTeachingWindow::recipeSaved);
        const auto target=dir.filePath(QStringLiteral("socket.json")); w.saveRecipe(target);
        QTRY_COMPARE_WITH_TIMEOUT(saved.size(),qsizetype(1),5000); QVERIFY(saved.front().front().toBool());
        QVERIFY(w.openRecipe(target));
    }

    void thousandBoxesMarqueeAndDragStayResponsive_data() {
        QTest::addColumn<int>("boxCount");
        QTest::newRow("reported-1017") << 1017;
        QTest::newRow("capacity-5000") << 5000;
    }

    void thousandBoxesMarqueeAndDragStayResponsive() {
        QFETCH(int,boxCount);
        QTemporaryDir dir;
        const int imageHeight=std::max(1000,((boxCount+39)/40)*35+50);
        QImage image(1600,imageHeight,QImage::Format_RGB32); image.fill(Qt::black);
        const auto imagePath=dir.filePath(QStringLiteral("large.png")); QVERIFY(image.save(imagePath));
        SolderTeachingSession seed; seed.openImage(imagePath.toStdString(),1600,imageHeight,0);
        std::vector<Rect> boxes;
        for (int i=0;i<boxCount;++i) boxes.push_back({20.0+(i%40)*38,20.0+(i/40)*35,20,14});
        QVERIFY(seed.replaceRegion({0,0,1600,static_cast<double>(imageHeight)},boxes));
        infra::JsonRecipeStore recipes;
        const auto recipePath=dir.filePath(QStringLiteral("large.json"));
        QVERIFY(recipes.save(seed.recipe(),recipePath.toStdString()));
        auto images=app::makeImageSource(); infra::QtThreadPoolExecutor executor(2);
        ThemeManager theme(*qobject_cast<QApplication*>(QCoreApplication::instance()),ThemeTokens::builtin());
        SolderTeachingWindow w(*images,recipes,{},executor,theme); w.resize(1430,920); w.show();
        QVERIFY(w.openRecipe(recipePath)); w.canvas()->fit();
        // 容量測試維持 1:1，避免小框縮到幾個螢幕像素後的取整與拖曳門檻干擾驗證。
        w.canvas()->zoomAt(QPointF(w.canvas()->viewport()->rect().center()),1.0/w.canvas()->viewScale());
        w.canvas()->centerOn(350,200); QCoreApplication::processEvents();
        auto* viewport=w.canvas()->viewport();
        const auto at=[&](double x,double y) { return w.canvas()->mapFromScene(QPointF(x,y)); };
        QTest::mousePress(viewport,Qt::LeftButton,Qt::NoModifier,at(15,15));
        QTest::mouseMove(viewport,at(195,38));
        QTest::mouseRelease(viewport,Qt::LeftButton,Qt::NoModifier,at(195,38));
        auto* table=w.findChild<QTableWidget*>(QStringLiteral("table"));
        QCOMPARE(table->selectionModel()->selectedRows().size(),qsizetype(5));
        const int id=w.session().recipe().board.components.front().id;
        const Rect original=*w.session().box(id);
        const auto revision=w.session().revision();
        QTest::mousePress(viewport,Qt::LeftButton,Qt::NoModifier,at(30,27));
        const auto start=std::chrono::steady_clock::now();
        for (int step=1;step<=20;++step) {
            QTest::mouseMove(viewport,at(30+step,27+step));
            QCoreApplication::processEvents();
        }
        QCOMPARE(w.session().revision(),revision); // 預覽不能提交或重建文件。
        const auto released=std::chrono::steady_clock::now();
        QTest::mouseRelease(viewport,Qt::LeftButton,Qt::NoModifier,at(50,47));
        const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        const double releaseMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-released).count();
        qInfo("%d boxes / 5 selected / 20 drag moves + release: %.2f ms; release: %.2f ms",boxCount,elapsed,releaseMs);
        QVERIFY2(elapsed<2000,"A short multi-selection drag blocked the UI thread.");
        QVERIFY(w.session().box(id)->x>original.x+18);
        QVERIFY(w.session().box(id)->y>original.y+18);
        QCOMPARE(w.session().revision(),revision+1);
        QCOMPARE(table->selectionModel()->selectedRows().size(),qsizetype(5));
        w.findChild<QAction*>(QStringLiteral("actionUndo"))->trigger();
        QCOMPARE(w.session().box(id)->x,original.x);
        w.findChild<QAction*>(QStringLiteral("actionRedo"))->trigger();
        QVERIFY(w.session().box(id)->x>original.x+18);
        const Rect moved=*w.session().box(id);
        const auto movedRevision=w.session().revision();
        QTest::mousePress(viewport,Qt::LeftButton,Qt::NoModifier,at(50,47));
        QTest::mouseMove(viewport,at(70,67));
        QTest::keyClick(viewport,Qt::Key_Escape);
        QTest::mouseRelease(viewport,Qt::LeftButton,Qt::NoModifier,at(70,67));
        QCOMPARE(w.session().revision(),movedRevision);
        QCOMPARE(w.session().box(id)->x,moved.x);
        QCOMPARE(table->selectionModel()->selectedRows().size(),qsizetype(5));
    }
};

QTEST_MAIN(SolderTeachingTest)
#include "tst_solder_teaching.moc"
