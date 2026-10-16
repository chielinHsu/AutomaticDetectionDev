#pragma once

#include <QString>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

#include "domain/image_buffer.h"
#include "domain/recipe.h"

// 配方與樣本庫的測試共用（S06）：一份「每個欄位都有不同值」的配方，以及逐欄位比較（浮點數逐位元）。
namespace aoi::test {

// 有紋理的參考影像；padded ＝ true 時是子影像（stride 比一列大），驗證存檔會把列緊密排好。
inline ImageView referenceImage(PixelFormat format, int w, int h, double umPerPixel, bool padded) {
    ImageBuffer buffer(w + (padded ? 3 : 0), h + (padded ? 1 : 0), format);
    for (int y = 0; y < buffer.height(); ++y)
        for (int i = 0; i < buffer.strideBytes(); ++i) buffer.row(y)[i] = static_cast<std::uint8_t>(i * 31 + y * 7 + 1);
    const ImageView whole = buffer.view(umPerPixel);
    return padded ? subView(whole, 1, 1, w, h) : whole;
}

inline ComponentType richType(const std::string& id, int variant) {
    ComponentType t;
    t.id = id;
    t.libId = variant == 0 ? "" : "lib-" + std::to_string(variant);
    t.partNumber = variant == 0 ? "" : "PN-" + std::to_string(8370 + variant);
    t.aliases = variant == 0 ? std::vector<std::string>{} : std::vector<std::string>{"別名 " + std::to_string(variant), "SOP20"};
    t.name = "類型 " + id;
    t.category = static_cast<ComponentCategory>(variant % 3);

    BoxGroup body;
    body.id = 1;
    body.kind = BoxGroupKind::Body;
    body.widthUm = 6400.125 + variant;
    body.heightUm = 1.0 / 3.0;
    BoxGroup pin;
    pin.id = 2;
    pin.kind = BoxGroupKind::Pin;
    pin.widthUm = 450.5;
    pin.heightUm = 1200.0;
    pin.centerXUm = -0.1;
    pin.centerYUm = -2600.0;
    pin.matrix.rows = 2;
    pin.matrix.cols = 10 + variant;
    pin.matrix.pitchXUm = 1270.0;
    pin.matrix.pitchYUm = 5200.0;
    pin.matrix.skipped = {{0, 3}, {1, 9}};
    BoxGroup window;
    window.id = 3;
    window.kind = BoxGroupKind::Window;
    window.window.parentGroupId = 2;
    window.window.offsetXRatio = 0.1;
    window.window.offsetYRatio = -0.25;
    window.window.widthRatio = 1.4;
    window.window.heightRatio = 2.0;
    window.window.algo = variant % 2 ? "Solder" : "Void";
    t.groups = {body, pin, window};
    const PixelFormat formats[] = {PixelFormat::Gray8, PixelFormat::Bgrx32, PixelFormat::Gray16, PixelFormat::Rgb24, PixelFormat::Bgr24};
    if (variant > 0) t.referenceImage = referenceImage(formats[variant % 5], 13 + variant, 7, variant * 2.5, variant % 2 == 1);
    if (t.referenceImage.format == PixelFormat::Gray16) t.referenceImage.significantBits = 12;  // 12 bit 的相機影像
    // 比對區（ADR-028，配方 v3）：variant 0 是預設（整框、灰階），1 是矩形＋邊緣，2 起是遮罩。
    if (variant == 1) {
        t.match.mode = MatchMode::Edges;
        t.match.region = MatchRegion{125.5, -1.0 / 3.0, 900.25, 1500.0, {}};
    } else if (variant >= 2) {
        MatchRegion r{-10.125 * variant, 2.5, 12.5, 7.5, {}};
        r.mask.width = 5;
        r.mask.height = 3;
        r.mask.runs = {{0, 1, 4}, {1, 0, 5}, {2, 1, 2}, {2, 3, 4}};
        t.match.region = r;
    }
    return t;
}

// components 個元件，輪流用三個類型、四種狀態、兩種來源、兩面；數字故意用不好表示的值（1/3、1e-300…）。
inline Recipe richRecipe(int components = 12) {
    Recipe r;
    r.board.name = "板子 A — 測試.png";
    r.board.imageWidth = 2592;
    r.board.imageHeight = 1944;
    r.board.umPerPixel = 12.345678901234567;
    r.board.significantBits = 12;  // schema v2（S07）
    r.board.types = {richType("T0", 0), richType("T1", 1), richType("T2", 2), richType("T3", 3), richType("T4", 4), richType("T5", 5)};
    const double tricky[] = {0.1, 1.0 / 3.0, 1e-300, -123456.789012345678, 5e300, 2.5, 4096.0, std::numeric_limits<double>::denorm_min()};
    for (int i = 0; i < components; ++i) {
        ComponentInstance c;
        c.id = i * 3 + 1;
        c.name = (i % 2 ? "R" : "U") + std::to_string(i + 1);
        c.pose = {100.0 + i * 37.5 + i * 0.1, tricky[i % 8], -179.999 + i % 360};
        c.side = i % 4 == 3 ? ComponentSide::Bottom : ComponentSide::Top;
        c.typeId = r.board.types[static_cast<std::size_t>(i % 6)].id;
        c.state = static_cast<ObjectState>(i % 4);
        c.origin = i % 5 == 4 ? ObjectOrigin::Manual : ObjectOrigin::Auto;
        if (c.origin == ObjectOrigin::Auto && c.state != ObjectState::Inferred) c.score = 0.3 + (i % 70) / 100.0;
        r.board.components.push_back(c);
    }
    r.params.minScore = 0.65;
    r.params.angleMinDeg = -7.5;
    r.params.angleMaxDeg = 12.25;
    r.params.maxMatches = 123456;
    return r;
}

inline bool sameBits(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }

// 兩張影像的內容（格式、大小、校正值、每列的像素；不比 stride 與 owner）。
inline QString diffImage(const ImageView& a, const ImageView& b, const QString& where) {
    if (a.valid() != b.valid()) return where + QStringLiteral(": one image is missing");
    if (!a.valid()) return {};
    if (a.format != b.format || a.width != b.width || a.height != b.height) return where + QStringLiteral(": format or size");
    if (!sameBits(a.umPerPixel, b.umPerPixel)) return where + QStringLiteral(".umPerPixel");
    if (a.significantBits != b.significantBits) return where + QStringLiteral(".significantBits");
    for (int y = 0; y < a.height; ++y) {
        if (std::memcmp(a.data + static_cast<std::size_t>(y) * a.strideBytes, b.data + static_cast<std::size_t>(y) * b.strideBytes,
                        static_cast<std::size_t>(a.rowBytes())) != 0) {
            return where + QStringLiteral(": pixels of row %1").arg(y);
        }
    }
    return {};
}

inline QString diffType(const ComponentType& a, const ComponentType& b, const QString& where) {
    if (a.id != b.id || a.libId != b.libId || a.partNumber != b.partNumber || a.aliases != b.aliases || a.name != b.name ||
        a.category != b.category)
        return where + QStringLiteral(": identity");
    if (a.groups.size() != b.groups.size()) return where + QStringLiteral(": group count");
    for (std::size_t i = 0; i < a.groups.size(); ++i) {
        const BoxGroup& g = a.groups[i];
        const BoxGroup& h = b.groups[i];
        const QString at = where + QStringLiteral(".groups[%1]").arg(i);
        if (g.id != h.id || g.kind != h.kind || !sameBits(g.widthUm, h.widthUm) || !sameBits(g.heightUm, h.heightUm) ||
            !sameBits(g.centerXUm, h.centerXUm) || !sameBits(g.centerYUm, h.centerYUm))
            return at;
        if (g.matrix.rows != h.matrix.rows || g.matrix.cols != h.matrix.cols || !sameBits(g.matrix.pitchXUm, h.matrix.pitchXUm) ||
            !sameBits(g.matrix.pitchYUm, h.matrix.pitchYUm) || g.matrix.skipped.size() != h.matrix.skipped.size())
            return at + QStringLiteral(".matrix");
        for (std::size_t k = 0; k < g.matrix.skipped.size(); ++k)
            if (g.matrix.skipped[k].row != h.matrix.skipped[k].row || g.matrix.skipped[k].col != h.matrix.skipped[k].col)
                return at + QStringLiteral(".matrix.skipped");
        const WindowSpec& w = g.window;
        const WindowSpec& v = h.window;
        if (w.parentGroupId != v.parentGroupId || !sameBits(w.offsetXRatio, v.offsetXRatio) || !sameBits(w.offsetYRatio, v.offsetYRatio) ||
            !sameBits(w.widthRatio, v.widthRatio) || !sameBits(w.heightRatio, v.heightRatio) || w.algo != v.algo)
            return at + QStringLiteral(".window");
    }
    if (!(a.match == b.match)) return where + QStringLiteral(".match");
    return diffImage(a.referenceImage, b.referenceImage, where + QStringLiteral(".referenceImage"));
}

// 第一個不同的地方；完全相同時回傳空字串。
inline QString diffRecipe(const Recipe& a, const Recipe& b) {
    const Board& x = a.board;
    const Board& y = b.board;
    if (x.name != y.name || x.imageWidth != y.imageWidth || x.imageHeight != y.imageHeight || !sameBits(x.umPerPixel, y.umPerPixel) ||
        x.significantBits != y.significantBits)
        return QStringLiteral("board header");
    if (x.types.size() != y.types.size()) return QStringLiteral("type count");
    for (std::size_t i = 0; i < x.types.size(); ++i) {
        const QString d = diffType(x.types[i], y.types[i], QStringLiteral("types[%1]").arg(i));
        if (!d.isEmpty()) return d;
    }
    if (x.components.size() != y.components.size()) return QStringLiteral("component count");
    for (std::size_t i = 0; i < x.components.size(); ++i) {
        const ComponentInstance& c = x.components[i];
        const ComponentInstance& d = y.components[i];
        if (c.id != d.id || c.name != d.name || !sameBits(c.pose.x, d.pose.x) || !sameBits(c.pose.y, d.pose.y) ||
            !sameBits(c.pose.angleDeg, d.pose.angleDeg) || c.side != d.side || c.typeId != d.typeId || c.state != d.state ||
            c.origin != d.origin || c.score.has_value() != d.score.has_value() || (c.score && !sameBits(*c.score, *d.score)))
            return QStringLiteral("components[%1]").arg(i);
    }
    const SearchParams& p = a.params;
    const SearchParams& q = b.params;
    if (!sameBits(p.minScore, q.minScore) || !sameBits(p.angleMinDeg, q.angleMinDeg) || !sameBits(p.angleMaxDeg, q.angleMaxDeg) ||
        p.maxMatches != q.maxMatches)
        return QStringLiteral("params");
    return {};
}

}  // namespace aoi::test
