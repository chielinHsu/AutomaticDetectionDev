#include "presentation/solder/SolderTeachingWindow.h"
#include "presentation/canvas/ImageCanvas.h"
#include "ui_SolderTeachingWindow.h"

namespace aoi::ui {
void SolderTeachingWindow::setCadGenerator(std::shared_ptr<const ISocketCadGenerator> generator) {
    if (busy()) return;
    cadMode_=true; cadGenerator_=std::move(generator);
    ui_->sampleGroup->hide(); ui_->cadGroup->show();
    ui_->hint->setText(qtTrId("socket.cadInstructions"));
    ui_->actionRegion->setText(qtTrId("socket.cadRegion"));
    ui_->actionSearch->setText(qtTrId("socket.cadWhole"));
    ui_->searchButton->setText(qtTrId("socket.cadWhole"));
    ui_->table->setHorizontalHeaderLabels({qtTrId("socket.cadKind"),qtTrId("socket.box"),qtTrId("socket.cadQuality"),
        QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("W"),QStringLiteral("H")});
    refresh(); showMessage(qtTrId("socket.cadInstructions"));
}

void SolderTeachingWindow::registerCadRegion(const Rect& region) {
    if (busy() || !image_.valid()) return;
    if (!cadGenerator_) { showMessage(qtTrId("socket.cadUnavailable")); return; }
    SocketCadRequest request; request.image=image_; request.region=region; request.snapPositions=ui_->cadSnap->isChecked();
    if (ui_->cadAuto->isChecked()) request.frameSize=session_.cadFrameSize().value_or(Size2D{});
    else request.frameSize={ui_->cadWidth->value(),ui_->cadHeight->value()};
    const auto engine=cadGenerator_; const auto generation=++generation_;
    auto result=std::make_shared<std::optional<Result<SocketCadResult>>>();
    detecting_=true; setBusyUi(); showMessage(qtTrId("socket.cadWorking"));
    runner_.submit([engine,request,result](const CancelToken& cancel) {
        result->emplace(engine->generate(request,cancel));
    },[this,poster=poster_,generation,result](const JobReport& report) {
        poster->post([this,generation,result,report] {
            if (generation!=generation_ || !detecting_) return;
            detecting_=false; bool success=false;
            if (report.status==JobStatus::Failed) showMessage(qtTrId("socket.failed").arg(QString::fromStdString(report.failure)));
            else if (!*result) showMessage(qtTrId("socket.cancelled"));
            else if (!**result) showMessage(qtTrId("socket.failed").arg(QString::fromStdString((*result)->error().message)));
            else if ((*result)->value().placements.empty()) showMessage(qtTrId("socket.cadEmpty"));
            else if (session_.replaceCadRegion((*result)->value())) {
                success=true; selected_.clear();
                const auto& cad=(*result)->value();
                ui_->cadWidth->setValue(cad.profiles.front().bodyPx.w);
                ui_->cadHeight->setValue(cad.profiles.front().bodyPx.h);
                ui_->canvas->setInteraction(ToolId::Select);
                showMessage(qtTrId("socket.cadFound").arg(cad.placements.size()).arg(cad.profiles.size()).arg(report.runMs,0,'f',1));
            } else showMessage(qtTrId("socket.invalidBox"));
            ui_->canvas->showRegionDraft(std::nullopt,DraftMode::Region);
            refresh(); emit detectionFinished(success);
        });
    });
}
}  // namespace aoi::ui
