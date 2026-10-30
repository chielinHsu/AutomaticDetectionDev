#include "presentation/solder/SolderTeachingWindow.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <chrono>
#include "presentation/canvas/ImageCanvas.h"
#include "ui_SolderTeachingWindow.h"

namespace aoi::ui {
namespace {
QString imageFilter() { return QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff);;All files (*)"); }
}

bool SolderTeachingWindow::openImage(const QString& path) {
    if (busy()) return false;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    auto result = images_.load(absolute.toStdString());
    if (!result) { showMessage(qtTrId("socket.openFailed").arg(QString::fromStdString(result.error().message))); return false; }
    image_ = std::move(result.value());
    session_.openImage(absolute.toStdString(),image_.width,image_.height,image_.umPerPixel);
    recipePath_.clear(); selected_.clear(); lastRegion_.reset(); ++generation_;
    ui_->canvas->showImage(image_);
    ui_->canvas->setInteraction(ToolId::Region);
    refresh();
    showMessage(qtTrId(cadMode_?"socket.cadInstructions":"socket.instructions"));
    return true;
}

bool SolderTeachingWindow::openRecipe(const QString& path, const QString& replacementImage) {
    if (busy()) return false;
    recipeImageMissing_ = false;
    auto recipe = recipes_.load(QFileInfo(path).absoluteFilePath().toStdString());
    if (!recipe) { showMessage(qtTrId("socket.openFailed").arg(QString::fromStdString(recipe.error().message))); return false; }
    QString imagePath = replacementImage.isEmpty() ? QString::fromStdString(recipe.value().board.name) : replacementImage;
    if (QFileInfo(imagePath).isRelative()) imagePath = QFileInfo(path).dir().absoluteFilePath(imagePath);
    auto image = images_.load(imagePath.toStdString());
    if (!image) { recipeImageMissing_ = true; showMessage(qtTrId("socket.imageMissing")); return false; }
    if (image.value().width != recipe.value().board.imageWidth || image.value().height != recipe.value().board.imageHeight) {
        showMessage(qtTrId("socket.sizeMismatch")); return false;
    }
    recipe.value().board.name = QFileInfo(imagePath).absoluteFilePath().toStdString();
    auto loaded = session_.load(std::move(recipe.value()));
    if (!loaded) { showMessage(qtTrId("socket.openFailed").arg(QString::fromStdString(loaded.error().message))); return false; }
    image_ = std::move(image.value());
    image_.umPerPixel = session_.recipe().board.umPerPixel;
    image_.significantBits = session_.recipe().board.significantBits;
    recipePath_ = QFileInfo(path).absoluteFilePath(); selected_.clear(); lastRegion_.reset(); ++generation_;
    ui_->canvas->showImage(image_); ui_->canvas->setInteraction(ToolId::Select); refresh();
    showMessage(qtTrId("socket.loaded").arg(session_.recipe().board.components.size()));
    return true;
}

SolderDetectionOptions SolderTeachingWindow::options() const {
    SolderDetectionOptions o;
    o.threshold = ui_->threshold->value(); o.darkForeground = ui_->darkForeground->isChecked();
    o.minArea = ui_->minArea->value(); o.maxArea = ui_->maxArea->value(); o.padding = ui_->padding->value();
    return o;
}

void SolderTeachingWindow::detectRegion(const Rect& region) {
    if (cadMode_) { registerCadRegion(region); return; }
    if (busy() || !image_.valid()) return;
    if (!detector_) { showMessage(qtTrId("socket.noDetector")); return; }
    const SolderDetectionOptions settings = options();
    if (settings.maxArea < settings.minArea) { showMessage(qtTrId("socket.invalidArea")); return; }
    lastRegion_ = region;
    const ImageView image = image_;
    const auto detector = detector_;
    const auto generation = ++generation_;
    auto result = std::make_shared<std::optional<Result<std::vector<Rect>>>>();
    detecting_ = true; setBusyUi(); showMessage(qtTrId("socket.detecting"));
    runner_.submit([detector,image,region,settings,result](const CancelToken& cancel) {
        result->emplace(detector->detect(image,region,settings,cancel));
    }, [this,poster=poster_,generation,region,result](const JobReport& report) {
        poster->post([this,generation,region,result,report] {
            if (generation != generation_ || !detecting_) return;
            detecting_ = false;
            bool success = false;
            if (report.status == JobStatus::Failed) showMessage(qtTrId("socket.failed").arg(QString::fromStdString(report.failure)));
            else if (!*result) showMessage(qtTrId("socket.cancelled"));
            else if (!**result) showMessage(qtTrId("socket.failed").arg(QString::fromStdString((*result)->error().message)));
            else if ((*result)->value().empty()) showMessage(qtTrId("socket.empty"));
            else if (session_.replaceRegion(region,(*result)->value())) {
                success = true; selected_.clear(); ui_->canvas->setInteraction(ToolId::Select);
                showMessage(qtTrId("socket.detected").arg((*result)->value().size()).arg(report.runMs,0,'f',1));
            } else showMessage(qtTrId("socket.invalidBox"));
            ui_->canvas->showRegionDraft(std::nullopt,DraftMode::Region); refresh(); emit detectionFinished(success);
        });
    });
}

void SolderTeachingWindow::cancelDetection() {
    if (!detecting_) return;
    ++generation_; runner_.cancel(); detecting_ = false;
    ui_->canvas->showRegionDraft(std::nullopt,DraftMode::Region);
    setBusyUi(); showMessage(qtTrId("socket.cancelled")); emit detectionFinished(false);
}

void SolderTeachingWindow::saveRecipe(const QString& path) {
    if (busy() || !image_.valid() || path.isEmpty()) return;
    const QString absolute = QFileInfo(path).absoluteFilePath();
    const auto revision = session_.revision();
    saving_ = true; setBusyUi(); showMessage(qtTrId("socket.saving"));
    saver_.save(session_.recipe(),absolute.toStdString(),[this,poster=poster_,absolute,revision](const RecipeSaver::Outcome& outcome) {
        const Result<void> result = outcome.result;
        poster->post([this,absolute,revision,result] {
            saving_ = false;
            if (result) {
                recipePath_ = absolute;
                if (session_.revision() == revision) session_.markSaved();
                showMessage(qtTrId("socket.saved").arg(absolute));
            } else showMessage(qtTrId("socket.failed").arg(QString::fromStdString(result.error().message)));
            refresh(); emit recipeSaved(result.ok());
        });
    });
}

void SolderTeachingWindow::promptImage() {
    if (!confirmDiscard()) return;
    const auto path = QFileDialog::getOpenFileName(this,qtTrId("socket.openImage"),{},imageFilter());
    if (!path.isEmpty()) openImage(path);
}
void SolderTeachingWindow::promptRecipe() {
    if (!confirmDiscard()) return;
    const auto path = QFileDialog::getOpenFileName(this,qtTrId("socket.openRecipe"),{},QStringLiteral("AOI recipe (*.json)"));
    if (path.isEmpty() || openRecipe(path) || !recipeImageMissing_) return;
    // 配方與原圖分開保存；搬移後允許使用者明確指定替代原圖，尺寸仍必須吻合。
    const auto replacement = QFileDialog::getOpenFileName(this,qtTrId("socket.locateImage"),{},imageFilter());
    if (!replacement.isEmpty()) openRecipe(path,replacement);
}
void SolderTeachingWindow::promptSave() {
    const auto path = QFileDialog::getSaveFileName(this,qtTrId("socket.save"),recipePath_.isEmpty() ?
        QStringLiteral("socket-recipe.json") : recipePath_,QStringLiteral("AOI recipe (*.json)"));
    if (!path.isEmpty()) saveRecipe(path.endsWith(QLatin1String(".json"),Qt::CaseInsensitive) ? path : path+QStringLiteral(".json"));
}
}  // namespace aoi::ui
