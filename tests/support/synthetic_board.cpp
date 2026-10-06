#include "synthetic_board.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "domain/image_buffer.h"

namespace aoi::test {
namespace {

// xorshift32：結果只取決於 seed，在任何編譯器與平台都一樣（和 presentation/dev/FakeScene 的相同）。
struct Rng {
    std::uint32_t s;
    explicit Rng(std::uint32_t seed) : s(seed ? seed : 0x9E3779B9u) {}
    std::uint32_t next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    double unit() { return static_cast<double>(next() >> 8) / 16777216.0; }  // [0, 1)
    double symmetric() { return unit() * 2.0 - 1.0; }                       // [−1, 1)
};

constexpr int kSamples = 4;  // 每個像素 4 × 4 個取樣點
constexpr double kPadFraction = 0.22;

bool validSpec(const SyntheticBoardSpec& s) {
    const bool levels = s.backgroundLevel >= 0 && s.backgroundLevel <= 255 && s.bodyLevel >= 0 && s.bodyLevel <= 255 &&
                        s.padLevel >= 0 && s.padLevel <= 255;
    return s.width > 0 && s.height > 0 && s.width <= 20000 && s.height <= 20000 && s.rows >= 0 && s.cols >= 0 &&
           s.pitchXPx > 0.0 && s.pitchYPx > 0.0 && s.bodyWidthPx > 0.0 && s.bodyHeightPx > 0.0 &&
           s.positionJitterPx >= 0.0 && s.missingFraction >= 0.0 && s.missingFraction <= 1.0 && s.glareFraction >= 0.0 &&
           s.glareFraction <= 1.0 && s.blurRadius >= 0 && s.blurRadius <= 50 && s.noiseAmplitude >= 0 &&
           s.noiseAmplitude <= 255 && s.umPerPixel >= 0.0 && levels &&
           (s.format == PixelFormat::Gray8 || s.format == PixelFormat::Bgrx32 || s.format == PixelFormat::Gray16) &&
           s.grayBits >= 8 && s.grayBits <= 16 && std::isfinite(s.tiltDeg) &&
           std::isfinite(s.originXPx) && std::isfinite(s.originYPx);
}

// 一個元件的區域裡，像素的 4 × 4 取樣點落在哪裡。local 是元件座標（未旋轉，中心在原點）。
struct Frame {
    Pose2D pose;
    double cr = 1.0, sr = 0.0;  // 轉回元件座標用：角度 −pose.angleDeg 的 cos、sin
    explicit Frame(const Pose2D& p) : pose(p) {
        const double r = degreesToRadians(-p.angleDeg);
        cr = std::cos(r);
        sr = std::sin(r);
    }
    Point2D local(double x, double y) const {  // 與 geometry.h 的 rotate(v, −angle) 相同
        const double dx = x - pose.x;
        const double dy = y - pose.y;
        return {dx * cr + dy * sr, -dx * sr + dy * cr};
    }
};

struct Box {
    int x0, y0, x1, y1;  // 半開區間
};

Box pixelBox(const SyntheticPart& part, int width, int height) {
    const Rect r = boundingRect(RectObj{part.pose, part.widthPx, part.heightPx});
    return {std::clamp(static_cast<int>(std::floor(r.x)) - 1, 0, width), std::clamp(static_cast<int>(std::floor(r.y)) - 1, 0, height),
            std::clamp(static_cast<int>(std::ceil(r.right())) + 1, 0, width),
            std::clamp(static_cast<int>(std::ceil(r.bottom())) + 1, 0, height)};
}

void drawPart(std::vector<std::uint8_t>& gray, int width, int height, const SyntheticPart& part, const SyntheticBoardSpec& s) {
    const Frame f(part.pose);
    const double hw = part.widthPx * 0.5;
    const double hh = part.heightPx * 0.5;
    const double padStart = hw - part.widthPx * kPadFraction;
    const Box b = pixelBox(part, width, height);
    for (int y = b.y0; y < b.y1; ++y) {
        for (int x = b.x0; x < b.x1; ++x) {
            int body = 0, pad = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const Point2D p = f.local(x + (sx + 0.5) / kSamples, y + (sy + 0.5) / kSamples);
                    if (std::abs(p.x) > hw || std::abs(p.y) > hh) continue;
                    if (std::abs(p.x) >= padStart) ++pad;
                    else ++body;
                }
            }
            if (body + pad == 0) continue;
            std::uint8_t& v = gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            const int n = kSamples * kSamples;
            v = static_cast<std::uint8_t>((v * (n - body - pad) + s.bodyLevel * body + s.padLevel * pad + n / 2) / n);
        }
    }
}

