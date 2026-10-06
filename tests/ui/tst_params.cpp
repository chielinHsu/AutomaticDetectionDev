// ParamsPanel（參數 dock 的控制項）與 ParamsPresenter（分數門檻立即、重新搜尋的參數防抖）。
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest>
#include <vector>

#include "presentation/i18n/Translations.h"
#include "presentation/params/ParamsPanel.h"
#include "presentation/theme/ThemeTokens.h"
#include "presentation/presenters/ParamsPresenter.h"

using namespace aoi;
using namespace aoi::ui;

namespace {

// 記錄 ParamsPresenter 交過來什麼的假 sink。
class RecordingSink final : public IParamsSink {
public:
    SearchParams params;
    std::vector<double> thresholds;
    std::vector<SearchParams> searches;
    std::vector<int> scopes;
    std::vector<int> bits;

    SearchParams searchParams() const override { return params; }
    void applyThreshold(double t) override { thresholds.push_back(t); }
    void applySearchParams(const SearchParams& p) override {
        searches.push_back(p);
        params = p;
    }
    void applyScanScope(int s) override { scopes.push_back(s); }
    void applySignificantBits(int b) override { bits.push_back(b); }
};

}  // namespace

class TstParams : public QObject {
    Q_OBJECT
private slots:
    void init() { Translations::install(QStringLiteral("zh_TW")); }

    // ---- ParamsPanel ----
    void draggingTheSliderReportsEveryStepAsAThreshold() {
        ParamsPanel panel;
        QSignalSpy spy(&panel, &ParamsPanel::thresholdChanged);
        panel.scoreSlider()->setValue(85);
        panel.scoreSlider()->setValue(86);
        QCOMPARE(spy.count(), 2);  // 拖曳中的每一步都回報（U3 量的就是這個）
        QCOMPARE(spy.at(0).at(0).toDouble(), 0.85);
        QCOMPARE(spy.at(1).at(0).toDouble(), 0.86);
        QCOMPARE(panel.scoreValue()->text(), QStringLiteral("0.86"));
    }

    void settingTheParamsFromTheOutsideDoesNotReportAnything() {
        ParamsPanel panel;
        QSignalSpy threshold(&panel, &ParamsPanel::thresholdChanged);
        QSignalSpy angle(&panel, &ParamsPanel::angleRangeChanged);
        ParamsState p;
        p.threshold = 0.42;
        p.angleMinDeg = -15;
        p.angleMaxDeg = 20;
        p.scopeIndex = 2;
        panel.setParams(p);
        QCOMPARE(threshold.count(), 0);
        QCOMPARE(angle.count(), 0);
        QCOMPARE(panel.scoreSlider()->value(), 42);
        QCOMPARE(panel.scoreValue()->text(), QStringLiteral("0.42"));
        QCOMPARE(panel.angleMin()->value(), -15.0);
        QCOMPARE(panel.angleMax()->value(), 20.0);
        QCOMPARE(panel.scopeCombo()->currentIndex(), 2);
    }

    void theScoreRangeFollowsTheCore() {
        ParamsPanel panel;
        panel.setScoreRange(0.30, 0.99);
        QCOMPARE(panel.scoreSlider()->minimum(), 30);
        QCOMPARE(panel.scoreSlider()->maximum(), 99);
    }

    void theAngleRowIsOnlyInAdvancedMode() {
        ParamsPanel panel;
        panel.show();
        QVERIFY(!panel.angleRowVisible());
        panel.advancedToggle()->setChecked(true);
        QVERIFY(panel.angleRowVisible());
        panel.advancedToggle()->setChecked(false);
        QVERIFY(!panel.angleRowVisible());
    }

    void theLowerAngleNeverPassesTheUpperOne() {
        ParamsPanel panel;
        QSignalSpy spy(&panel, &ParamsPanel::angleRangeChanged);
        panel.angleMin()->setValue(12.0);  // 預設上限是 +5
        QCOMPARE(panel.angleMax()->value(), 12.0);
        QVERIFY(spy.count() >= 1);
        QCOMPARE(spy.last().at(0).toDouble(), 12.0);
        QCOMPARE(spy.last().at(1).toDouble(), 12.0);
        panel.angleMax()->setValue(-20.0);  // 反過來也是
        QCOMPARE(panel.angleMin()->value(), -20.0);
    }

    void pickingAScopeReportsTheIndex() {
        ParamsPanel panel;
        QSignalSpy spy(&panel, &ParamsPanel::scopeChanged);
        panel.scopeCombo()->setCurrentIndex(1);
        emit panel.scopeCombo()->activated(1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toInt(), 1);
    }

    void theThreeScopesAreTranslated() {
        ParamsPanel panel;
        QVERIFY(panel.scopeCombo()->itemText(0).contains(QStringLiteral("板子")));
        Translations::install(QStringLiteral("en"));
        panel.retranslate();
        QVERIFY(panel.scopeCombo()->itemText(2).contains(QStringLiteral("library")));
    }

