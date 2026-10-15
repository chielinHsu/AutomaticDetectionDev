#include "application/usecases/SearchBoardWithSamples.h"

#include <algorithm>

#include "application/Error.h"

namespace aoi {

namespace {

// 第 index 個樣本（共 count 個）的進度 → 整體的進度：(index + f) ÷ count。
class ScaledProgress final : public IProgressSink {
public:
    ScaledProgress(IProgressSink* inner, std::size_t index, std::size_t count) : inner_(inner), index_(index), count_(count) {}
    void report(const ProgressEvent& event) noexcept override {
        if (inner_ == nullptr || count_ == 0) return;
        ProgressEvent scaled = event;
        const double f = std::clamp(event.fraction, 0.0, 1.0);
        scaled.fraction = (static_cast<double>(index_) + f) / static_cast<double>(count_);
        scaled.stage = Stage::Match;  // 對呼叫端來說是「第幾個樣本」的整體進度（0..1），不再分四個階段
        inner_->report(scaled);
    }

private:
    IProgressSink* inner_;
    std::size_t index_;
    std::size_t count_;
};

}  // namespace

Result<BoardProposal> searchBoardWithSamples(const GenerateObjectsFromSample& useCase, const BoardSearchRequest& request,
                                             const CancelToken& cancel, IProgressSink* progress) {
    if (!request.image.valid()) return makeError(ErrorCode::InvalidArgument, "The image is not valid.");
    if (request.samples.empty()) return makeError(ErrorCode::InvalidArgument, "There is no sample to search with.");

    std::vector<SampleRun> runs;
    runs.reserve(request.samples.size());
    for (std::size_t i = 0; i < request.samples.size(); ++i) {
        if (cancel.cancelled()) return makeError(ErrorCode::Cancelled, "The job was cancelled.");
        SampleRequest one;
        one.image = request.image;
        one.sample = request.samples[i].sample;
        one.search = request.search;
        one.options = request.options;
        one.match = request.samples[i].match;
        ScaledProgress scaled(progress, i, request.samples.size());
        Result<SampleProposal> r = useCase.run(one, cancel, progress != nullptr ? &scaled : nullptr);
        if (!r) return r.error();
        runs.push_back(SampleRun{request.samples[i].typeId, std::move(r.value())});
    }
    BoardProposal merged = mergeSampleProposals(runs, request.occupied, request.options.nms);
    if (progress != nullptr) progress->report(ProgressEvent{Stage::Match, 1.0});
    return merged;
}

}  // namespace aoi
