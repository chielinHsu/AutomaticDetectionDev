#pragma once

#include <QObject>
#include <QString>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "application/backend/BackendRegistry.h"
#include "application/backend/SearchEngine.h"
#include "application/compare/BackendComparison.h"
#include "application/jobs/JobRunner.h"
#include "application/ports/IExecutor.h"
#include "presentation/compare/CompareInterfaces.h"
#include "presentation/presenters/BackendPresenter.h"
#include "presentation/presenters/UiPoster.h"

namespace aoi::ui {

// A/B 比較需要的核心（組裝根給的，和 BackendServices 同一份來源）。
struct CompareServices {
    std::vector<BackendInfo> backends;          // 註冊順序（由簡單到完整），含不可用的
    SearchEngineFactory create;                 // 建立 back end ＋ use case（在背景執行緒呼叫）
    IExecutor* executor = nullptr;              // 背景執行緒（和主視窗的 JobRunner 共用）；空 ＝ 在 UI 執行緒跑（測試）
    std::vector<SearchEnginePtr> ready;         // 已經建好的（啟動時的那一個）：直接用，不重建
    std::function<std::string()> currentBackend;  // 主視窗目前用的 back end（預設的 A）
};

// A/B 比較的 Presenter（S09）：主視窗的入口、比較視窗、兩個 back end 依序搜尋同一個樣本、配對與摘要。
//
//  - 規則（怎麼配對、怎麼分類、差異的原因）在 application 的 BackendComparison（沒有 Qt，單元測試逐項驗證）；
//    這裡只做 Qt 這一側：背景執行緒 ↔ UI 執行緒、把結果變成畫面上的文字與標記。
//  - 自己的 JobRunner（latest-wins 只在比較之間）：比較不會作廢主視窗進行中的搜尋，主視窗的搜尋也不會作廢比較。
//    兩者共用同一個執行緒池。
//  - 比較用的 back end 第一次用到時在背景建立（HALCON 第一次約 2.8 秒），之後留著重複用；啟動時已經建好的直接用。
//  - 分數門檻兩邊各自一個（兩邊的分數不是同一把尺），改門檻只重新比較（compareProposals），不重新搜尋。
//  - 只在 UI 執行緒使用。
class ComparePresenter : public QObject, public ICompareController {
    Q_OBJECT
public:
    ComparePresenter(ICompareEntryView& entry, ICompareView& view, ICompareSource& source, CompareServices services,
                     QObject* parent = nullptr);
    ~ComparePresenter() override;

    // ---- ICompareController ----
    void openCompare() override;
    void runCompare(const QString& idA, const QString& idB) override;
    void setThresholds(double a, double b) override;
    void cancelCompare() override;

    // ---- 讀（測試與量測用）----
    bool running() const { return running_; }
    const std::optional<aoi::BackendComparison>& comparison() const { return comparison_; }
    const CompareResultView& lastResult() const { return result_; }
    const std::string& idA() const { return idA_; }
    const std::string& idB() const { return idB_; }
    // 比較視窗的預設：A ＝ 主視窗目前的 back end、B ＝ 另一個能用的（註冊順序裡最完整的那一個）。
    std::pair<std::string, std::string> defaultPair() const;

public slots:
    // 主視窗的狀態變了（AutoRoiPresenter::stateChanged）：入口要不要能按。
    void refreshEntry();
    // 語言換了：重新發布所有文字。
    void refreshTexts();

signals:
    // 一次比較結束（成功、失敗或取消）；測試等它。
    void compareFinished(bool succeeded);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct Outcome;
    class ProgressBridge;
    friend class ProgressBridge;

    void onFinished(std::uint64_t generation, const JobReport& report, const std::shared_ptr<Outcome>& outcome);
    void onProgress(std::uint64_t generation, double fraction);
    void publishChoices();
    void publishResult();
    QString displayName(const std::string& id) const;
    bool available(const std::string& id) const;
    std::vector<std::string> availableIds() const;
    SearchEnginePtr engine(const std::string& id) const;

    ICompareEntryView& entry_;
    ICompareView& view_;
    ICompareSource& source_;
    CompareServices services_;
    std::shared_ptr<UiPoster> poster_;
    std::unique_ptr<JobRunner> runner_;  // executor 是空的時候沒有（同步執行）
    std::map<std::string, SearchEnginePtr> engines_;
    std::string idA_;
    std::string idB_;
    std::optional<CompareRequest> request_;  // 正在比較或畫面上那一次的請求
    double thresholdA_ = 0.70;
    double thresholdB_ = 0.70;
    std::shared_ptr<const aoi::SampleProposal> proposalA_;
    std::shared_ptr<const aoi::SampleProposal> proposalB_;
    std::optional<aoi::BackendComparison> comparison_;
    CompareResultView result_;
    std::uint64_t generation_ = 0;
    bool running_ = false;
    QString status_;
};

}  // namespace aoi::ui
