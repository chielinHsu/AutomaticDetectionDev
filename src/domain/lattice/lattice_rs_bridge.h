#pragma once

#include <cstddef>
#include <exception>
#include <span>
#include <vector>

#include "domain/ffi/aoi_domain_rs.h"
#include "domain/geometry.h"
#include "domain/lattice/lattice_accumulator_access.h"
#include "domain/lattice/lattice_basis.h"
#include "domain/lattice/lattice_detail.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_model.h"
#include "domain/lattice/vector_clustering.h"

// domain 層：只用 C++ 標準庫。AOI_DOMAIN_IMPL=rust 才編（R05.2，ADR-031）：晶格的 *_rs.cpp 共用的轉換。
//
// 契約和 C++ 的實作相同（各公開標頭）：noexcept 的函式遇到 Rust 回報錯誤時回安全值；其他的配置失敗丟 std::bad_alloc；
// Rust 的 panic 是 bug，丟 std::logic_error（panic 本身不跨過 FFI）；自訂的分群策略丟出的例外原樣重新丟出。
namespace aoi::detail {

// GridVoteClustering、GreedyClustering 的 friend：Rust 的實作要知道內建策略的參數（列舉＋參數，不必回呼）。
struct ClusteringAccess {
    static double binFraction(const GridVoteClustering& c) noexcept { return c.binFraction_; }
    static double tolerance(const GreedyClustering& c) noexcept { return c.tolerance_; }
};

}  // namespace aoi::detail

namespace aoi::rs {

// 連到 ABI 不同的程式庫（例如舊的建置）時第一次呼叫就失敗。
void checkAbi();
// AOI_RS_OK 以外：AOI_RS_NO_MEMORY 丟 std::bad_alloc，其他丟 std::logic_error。
void check(AoiRsStatus status, const char* what);

inline AoiRsPoint toRs(Point2D p) noexcept { return {p.x, p.y}; }
inline Point2D fromRs(AoiRsPoint p) noexcept { return {p.x, p.y}; }
inline AoiRsCell toRs(LatticeIndex c) noexcept { return {c.i, c.j}; }
inline LatticeIndex fromRs(AoiRsCell c) noexcept { return {c.i, c.j}; }
AoiRsBasis toRs(const LatticeBasis& b) noexcept;
LatticeBasis fromRs(const AoiRsBasis& b) noexcept;
AoiRsLatticeSums toRs(const detail::LatticeSums& s) noexcept;
detail::LatticeSums fromRs(const AoiRsLatticeSums& s) noexcept;
AoiRsLatticeOptions toRs(const LatticeOptions& o) noexcept;

// detail::LatticeSums 和 AoiRsLatticeSums 的欄位一一相同（lattice_rs_bridge.cpp 逐欄 static_assert）：累加器直接交出位址，不複製。
// C++ 這邊從不經過 AoiRsLatticeSums 讀寫（只在 Rust 那一側，另一個語言、另一個翻譯單元），所以沒有型別別名的問題。
inline AoiRsLatticeSums* asRs(detail::LatticeSums& s) noexcept { return reinterpret_cast<AoiRsLatticeSums*>(&s); }
inline const AoiRsLatticeSums* asRs(const detail::LatticeSums& s) noexcept { return reinterpret_cast<const AoiRsLatticeSums*>(&s); }
inline AoiRsBounds toRs(const Rect& r) noexcept { return {r.x, r.y, r.w, r.h}; }

// 複製成 C 的結構（Point2D 與 AoiRsPoint 的欄位相同，但不同型別之間轉指標在 C++ 不合法）。
std::vector<AoiRsPoint> toRs(std::span<const Point2D> points);
std::vector<AoiRsCell> toRs(const std::vector<LatticeIndex>& cells);

// 分群策略：GridVoteClustering、GreedyClustering 用列舉＋參數；其他的（測試注入的、使用者的）用回呼。
// 回呼裡的 C++ 例外不可以穿過 Rust：記在這裡，Rust 回傳 AOI_RS_CALLBACK_FAILED 之後由 check() 重新丟出。
// 一次呼叫一個物件（放在呼叫端的堆疊上），所以多條執行緒同時估計互不影響。
class ClusteringBridge {
public:
    explicit ClusteringBridge(const IVectorClustering& clustering) noexcept;
    ClusteringBridge(const ClusteringBridge&) = delete;
    ClusteringBridge& operator=(const ClusteringBridge&) = delete;

    const AoiRsClustering* spec() const noexcept { return &spec_; }
    // 回呼丟過例外就重新丟出；回呼的輸出違反契約丟 std::logic_error；其餘照 rs::check。
    void check(AoiRsStatus status, const char* what) const;

private:
    static AoiRsStatus trampoline(void* context, const AoiRsPoint* vectors, std::size_t count, double scale,
                                  AoiRsVectorCluster* clusters, std::size_t* clusterCount, int32_t* labels) noexcept;

    const IVectorClustering& clustering_;
    AoiRsClustering spec_{};
    mutable std::exception_ptr error_;
};

// LatticeModel 的 C 檢視（陣列指標指向這個物件裡的複本；物件活著時有效）。
class ModelView {
public:
    explicit ModelView(const LatticeModel& m);
    ModelView(const ModelView&) = delete;
    ModelView& operator=(const ModelView&) = delete;
    const AoiRsLatticeModel* get() const noexcept { return &view_; }

private:
    std::vector<std::size_t> inliers_;
    std::vector<AoiRsCell> cells_;
    AoiRsLatticeModel view_{};
};

// detail::LatticeCandidate 的 C 檢視。
class CandidateView {
public:
    explicit CandidateView(const detail::LatticeCandidate& c);
    CandidateView(const CandidateView&) = delete;
    CandidateView& operator=(const CandidateView&) = delete;
    const AoiRsLatticeCandidate* get() const noexcept { return &view_; }

private:
    std::vector<std::size_t> members_;
    std::vector<AoiRsCell> cells_;
    AoiRsLatticeCandidate view_{};
};

// Rust 的結果（aoi_rs_lattice_fit_model 讀出的檢視）複製成 LatticeModel。
LatticeModel toModel(const AoiRsLatticeModel& m);

}  // namespace aoi::rs
