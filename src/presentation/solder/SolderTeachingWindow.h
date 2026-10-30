#pragma once

#include <QMainWindow>
#include <memory>
#include <optional>
#include "application/document/RecipeSaver.h"
#include "application/interaction/SolderTeachingSession.h"
#include "application/jobs/JobRunner.h"
#include "application/ports/IImageSource.h"
#include "application/ports/ISolderRegionDetector.h"
#include "application/ports/ISocketSampleSearcher.h"
#include "application/ports/ISocketCadGenerator.h"
#include "presentation/canvas/ICanvasController.h"
#include "presentation/presenters/UiPoster.h"

namespace aoi::ui {
class ImageCanvas;
class ThemeManager;
namespace Ui { class SolderTeachingWindow; }

// Qt 接線與背景工作；幾何、資料修改及復原規則在 SolderTeachingSession。
class SolderTeachingWindow final : public QMainWindow, public ICanvasController,
                                   public ISelectionController, public IKeyboardController {
    Q_OBJECT
public:
    SolderTeachingWindow(IImageSource& images, IRecipeStore& recipes, std::shared_ptr<const ISolderRegionDetector> detector,
                         IExecutor& executor, ThemeManager& theme, QWidget* parent = nullptr);
    ~SolderTeachingWindow() override;
    bool openImage(const QString& path);
    bool openRecipe(const QString& path, const QString& replacementImage = {});
    void saveRecipe(const QString& path);
    void detectRegion(const Rect& region);
    void cancelDetection();
    void setSampleSearcher(std::shared_ptr<const ISocketSampleSearcher> searcher);
    void searchWholeImage();
    void setCadGenerator(std::shared_ptr<const ISocketCadGenerator> generator);
    void registerCadRegion(const Rect& region);
    bool busy() const { return detecting_ || saving_; }
    const SolderTeachingSession& session() const { return session_; }
    ImageCanvas* canvas() const;

    void onRegionDrawn(const QRectF& rect, DraftMode mode) override;
    void onRegionAborted() override;
    void onAddRequested(const QPointF& point) override;
    void onComponentsMoved(const std::vector<int>& ids, const QPointF& delta) override;
    void onOpenTypeEditor(int id, const QPointF& point) override;
    void onComponentResized(int id, const QRectF& rect) override;
    void select(const std::vector<int>& ids) override;
    void deleteSelected() override;
    void selectAll() override;
    void selectSameType() override;
    bool onKeyPressed(int key, Qt::KeyboardModifiers modifiers) override;

signals:
    void detectionFinished(bool success);
    void recipeSaved(bool success);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildConnections(ThemeManager& theme);
    void refresh();
    void refreshSelection();
    void setBusyUi();
    void showMessage(const QString& message);
    bool confirmDiscard();
    void promptImage();
    void promptRecipe();
    void promptSave();
    void applyGeometry();
    SolderDetectionOptions options() const;

    std::unique_ptr<Ui::SolderTeachingWindow> ui_;
    IImageSource& images_;
    IRecipeStore& recipes_;
    std::shared_ptr<const ISolderRegionDetector> detector_;
    std::shared_ptr<const ISocketSampleSearcher> sampleSearcher_;
    std::shared_ptr<const ISocketCadGenerator> cadGenerator_;
    std::shared_ptr<UiPoster> poster_;
    JobRunner runner_;
    RecipeSaver saver_;
    SolderTeachingSession session_;
    ImageView image_;
    std::vector<int> selected_;
    std::optional<Rect> lastRegion_;
    QString recipePath_;
    bool detecting_ = false;
    bool saving_ = false;
    bool recipeImageMissing_ = false;
    bool cadMode_ = false;
    std::uint64_t generation_ = 0;
};
}  // namespace aoi::ui