// 反光：元件上偏右上的一塊橢圓，飽和到 255。
void drawGlare(std::vector<std::uint8_t>& gray, int width, int height, const SyntheticPart& part) {
    const Frame f(part.pose);
    const double cx = part.widthPx * 0.15, cy = -part.heightPx * 0.10;
    const double ax = part.widthPx * 0.30, ay = part.heightPx * 0.45;
    const Box b = pixelBox(part, width, height);
    for (int y = b.y0; y < b.y1; ++y) {
        for (int x = b.x0; x < b.x1; ++x) {
            int inside = 0;
            for (int sy = 0; sy < kSamples; ++sy) {
                for (int sx = 0; sx < kSamples; ++sx) {
                    const Point2D p = f.local(x + (sx + 0.5) / kSamples, y + (sy + 0.5) / kSamples);
                    const double u = (p.x - cx) / ax, w = (p.y - cy) / ay;
                    if (u * u + w * w <= 1.0) ++inside;
                }
            }
            if (inside == 0) continue;
            std::uint8_t& v = gray[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)];
            const int n = kSamples * kSamples;
            v = static_cast<std::uint8_t>((v * (n - inside) + 255 * inside + n / 2) / n);
        }
    }
}

// 一維方框模糊（整數、四捨五入、邊緣重複最外面的像素），running sum：每個像素 O(1)。
void boxLine(const std::uint8_t* in, std::uint8_t* out, int n, std::ptrdiff_t step, int r) {
    const int window = 2 * r + 1;
    auto at = [&](int i) { return static_cast<int>(in[static_cast<std::ptrdiff_t>(std::clamp(i, 0, n - 1)) * step]); };
    int sum = 0;
    for (int i = -r; i <= r; ++i) sum += at(i);
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::ptrdiff_t>(i) * step] = static_cast<std::uint8_t>((sum + window / 2) / window);
        sum += at(i + r + 1) - at(i - r);
    }
}

void boxBlur(std::vector<std::uint8_t>& gray, int width, int height, int r) {
    std::vector<std::uint8_t> tmp(gray.size());
    for (int y = 0; y < height; ++y) {
        const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
        boxLine(gray.data() + row, tmp.data() + row, width, 1, r);
    }
    for (int x = 0; x < width; ++x) boxLine(tmp.data() + x, gray.data() + x, height, width, r);
}

void appendNumber(std::string& out, double v) {
    char buf[64];
    const auto res = std::to_chars(buf, buf + sizeof buf, v);  // 最短、可往返的十進位
    out.append(buf, res.ptr);
}
void appendNumber(std::string& out, long long v) { out += std::to_string(v); }

}  // namespace

std::size_t SyntheticBoard::presentCount() const {
    return static_cast<std::size_t>(std::count_if(parts.begin(), parts.end(), [](const SyntheticPart& p) { return p.present; }));
}

