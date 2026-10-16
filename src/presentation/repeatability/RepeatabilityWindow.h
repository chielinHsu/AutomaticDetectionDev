#pragma once

#include <QWidget>
#include <map>
#include <memory>
#include <set>
#include "presentation/repeatability/RepeatabilityPresenter.h"

namespace aoi::ui {
class RepeatabilityPlot;
class ThemeManager;
namespace Ui { class RepeatabilityWindow; }
class RepeatabilityWindow : public QWidget {
    Q_OBJECT
public:
    RepeatabilityWindow(std::shared_ptr<IRepeatabilitySource> source, ThemeManager& theme, QWidget* parent = nullptr);
    ~RepeatabilityWindow() override;
    void loadFolder(const QString& path) { presenter_->loadFolder(path); }
    RepeatabilityPresenter& presenter() { return *presenter_; }
protected:
    void changeEvent(QEvent* event) override;
private:
    void retranslate();
    void refresh();
    void refreshPlots();
    void refreshTable();
    void updateBusy();
    void editMetrics();
    void showDetails(std::size_t group, const std::string& metric, const QString& component, const QString& box);
    void exportCharts(const QString& folder);
    std::vector<std::string> selectedMetrics() const;
    repeatability::Statistic statistic() const;
    std::unique_ptr<Ui::RepeatabilityWindow> ui_;
    std::unique_ptr<RepeatabilityPresenter> presenter_;
    std::set<std::string> seen_, checked_;
    std::map<std::string, std::string> folderMetrics_;
    repeatability::BoxKey selectedBox_;
    bool hasSelectedBox_ = false;
};
}  // namespace aoi::ui
