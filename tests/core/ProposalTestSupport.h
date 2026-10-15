#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "application/usecases/BoardProposal.h"
#include "application/usecases/GenerateObjectsFromSample.h"

// 文件、提案與工作階段的測試共用：直接組一份提案（不跑 use case），分數由測試指定。
// 2026-10-14 起提案是板子的提案（BoardProposal，ADR-025）：這裡用單一樣本的 SampleProposal 組好再轉過去（不合併、不剔除），
// 候選順序與編號和以前一樣。typeId 是板子上的類型；測試的文件沒有這個類型時，接受不會放進任何元件（見 tst_document）。
namespace aoi::test {

inline constexpr const char* kTestTypeId = "t1";

// 候選順序：Sample、各個分數的 Seed（水平排開，間隔 50）、可選的 Predicted。
// 編號（classify 依順序編 1..N）：Sample ＝ 1，scores[i] ＝ i + 2，Predicted 接在後面。
inline SampleProposal makeSampleProposal(const std::vector<double>& scores, std::size_t predicted = 0, Size2D part = {40, 20}) {
    SampleProposal p;
    p.partSize = part;
    Candidate sample;
    sample.pose = {100, 100, 0};
    sample.score = 1.0;
    sample.source = CandidateSource::Sample;
    p.candidates.push_back(sample);
    double x = 150;
    for (double s : scores) {
        Candidate c;
        c.pose = {x, 100, 0};
        c.score = s;
        c.source = CandidateSource::Seed;
        p.candidates.push_back(c);
        x += 50;
    }
    for (std::size_t i = 0; i < predicted; ++i) {
        Candidate c;
        c.pose = {x, 100, 0};
        c.score = 0.0;
        c.source = CandidateSource::Predicted;
        p.candidates.push_back(c);
        x += 50;
    }
    return p;
}

inline std::shared_ptr<const BoardProposal> makeProposal(const std::vector<double>& scores, std::size_t predicted = 0,
                                                         Size2D part = {40, 20}, std::string typeId = kTestTypeId) {
    return std::make_shared<const BoardProposal>(boardProposalFrom(makeSampleProposal(scores, predicted, part), std::move(typeId)));
}

// ADR-026：帶晶格位姿的提案。和 makeProposal 同樣的候選順序與編號；scores 的索引在 members 裡的 Seed 是晶格的成員：
// 格點在原本的位置（x, 100）、匹配位置偏 (0.4, −0.3)（偏差 0.5）；在 strays 裡的 Seed 偏離陣列：匹配位置往下 8 px、沒有晶格位姿、
// 離最近格點 8；其餘的 Seed 不在任何陣列（兩個欄位都沒有）。Sample 是成員（偏差 0）；Predicted 本來就在格點上。
inline std::shared_ptr<const BoardProposal> makeLatticeProposal(const std::vector<double>& scores, const std::vector<std::size_t>& members,
                                                                const std::vector<std::size_t>& strays = {}, std::size_t predicted = 0,
                                                                std::string typeId = kTestTypeId) {
    SampleProposal p = makeSampleProposal(scores, predicted);
    const auto has = [](const std::vector<std::size_t>& v, std::size_t i) { return std::find(v.begin(), v.end(), i) != v.end(); };
    p.candidates[0].latticePose = p.candidates[0].pose;
    p.candidates[0].latticeDeviationPx = 0.0;
    for (std::size_t i = 0; i < scores.size(); ++i) {
        Candidate& c = p.candidates[i + 1];
        if (has(members, i)) {
            c.latticePose = c.pose;
            c.pose.x += 0.4;
            c.pose.y -= 0.3;
            c.latticeDeviationPx = 0.5;
        } else if (has(strays, i)) {
            c.pose.y += 8.0;
            c.latticeDeviationPx = 8.0;
        }
    }
    for (std::size_t i = scores.size() + 1; i < p.candidates.size(); ++i) {
        p.candidates[i].latticePose = p.candidates[i].pose;
        p.candidates[i].latticeDeviationPx = 0.0;
    }
    return std::make_shared<const BoardProposal>(boardProposalFrom(p, std::move(typeId)));
}

// 搜尋前就知道的樣本問題（候選是空的）。
inline std::shared_ptr<const BoardProposal> makeProblemProposal(NoMatchReason reason) {
    auto p = std::make_shared<BoardProposal>();
    p->sampleProblem = NoMatchInfo{reason, 0.0, 0.0, 0};
    return p;
}

// 很多候選的提案（效能與 I6 用）：n 個 Seed，排成方格，分數在 0.5–1.0 之間決定性地變化。
inline std::shared_ptr<const BoardProposal> makeBigProposal(std::size_t n, Size2D part = {24, 16}, std::string typeId = kTestTypeId) {
    SampleProposal p;
    p.partSize = part;
    Candidate sample;
    sample.pose = {20, 20, 0};
    sample.score = 1.0;
    sample.source = CandidateSource::Sample;
    p.candidates.push_back(sample);
    const std::size_t columns = 70;
    for (std::size_t i = 0; i < n; ++i) {
        Candidate c;
        c.pose = {20.0 + static_cast<double>((i + 1) % columns) * 36.0, 20.0 + static_cast<double>((i + 1) / columns) * 24.0, 0};
        c.score = 0.50 + 0.50 * static_cast<double>((i * 2654435761u) % 1000u) / 1000.0;
        c.source = CandidateSource::Seed;
        p.candidates.push_back(c);
    }
    return std::make_shared<const BoardProposal>(boardProposalFrom(p, std::move(typeId)));
}

}  // namespace aoi::test
