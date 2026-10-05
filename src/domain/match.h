#pragma once

#include <optional>
#include <string>
#include <vector>

#include "domain/geometry.h"
#include "domain/object_state.h"

// domain 層：只用 C++ 標準庫。座標與角度慣例見 geometry.h；本檔所有座標都是影像像素。
namespace aoi {

// 搜尋器回報的一個匹配：中心位姿與分數（0..1）。
struct Match {
    Pose2D pose;
    double score = 0.0;
};

// 一個候選元件的來源（流程 A）。
enum class CandidateSource {
    Sample,     // 使用者框的那一個；分數是 1.0（範本就是從它建的，依定義完全相符）或搜尋器回報的自我匹配分數
    Seed,       // 種子搜尋（整張影像，較高的分數下限）找到
    Verified,   // 晶格預測的位置，在小視窗內以較低的分數下限找到
    Predicted,  // 晶格預測的位置，視窗內沒有找到；沒有分數
};

// use case 回傳的候選。**所有**驗證過的候選都回傳（分數 ≥ 搜尋下限），由 Presenter 套門檻：
// 分數門檻變動時只要重新過濾，不必重新搜尋（ux-spec §12.1 第 3 項、預算 U3）。
struct Candidate {
    Pose2D pose;
    double score = 0.0;  // Predicted 沒有分數，固定為 0，不可拿來排序或顯示
    CandidateSource source = CandidateSource::Seed;

    bool hasScore() const noexcept { return source != CandidateSource::Predicted; }
};

// 套過門檻的結果：一個檢測框與它的狀態。流程 A 接受後，每個框就是一個元件的主體框。
struct InspectionObject {
    int id = 0;  // 從 1 起，依候選順序；每次 classify 重新編號
    RectObj rect;
    ObjectState state = ObjectState::Found;
    std::optional<double> score;  // 手動補的、Predicted 的沒有
    ObjectOrigin origin = ObjectOrigin::Auto;
};

// 流程 B：一個辨識出的元件，附每個候選類型的分數（由高到低）。
struct TypeScore {
    std::string typeId;
    double score = 0.0;
};

struct ComponentCandidate {
    Pose2D pose;
    std::vector<TypeScore> typeScores;  // 空 ＝ 認得這裡有元件，但沒有任何類型的分數 ≥ 下限
};

}  // namespace aoi
