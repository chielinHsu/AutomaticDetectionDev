#pragma once

#include <memory>
#include <vector>

#include "application/usecases/GenerateObjectsFromSample.h"

// 文件、提案與工作階段的測試共用：直接組一份 SampleProposal（不跑 use case），分數由測試指定。
namespace aoi::test {

// 候選順序：Sample、各個分數的 Seed（水平排開，間隔 50）、可選的 Predicted。
// 編號（classify 依順序編 1..N）：Sample ＝ 1，scores[i] ＝ i + 2，Predicted 接在後面。
inline std::shared_ptr<const SampleProposal> makeProposal(const std::vector<double>& scores, std::size_t predicted = 0,
                                                          Size2D part = {40, 20}) {
    auto p = std::make_shared<SampleProposal>();
    p->partSize = part;
    Candidate sample;
    sample.pose = {100, 100, 0};
    sample.score = 1.0;
    sample.source = CandidateSource::Sample;
    p->candidates.push_back(sample);
    double x = 150;
    for (double s : scores) {
        Candidate c;
        c.pose = {x, 100, 0};
        c.score = s;
        c.source = CandidateSource::Seed;
        p->candidates.push_back(c);
        x += 50;
    }
    for (std::size_t i = 0; i < predicted; ++i) {
        Candidate c;
        c.pose = {x, 100, 0};
        c.score = 0.0;
        c.source = CandidateSource::Predicted;
        p->candidates.push_back(c);
        x += 50;
    }
    return p;
}

// 很多候選的提案（效能與 I6 用）：n 個 Seed，排成方格，分數在 0.5–1.0 之間決定性地變化。
inline std::shared_ptr<const SampleProposal> makeBigProposal(std::size_t n, Size2D part = {24, 16}) {
    auto p = std::make_shared<SampleProposal>();
    p->partSize = part;
    Candidate sample;
    sample.pose = {20, 20, 0};
    sample.score = 1.0;
    sample.source = CandidateSource::Sample;
    p->candidates.push_back(sample);
    const std::size_t columns = 70;
    for (std::size_t i = 0; i < n; ++i) {
        Candidate c;
        c.pose = {20.0 + static_cast<double>((i + 1) % columns) * 36.0, 20.0 + static_cast<double>((i + 1) / columns) * 24.0, 0};
        c.score = 0.50 + 0.50 * static_cast<double>((i * 2654435761u) % 1000u) / 1000.0;
        c.source = CandidateSource::Seed;
        p->candidates.push_back(c);
    }
    return p;
}

}  // namespace aoi::test
