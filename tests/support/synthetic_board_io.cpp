#include "synthetic_board_io.h"

#include <QDir>
#include <QFile>
#include <QImage>

#include "infrastructure/image_qt/QtImages.h"

namespace aoi::test {

QString saveSyntheticBoard(const SyntheticBoard& board, const QString& dir, const QString& name) {
    if (!board.image.valid()) return QStringLiteral("the board has no image (invalid spec)");
    if (!QDir().mkpath(dir)) return QStringLiteral("cannot create %1").arg(dir);
    const QImage image = infra::wrapAsQImage(board.image);  // 零複製
    const QString png = QDir(dir).filePath(name + QStringLiteral(".png"));
    if (!image.save(png, "PNG")) return QStringLiteral("cannot write %1").arg(png);
    QFile json(QDir(dir).filePath(name + QStringLiteral(".json")));
    if (!json.open(QIODevice::WriteOnly)) return QStringLiteral("cannot write %1").arg(json.fileName());
    const std::string text = groundTruthJson(board);
    if (json.write(text.data(), static_cast<qint64>(text.size())) != static_cast<qint64>(text.size())) {
        return QStringLiteral("cannot write %1").arg(json.fileName());
    }
    return {};
}

}  // namespace aoi::test
