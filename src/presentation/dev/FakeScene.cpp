#include "presentation/dev/FakeScene.h"

#include <QGraphicsScene>
#include <QPainter>
#include <algorithm>
#include <cmath>

#include "presentation/canvas/ImageItem.h"

namespace aoi::dev {
namespace {

using aoi::ui::BoxKind;
using aoi::ui::BoxView;
using aoi::ui::ComponentsSnapshot;
using aoi::ui::ComponentView;
using aoi::ui::OverlayState;

// xorshift32：結果只取決於 seed，在任何編譯器與平台都一樣。
struct Rng {
    std::uint32_t s;
    explicit Rng(unsigned seed) : s(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    double unit() { return (next() >> 8) / double(1 << 24); }
};

constexpr int kIcBoxes = 1 + 20 + 20;  // 主體 ＋ 2×10 Pin ＋ 每支 Pin 一個窗

ComponentView makeChip(int id, const QRectF& body, OverlayState state, const QString& label) {
    ComponentView c;
    c.id = id;
    c.state = state;
    c.outline = body;
    c.text = label;
    c.boxes.push_back({body, BoxKind::Body});
    return c;
}

ComponentView makeIc(int id, const QRectF& body, OverlayState state, const QString& label) {
    ComponentView c;
    c.id = id;
    c.state = state;
    c.text = label;
    c.boxes.reserve(kIcBoxes);
    c.boxes.push_back({body, BoxKind::Body});
    const double pinW = body.width() / 10.0 * 0.55;
    const double pinH = body.height() * 0.22;
    QRectF all = body;
    for (int row = 0; row < 2; ++row) {
        const double y = row == 0 ? body.top() - pinH : body.bottom();
        for (int col = 0; col < 10; ++col) {
            const double x = body.left() + (col + 0.5) * body.width() / 10.0 - pinW / 2.0;
            const QRectF pin(x, y, pinW, pinH);
            c.boxes.push_back({pin, BoxKind::Pin});
            all = all.united(pin);
        }
    }
    // 窗：Pin 的 140% × 200%，同一個中心。
    for (int i = 1; i <= 20; ++i) {
        const QRectF& pin = c.boxes[static_cast<size_t>(i)].rect;
        const double w = pin.width() * 1.4, h = pin.height() * 2.0;
        const QRectF win(pin.center().x() - w / 2.0, pin.center().y() - h / 2.0, w, h);
        c.boxes.push_back({win, BoxKind::Window});
        all = all.united(win);
    }
    c.outline = all;
    return c;
}

OverlayState pickState(int index, Rng& rng) {
    if (index == 0) return OverlayState::Sample;
    const double r = rng.unit();
    if (r < 0.04) return OverlayState::Rejected;
    if (r < 0.07) return OverlayState::Inferred;
    return OverlayState::Found;
}

}  // namespace

aoi::ImageView FakeBoard::view() const {
    aoi::ImageView v;
    v.data = pixels ? pixels->data() : nullptr;
    v.width = width;
    v.height = height;
    v.strideBytes = width * 4;
    v.format = aoi::PixelFormat::Bgrx32;
    v.owner = pixels;
    return v;
}

QImage FakeBoard::toQImage() const { return aoi::ui::ImageItem::wrap(view()); }

FakeBoard makeFakeBoard(int width, int height, unsigned seed) {
    FakeBoard board;
    board.width = width;
    board.height = height;
    board.pixels = std::make_shared<std::vector<std::uint8_t>>(static_cast<size_t>(width) * height * 4);

    // 綠底加每像素雜訊（tokens 的 overlay.board.green ＝ #1A5C3A）。雜訊讓 PNG／JPEG 不會壓得不真實地小。
    Rng rng(seed);
    std::uint8_t* p = board.pixels->data();
    const size_t count = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < count; ++i) {
        const std::uint32_t r = rng.next();
        const int n = static_cast<int>(r & 15) - 8;
        p[i * 4 + 0] = static_cast<std::uint8_t>(std::clamp(0x3A + n, 0, 255));  // B
        p[i * 4 + 1] = static_cast<std::uint8_t>(std::clamp(0x5C + n, 0, 255));  // G
        p[i * 4 + 2] = static_cast<std::uint8_t>(std::clamp(0x1A + n, 0, 255));  // R
        p[i * 4 + 3] = 0xFF;
    }

