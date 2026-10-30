#include "presentation/solder/SolderTeachingWindow.h"

#include "presentation/canvas/ImageCanvas.h"
#include "ui_SolderTeachingWindow.h"

namespace aoi::ui {
void SolderTeachingWindow::setSampleSearcher(std::shared_ptr<const ISocketSampleSearcher> searcher) {
    if (busy()) return;
    sampleSearcher_ = std::move(searcher);
    setBusyUi();
}

void SolderTeachingWindow::searchWholeImage() {
    if (busy() || !image_.valid()) return;
    if (cadMode_) { registerCadRegion({0,0,static_cast<double>(image_.width),static_cast<double>(image_.height)}); return; }
    if (!sampleSearcher_) { showMessage(qtTrId("socket.sampleUnavailable")); return; }
    const auto refs = session_.samples();
    if (refs.empty()) { showMessage(qtTrId("socket.needSamples")); return; }
    SocketSampleRequest request{image_,refs,session_.recipe().params.minScore};
    const auto engine = sampleSearcher_;
    const auto generation = ++generation_;
    auto result = std::make_shared<std::optional<Result<std::vector<SocketSampleMatch>>>>();
    detecting_ = true;
    setBusyUi();
    showMessage(qtTrId("socket.sampleSearching").arg(refs.size()));
    runner_.submit([engine,request,result](const CancelToken& cancel) {
        result->emplace(engine->search(request,cancel));
    },[this,poster=poster_,generation,result](const JobReport& report) {
        poster->post([this,generation,result,report] {
            if (generation != generation_ || !detecting_) return;
            detecting_ = false;
            bool success = false;
            if (report.status == JobStatus::Failed) showMessage(qtTrId("socket.failed").arg(QString::fromStdString(report.failure)));
            else if (!*result) showMessage(qtTrId("socket.cancelled"));
            else if (!**result) showMessage(qtTrId("socket.failed").arg(QString::fromStdString((*result)->error().message)));
            else if ((*result)->value().empty()) showMessage(qtTrId("socket.sampleEmpty"));
            else if (session_.replaceMatches((*result)->value())) {
                success = true;
                selected_.clear();
                ui_->canvas->setInteraction(ToolId::Select);
                showMessage(qtTrId("socket.sampleFound").arg((*result)->value().size()).arg(report.runMs,0,'f',1));
            } else showMessage(qtTrId("socket.invalidBox"));
            refresh();
            emit detectionFinished(success);
        });
    });
}
}  // namespace aoi::ui
