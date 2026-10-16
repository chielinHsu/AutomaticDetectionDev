#pragma once

#include <QList>
#include <QPointF>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "application/compare/BackendComparison.h"
#include "domain/geometry.h"
#include "domain/image_view.h"
#include "domain/match_region.h"
#include "domain/search_params.h"
#include "presentation/presenters/ViewInterfaces.h"

// A/B 比較（S09）的 View、Controller 與資料。每個介面 ≤ 5 個方法；文字都是 Presenter 翻好的字串（View 不查文案）。
namespace aoi::ui {

// 要比較的東西：目前畫面上的影像、使用者框的樣本、搜尋參數與分數門檻（AutoRoiPresenter 提供）。
struct CompareRequest {
    aoi::ImageView image;  // 帶著 owner：像素活到比較結束
    aoi::Rect sample;
    aoi::MatchSpec match;  // 樣本的比對區（ADR-028）：和主搜尋用同一個範本
    aoi::SearchParams search;
    double threshold = 0.70;
    QString imageName;
};

// 主視窗這一側：有沒有東西可以比較。
class ICompareSource {
public:
    virtual ~ICompareSource() = default;
    // 有樣本的搜尋（預覽、無匹配、錯誤）才有；其餘（沒有影像、還沒框樣本、框的是區域）沒有。
    virtual std::optional<CompareRequest> compareRequest() const = 0;
};

// 主視窗上的入口（檢視選單與工具列的〔A/B 比較…〕）。
class ICompareEntryView {
public:
    virtual ~ICompareEntryView() = default;
    virtual void setCompareAvailable(bool enabled, const QString& tip) = 0;
};

// 疊圖的一個標記：兩邊都找到＝綠圓、只有 A＝藍方、只有 B＝橘三角（形狀不同，不只靠顏色）。
struct CompareMark {
    QPointF center;  // 影像像素（場景座標）
    double angleDeg = 0.0;
    aoi::CompareKind kind = aoi::CompareKind::Both;
};

struct CompareOverlay {
    QSizeF partSize;  // 每個元件的框（範本大小）
    std::vector<CompareMark> marks;
    std::size_t count(aoi::CompareKind kind) const {
        return static_cast<std::size_t>(std::count_if(marks.begin(), marks.end(), [kind](const CompareMark& m) { return m.kind == kind; }));
    }
};
using CompareOverlayPtr = std::shared_ptr<const CompareOverlay>;

// 摘要表的一列：項目、A、B、B − A。
struct CompareTableRow {
    QString label;
    QString a;
    QString b;
    QString diff;
};

// 差異清單的一列（只有 A、只有 B）。markIndex 是疊圖裡那一個標記的索引（選了它，畫布移過去）。
struct CompareDifferenceRow {
    QString kind;
    QString where;
    QString scoreA;
    QString scoreB;
    QString reason;
    int markIndex = -1;
};

struct CompareResultView {
    QString title;          // 視窗標題
    QString status;         // 「完成：OpenCV 72 ms、HALCON 31 ms」
    QStringList legend;     // 三種標記的說明（含數量），依 Both、OnlyA、OnlyB
    QList<CompareTableRow> table;
    QString differencesTitle;  // 「差異（12）」
    QList<CompareDifferenceRow> differences;
    CompareOverlayPtr overlay;
};

// 比較視窗（CompareWindow）。
class ICompareView {
public:
    virtual ~ICompareView() = default;
    // 選單：兩個 back end 選單的內容（註冊順序，不可用的停用）與目前選的 A、B。
    virtual void setChoices(const QList<BackendChoice>& choices, int a, int b) = 0;
    // 顯示視窗（已經顯示就帶到前面）與要比較的影像（換了影像才重畫）。
    virtual void showRequest(const aoi::ImageView& image, double thresholdA, double thresholdB) = 0;
    virtual void showResult(const CompareResultView& result) = 0;
    // 比較中（停用〔比較〕、顯示〔取消〕）或不在比較中；text 是狀態列的文字（進度、錯誤、取消）。
    virtual void showBusy(bool busy, const QString& text) = 0;
};

// 使用者的操作（主視窗的入口與比較視窗）。
class ICompareController {
public:
    virtual ~ICompareController() = default;
    // 主視窗的〔A/B 比較…〕：打開比較視窗，A ＝ 目前的 back end、B ＝ 另一個能用的，馬上比較。
    virtual void openCompare() = 0;
    virtual void runCompare(const QString& idA, const QString& idB) = 0;
    // 兩邊各自的門檻（分數不是同一把尺）：只重新比較，不重新搜尋。
    virtual void setThresholds(double a, double b) = 0;
    virtual void cancelCompare() = 0;
};

}  // namespace aoi::ui