    // ---- 影像位元數（S07）----
    void theBitDepthComboShowsTheDetectedValueAndTheChoice() {
        ParamsPanel panel;
        QComboBox* combo = panel.bitsCombo();
        QCOMPARE(combo->count(), 6);  // 自動、8、10、12、14、16
        QVERIFY(!combo->isEnabled());  // 還沒有影像
        QCOMPARE(combo->itemText(0), QStringLiteral("自動"));
        ParamsState p;
        p.detectedBits = 10;
        p.significantBits = 12;
        p.bitsApplicable = true;
        QSignalSpy spy(&panel, &ParamsPanel::significantBitsChanged);
        panel.setParams(p);
        QCOMPARE(spy.count(), 0);  // 外面設的值不回報
        QVERIFY(combo->isEnabled());
        QCOMPARE(combo->itemText(0), QStringLiteral("自動（偵測：10 bit）"));
        QCOMPARE(combo->currentData().toInt(), 12);
        QCOMPARE(combo->currentText(), QStringLiteral("12 bit"));
        // 不是 16 bit 灰階：停用。
        p.bitsApplicable = false;
        p.detectedBits = 0;
        p.significantBits = 0;
        panel.setParams(p);
        QVERIFY(!combo->isEnabled());
        QCOMPARE(combo->currentIndex(), 0);
    }

    void pickingABitDepthReportsTheBits() {
        ParamsPanel panel;
        QSignalSpy spy(&panel, &ParamsPanel::significantBitsChanged);
        const int twelve = panel.bitsCombo()->findData(12);
        QVERIFY(twelve > 0);
        panel.bitsCombo()->setCurrentIndex(twelve);
        emit panel.bitsCombo()->activated(twelve);
        emit panel.bitsCombo()->activated(0);
        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).first().toInt(), 12);
        QCOMPARE(spy.at(1).first().toInt(), 0);  // 自動
        Translations::install(QStringLiteral("en"));
        panel.retranslate();
        QCOMPARE(panel.bitsCombo()->itemText(twelve), QStringLiteral("12-bit"));
    }

    void disablingThePanelDisablesTheControls() {
        ParamsPanel panel;
        static_cast<IParamsView&>(panel).setEnabled(false);
        QVERIFY(!panel.scoreSlider()->isEnabled());
        static_cast<IParamsView&>(panel).setEnabled(true);
        QVERIFY(panel.scoreSlider()->isEnabled());
    }

    // ---- ParamsPresenter ----
    void theBitDepthGoesStraightThrough() {
        RecordingSink sink;
        ParamsPresenter p(sink, 120);
        p.setSignificantBits(12);
        p.setSignificantBits(0);
        QCOMPARE(sink.bits, (std::vector<int>{12, 0}));
        QVERIFY(sink.searches.empty());  // 不搜尋
        QVERIFY(!p.searchPending());
    }

    void theThresholdGoesStraightThrough() {
        RecordingSink sink;
        ParamsPresenter p(sink, 120);
        p.setScoreThreshold(0.8);
        p.setScoreThreshold(0.81);
        QCOMPARE(sink.thresholds, (std::vector<double>{0.8, 0.81}));
        QVERIFY(sink.searches.empty());
        QVERIFY(!p.searchPending());
    }

    void angleChangesAreDebouncedIntoOneSearchWithTheLastValues() {
        RecordingSink sink;
        sink.params.maxMatches = 777;  // 其他欄位原樣保留
        ParamsPresenter p(sink, 60);
        p.setAngleRange(-6, 6);
        p.setAngleRange(-7, 7);
        p.setAngleRange(-8, 9);
        QVERIFY(p.searchPending());
        QVERIFY(sink.searches.empty());  // 防抖期間不搜尋
        QVERIFY(QTest::qWaitFor([&] { return !sink.searches.empty(); }, 2000));
        QCOMPARE(static_cast<int>(sink.searches.size()), 1);
        QCOMPARE(sink.searches[0].angleMinDeg, -8.0);
        QCOMPARE(sink.searches[0].angleMaxDeg, 9.0);
        QCOMPARE(sink.searches[0].maxMatches, std::size_t{777});
        QVERIFY(!p.searchPending());
        QTest::qWait(150);
        QCOMPARE(static_cast<int>(sink.searches.size()), 1);  // 沒有第二次
    }

    void everyChangeRestartsTheDebounceTimer() {
        RecordingSink sink;
        ParamsPresenter p(sink, 100);
        p.setAngleRange(-6, 6);
        QTest::qWait(60);
        p.setAngleRange(-7, 7);  // 第一次的計時還沒到，重新計時
        QTest::qWait(60);
        QVERIFY(sink.searches.empty());  // 距離第一次已經 120 ms，但距離最後一次只有 60 ms
        QVERIFY(QTest::qWaitFor([&] { return !sink.searches.empty(); }, 2000));
        QCOMPARE(sink.searches[0].angleMinDeg, -7.0);
    }

    void flushAppliesThePendingSearchNow() {
        RecordingSink sink;
        ParamsPresenter p(sink, 5000);
        p.flush();  // 沒有待處理的：什麼都不做
        QVERIFY(sink.searches.empty());
        p.setAngleRange(-3, 3);
        p.flush();
        QCOMPARE(static_cast<int>(sink.searches.size()), 1);
        QVERIFY(!p.searchPending());
    }

    void theScopeGoesStraightThrough() {
        RecordingSink sink;
        ParamsPresenter p(sink, 120);
        p.setScanScope(2);
        QCOMPARE(sink.scopes, (std::vector<int>{2}));
    }

    void theDebounceComesFromTheTokens() {
        QCOMPARE(ThemeTokens::builtin().timing.paramDebounce, 120);
    }
};

QTEST_MAIN(TstParams)
#include "tst_params.moc"