SyntheticBoard makeSyntheticBoard(const SyntheticBoardSpec& spec) {
    SyntheticBoard board;
    board.spec = spec;
    if (!validSpec(spec)) return board;

    // 1. 格子與標準答案（亂數的順序：每格 4 個數，見標頭）
    Rng rng(spec.seed);
    const double hw = spec.bodyWidthPx * 0.5, hh = spec.bodyHeightPx * 0.5;
    for (int r = 0; r < spec.rows; ++r) {
        for (int c = 0; c < spec.cols; ++c) {
            const double jx = rng.symmetric() * spec.positionJitterPx;
            const double jy = rng.symmetric() * spec.positionJitterPx;
            const bool missing = rng.unit() < spec.missingFraction;
            const bool glare = rng.unit() < spec.glareFraction;
            const Point2D offset = rotate({c * spec.pitchXPx, r * spec.pitchYPx}, spec.tiltDeg);
            SyntheticPart part;
            part.row = r;
            part.col = c;
            part.pose = {spec.originXPx + offset.x + jx, spec.originYPx + offset.y + jy, spec.tiltDeg};
            part.widthPx = spec.bodyWidthPx;
            part.heightPx = spec.bodyHeightPx;
            part.present = !missing;
            part.glare = glare && !missing;
            // 只留整個落在影像內的格子（四個角都在 [0, width] × [0, height]）
            bool inside = true;
            for (const Point2D corner : {Point2D{-hw, -hh}, Point2D{hw, -hh}, Point2D{hw, hh}, Point2D{-hw, hh}}) {
                const Point2D q = rotate(corner, spec.tiltDeg);
                const double x = part.pose.x + q.x, y = part.pose.y + q.y;
                if (x < 0.0 || y < 0.0 || x > spec.width || y > spec.height) inside = false;
            }
            if (inside) board.parts.push_back(part);
        }
    }

    // 2. 畫：底色 → 元件 → 反光 → 模糊 → 雜訊
    std::vector<std::uint8_t> gray(static_cast<std::size_t>(spec.width) * static_cast<std::size_t>(spec.height),
                                   static_cast<std::uint8_t>(spec.backgroundLevel));
    for (const SyntheticPart& p : board.parts)
        if (p.present) drawPart(gray, spec.width, spec.height, p, spec);
    for (const SyntheticPart& p : board.parts)
        if (p.glare) drawGlare(gray, spec.width, spec.height, p);
    if (spec.blurRadius > 0)
        for (int pass = 0; pass < 3; ++pass) boxBlur(gray, spec.width, spec.height, spec.blurRadius);
    if (spec.noiseAmplitude > 0) {
        const auto span = static_cast<std::uint32_t>(2 * spec.noiseAmplitude + 1);
        for (std::uint8_t& v : gray) {
            const int n = static_cast<int>(rng.next() % span) - spec.noiseAmplitude;
            v = static_cast<std::uint8_t>(std::clamp(static_cast<int>(v) + n, 0, 255));
        }
    }

    // 3. 輸出（產生像素，不是複製，所以不計數）
    ImageBuffer buffer(spec.width, spec.height, spec.format);
    if (!buffer.valid()) return board;
    for (int y = 0; y < spec.height; ++y) {
        const std::uint8_t* src = gray.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(spec.width);
        std::uint8_t* dst = buffer.row(y);
        if (spec.format == PixelFormat::Gray8) {
            std::copy(src, src + spec.width, dst);
        } else if (spec.format == PixelFormat::Gray16) {
            const std::uint32_t maxValue = (1u << spec.grayBits) - 1u;
            for (int x = 0; x < spec.width; ++x) {
                const auto v = static_cast<std::uint16_t>((src[x] * maxValue + 127u) / 255u);  // 整數四捨五入：逐位元可重現
                std::memcpy(dst + 2 * static_cast<std::size_t>(x), &v, sizeof v);
            }
        } else {
            for (int x = 0; x < spec.width; ++x) {
                dst[4 * x + 0] = dst[4 * x + 1] = dst[4 * x + 2] = src[x];
                dst[4 * x + 3] = 255;
            }
        }
    }
    board.image = buffer.view(spec.umPerPixel);
    if (spec.format == PixelFormat::Gray16) board.image.significantBits = spec.grayBits;
    return board;
}

