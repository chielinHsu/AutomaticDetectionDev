#pragma once

#include <QDialog>
#include <memory>
#include "presentation/repeatability/RepeatabilityPresenter.h"

namespace aoi::ui {
namespace Ui { class RepeatabilityComparisonDialog; }
class RepeatabilityComparisonDialog : public QDialog {
    Q_OBJECT
public:
    RepeatabilityComparisonDialog(std::shared_ptr<const RepeatabilityState> snapshot,
                                  const std::vector<std::string>& metrics, QWidget* parent = nullptr);
    ~RepeatabilityComparisonDialog() override;
private:
    void refreshCharts();
    std::shared_ptr<const RepeatabilityState> snapshot_;
    std::unique_ptr<Ui::RepeatabilityComparisonDialog> ui_;
};
}  // namespace aoi::ui