    // 細線路：水平與垂直的淺色線，間隔不規則。
    QImage canvas(p, width, height, width * 4, QImage::Format_RGB32);
    QPainter painter(&canvas);
    painter.setPen(QPen(QColor(0x2E, 0x7D, 0x4F), 2));
    for (int y = 40; y < height; y += 60 + static_cast<int>(rng.next() % 90))
        painter.drawLine(0, y, width, y);
    painter.setPen(QPen(QColor(0x2E, 0x7D, 0x4F), 1));
    for (int x = 25; x < width; x += 50 + static_cast<int>(rng.next() % 120))
        painter.drawLine(x, 0, x, height);
    return board;
}

aoi::ui::SnapshotPtr makeFakeComponents(const FakeSceneOptions& o) {
    auto snapshot = std::make_shared<ComponentsSnapshot>();
    Rng rng(o.seed * 7919u + 13u);

    const std::size_t icCount = o.layout == FakeLayout::Ics ? o.boxes / kIcBoxes : 0;
    const std::size_t chipCount = o.boxes - icCount * kIcBoxes;
    const std::size_t total = icCount + chipCount;
    if (total == 0) return snapshot;

    // 格子：依板子的長寬比排成 cols × rows，每個元件佔格子的 70%。
    const double aspect = static_cast<double>(o.width) / o.height;
    const int cols = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(total) * aspect))));
    const int rows = std::max(1, static_cast<int>((total + static_cast<std::size_t>(cols) - 1) / static_cast<std::size_t>(cols)));
    const double cellW = static_cast<double>(o.width) / cols;
    const double cellH = static_cast<double>(o.height) / rows;

    snapshot->components.reserve(total);
    for (std::size_t i = 0; i < total; ++i) {
        const int col = static_cast<int>(i % static_cast<std::size_t>(cols));
        const int row = static_cast<int>(i / static_cast<std::size_t>(cols));
        const bool ic = i < icCount;
        const OverlayState state = pickState(static_cast<int>(i), rng);
        const QString label = QStringLiteral("%1 %2").arg(ic ? QStringLiteral("8371") : QStringLiteral("5102"),
                                                           QString::number(0.60 + 0.39 * rng.unit(), 'f', 2));
        const int id = static_cast<int>(i) + 1;
        if (ic) {
            // 主體佔格子中央：寬 70%、高 36%；Pin 與窗在上下突出，剩下的空間留給它們。
            const double w = cellW * 0.70, h = cellH * 0.36;
            const QRectF body(col * cellW + (cellW - w) / 2.0, row * cellH + (cellH - h) / 2.0, w, h);
            snapshot->components.push_back(makeIc(id, body, state, label));
        } else {
            const double w = cellW * 0.50, h = cellH * 0.30;
            const QRectF body(col * cellW + (cellW - w) / 2.0, row * cellH + (cellH - h) / 2.0, w, h);
            snapshot->components.push_back(makeChip(id, body, state, label));
        }
    }
    return snapshot;
}

std::vector<aoi::ui::RoiGraphicsItem*> addPerComponentItems(QGraphicsScene& scene, aoi::ui::OverlayStyle& style,
                                                            const ComponentsSnapshot& snapshot) {
    std::vector<aoi::ui::RoiGraphicsItem*> items;
    items.reserve(snapshot.components.size());
    for (const ComponentView& c : snapshot.components) {
        auto* item = new aoi::ui::RoiGraphicsItem(style);
        item->setComponent(c);
        item->setZValue(1.5);
        scene.addItem(item);
        items.push_back(item);
    }
    return items;
}

}  // namespace aoi::dev