std::uint64_t pixelHash(const ImageView& image) {
    std::uint64_t h = 14695981039346656037ull;
    if (!image.valid()) return h;
    const auto rowBytes = static_cast<std::size_t>(image.rowBytes());
    for (int y = 0; y < image.height; ++y) {
        const std::uint8_t* row = image.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.strideBytes);
        for (std::size_t i = 0; i < rowBytes; ++i) {
            h ^= row[i];
            h *= 1099511628211ull;
        }
    }
    return h;
}

std::string groundTruthJson(const SyntheticBoard& board) {
    const SyntheticBoardSpec& s = board.spec;
    std::string o;
    o.reserve(256 + board.parts.size() * 160);
    auto key = [&](const char* k) {
        o += '"';
        o += k;
        o += "\": ";
    };
    auto num = [&](const char* k, double v, bool comma = true) {
        key(k);
        appendNumber(o, v);
        if (comma) o += ", ";
    };
    auto integer = [&](const char* k, long long v, bool comma = true) {
        key(k);
        appendNumber(o, v);
        if (comma) o += ", ";
    };
    o += "{\n  \"generator\": \"aoi-synthetic-board\",\n  \"version\": 1,\n  \"spec\": {";
    integer("width", s.width);
    integer("height", s.height);
    key("format");
    o += '"';
    o += toString(s.format);
    o += "\", ";
    integer("rows", s.rows);
    integer("cols", s.cols);
    num("pitchXPx", s.pitchXPx);
    num("pitchYPx", s.pitchYPx);
    num("originXPx", s.originXPx);
    num("originYPx", s.originYPx);
    num("tiltDeg", s.tiltDeg);
    num("bodyWidthPx", s.bodyWidthPx);
    num("bodyHeightPx", s.bodyHeightPx);
    num("positionJitterPx", s.positionJitterPx);
    num("missingFraction", s.missingFraction);
    num("glareFraction", s.glareFraction);
    integer("blurRadius", s.blurRadius);
    integer("noiseAmplitude", s.noiseAmplitude);
    integer("grayBits", s.grayBits);
    num("umPerPixel", s.umPerPixel);
    integer("seed", static_cast<long long>(s.seed));
    integer("backgroundLevel", s.backgroundLevel);
    integer("bodyLevel", s.bodyLevel);
    integer("padLevel", s.padLevel, false);
    o += "},\n  \"image\": {";
    integer("width", board.image.width);
    integer("height", board.image.height);
    num("umPerPixel", board.image.umPerPixel);
    integer("significantBits", board.image.significantBits);
    char hash[32];
    std::snprintf(hash, sizeof hash, "%016llx", static_cast<unsigned long long>(pixelHash(board.image)));
    key("pixelHash");
    o += '"';
    o += hash;
    o += "\"},\n  \"counts\": {";
    std::size_t glare = 0;
    for (const SyntheticPart& p : board.parts) glare += p.glare ? 1 : 0;
    integer("cells", static_cast<long long>(board.parts.size()));
    integer("present", static_cast<long long>(board.presentCount()));
    integer("missing", static_cast<long long>(board.parts.size() - board.presentCount()));
    integer("glare", static_cast<long long>(glare), false);
    o += "},\n  \"parts\": [";
    for (std::size_t i = 0; i < board.parts.size(); ++i) {
        const SyntheticPart& p = board.parts[i];
        o += i == 0 ? "\n    {" : ",\n    {";
        integer("row", p.row);
        integer("col", p.col);
        num("x", p.pose.x);
        num("y", p.pose.y);
        num("angleDeg", p.pose.angleDeg);
        num("widthPx", p.widthPx);
        num("heightPx", p.heightPx);
        key("present");
        o += p.present ? "true, " : "false, ";
        key("glare");
        o += p.glare ? "true}" : "false}";
    }
    o += board.parts.empty() ? "]\n}\n" : "\n  ]\n}\n";
    return o;
}

}  // namespace aoi::test
