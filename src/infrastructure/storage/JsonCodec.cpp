#include "infrastructure/storage/JsonCodec.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QJsonArray>
#include <QString>
#include <array>
#include <cmath>
#include <cstring>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "domain/image_buffer.h"
#include "domain/image_depth.h"

namespace aoi::infra::json {
namespace {

constexpr double kMaxExactInteger = 9007199254740992.0;  // 2^53：double 能精確表示的最大整數

template <class E>
struct Name {
    E value;
    const char* text;
};

constexpr std::array<Name<ComponentCategory>, 3> kCategories{{
    {ComponentCategory::Chip, "chip"}, {ComponentCategory::IC, "ic"}, {ComponentCategory::Other, "other"}}};
constexpr std::array<Name<ComponentSide>, 2> kSides{{{ComponentSide::Top, "top"}, {ComponentSide::Bottom, "bottom"}}};
constexpr std::array<Name<BoxGroupKind>, 3> kKinds{{
    {BoxGroupKind::Body, "body"}, {BoxGroupKind::Pin, "pin"}, {BoxGroupKind::Window, "window"}}};
constexpr std::array<Name<ObjectState>, 4> kStates{{{ObjectState::Sample, "sample"},
                                                    {ObjectState::Found, "found"},
                                                    {ObjectState::Inferred, "inferred"},
                                                    {ObjectState::Rejected, "rejected"}}};
constexpr std::array<Name<ObjectOrigin>, 2> kOrigins{{{ObjectOrigin::Auto, "auto"}, {ObjectOrigin::Manual, "manual"}}};
constexpr std::array<Name<MatchMode>, 2> kModes{{{MatchMode::Intensity, "intensity"}, {MatchMode::Edges, "edges"}}};
// 遮罩格子的邊長上限：損壞或惡意的檔案不能要求巨大的配置（4096² ＝ 16 MB 的位元）。
constexpr int kMaxMaskSide = 4096;
constexpr std::array<Name<PixelFormat>, 5> kFormats{{{PixelFormat::Gray8, "Gray8"},
                                                     {PixelFormat::Rgb24, "Rgb24"},
                                                     {PixelFormat::Bgrx32, "Bgrx32"},
                                                     {PixelFormat::Bgr24, "Bgr24"},
                                                     {PixelFormat::Gray16, "Gray16"}}};

template <class E, std::size_t N>
QString nameOf(const std::array<Name<E>, N>& table, E value) {
    for (const Name<E>& n : table) {
        if (n.value == value) return QString::fromLatin1(n.text);
    }
    return QStringLiteral("?");
}

std::string at(const std::string& path, const char* key) { return path + "." + key; }
std::string at(const std::string& path, qsizetype index) { return path + "[" + std::to_string(index) + "]"; }

// ===================================================================================================
// 寫
// ===================================================================================================

class Encoder {
public:
    QJsonValue number(double v, const std::string& path) {
        if (!std::isfinite(v)) fail(path + " is not a finite number");
        return std::isfinite(v) ? QJsonValue(v) : QJsonValue(0.0);
    }
    QJsonValue text(const std::string& s, const std::string& path) {
        if (!QByteArrayView(s.data(), static_cast<qsizetype>(s.size())).isValidUtf8()) fail(path + " is not valid UTF-8");
        return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
    }
    QJsonValue integer(std::size_t v, const std::string& path) {
        if (static_cast<double>(v) > kMaxExactInteger) fail(path + " is too large to store exactly");
        return static_cast<double>(v);
    }
    void fail(std::string why) {
        if (!error_) error_ = std::move(why);
    }
    bool ok() const { return !error_.has_value(); }
    Error error() const { return makeError(ErrorCode::InvalidArgument, "Cannot save: " + error_.value_or("?") + "."); }

private:
    std::optional<std::string> error_;
};

QJsonObject encodePose(Encoder& e, const Pose2D& p, const std::string& path) {
    QJsonObject o;
    o.insert(QStringLiteral("x"), e.number(p.x, at(path, "x")));
    o.insert(QStringLiteral("y"), e.number(p.y, at(path, "y")));
    o.insert(QStringLiteral("angleDeg"), e.number(p.angleDeg, at(path, "angleDeg")));
    return o;
}

QJsonObject encodeGroup(Encoder& e, const BoxGroup& g, const std::string& path) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), g.id);
    o.insert(QStringLiteral("kind"), nameOf(kKinds, g.kind));
    o.insert(QStringLiteral("widthUm"), e.number(g.widthUm, at(path, "widthUm")));
    o.insert(QStringLiteral("heightUm"), e.number(g.heightUm, at(path, "heightUm")));
    o.insert(QStringLiteral("centerXUm"), e.number(g.centerXUm, at(path, "centerXUm")));
    o.insert(QStringLiteral("centerYUm"), e.number(g.centerYUm, at(path, "centerYUm")));

    QJsonObject m;
    m.insert(QStringLiteral("rows"), g.matrix.rows);
    m.insert(QStringLiteral("cols"), g.matrix.cols);
    m.insert(QStringLiteral("pitchXUm"), e.number(g.matrix.pitchXUm, at(path, "matrix.pitchXUm")));
    m.insert(QStringLiteral("pitchYUm"), e.number(g.matrix.pitchYUm, at(path, "matrix.pitchYUm")));
    QJsonArray skipped;
    for (const MatrixCell& c : g.matrix.skipped) skipped.append(QJsonObject{{QStringLiteral("row"), c.row}, {QStringLiteral("col"), c.col}});
    m.insert(QStringLiteral("skipped"), skipped);
    o.insert(QStringLiteral("matrix"), m);

    QJsonObject w;
    w.insert(QStringLiteral("parentGroupId"), g.window.parentGroupId);
    w.insert(QStringLiteral("offsetXRatio"), e.number(g.window.offsetXRatio, at(path, "window.offsetXRatio")));
    w.insert(QStringLiteral("offsetYRatio"), e.number(g.window.offsetYRatio, at(path, "window.offsetYRatio")));
    w.insert(QStringLiteral("widthRatio"), e.number(g.window.widthRatio, at(path, "window.widthRatio")));
    w.insert(QStringLiteral("heightRatio"), e.number(g.window.heightRatio, at(path, "window.heightRatio")));
    w.insert(QStringLiteral("algo"), e.text(g.window.algo, at(path, "window.algo")));
    o.insert(QStringLiteral("window"), w);
    return o;
}

QJsonValue encodeImageWith(Encoder& e, const ImageView& image, const std::string& path) {
    if (!image.valid()) return QJsonValue(QJsonValue::Null);
    const auto rowBytes = static_cast<std::size_t>(image.rowBytes());
    const std::size_t bytes = rowBytes * static_cast<std::size_t>(image.height);
    if (image.width > kMaxReferenceSide || image.height > kMaxReferenceSide || bytes > kMaxReferenceBytes) {
        e.fail(path + " is too large for a reference image");
        return QJsonValue(QJsonValue::Null);
    }
    QByteArray packed(static_cast<qsizetype>(bytes), Qt::Uninitialized);  // 序列化，不算影像複製（image_copy.h）
    for (int y = 0; y < image.height; ++y) {
        std::memcpy(packed.data() + static_cast<std::size_t>(y) * rowBytes,
                    image.data + static_cast<std::size_t>(y) * static_cast<std::size_t>(image.strideBytes), rowBytes);
    }
    QJsonObject o;
    o.insert(QStringLiteral("width"), image.width);
    o.insert(QStringLiteral("height"), image.height);
    o.insert(QStringLiteral("format"), nameOf(kFormats, image.format));
    o.insert(QStringLiteral("umPerPixel"), e.number(image.umPerPixel, at(path, "umPerPixel")));
    if (image.significantBits < 0 || image.significantBits > 16) e.fail(path + ".significantBits is out of range");
    o.insert(QStringLiteral("significantBits"), image.significantBits);
    o.insert(QStringLiteral("encoding"), QStringLiteral("base64"));
    o.insert(QStringLiteral("data"), QString::fromLatin1(packed.toBase64()));
    return o;
}

// 比對區（ADR-028；配方 v3、樣本庫 v2）：{"mode", "region": null | {centerXUm, centerYUm, widthUm, heightUm,
// "mask": null | {"width", "height", "runs": [[row, colBegin, colEnd], ...]}}}。runs 用 JSON 陣列（看得懂、diff 得出來）。
QJsonObject encodeMatchWith(Encoder& e, const MatchSettings& m, const std::string& path) {
    QJsonObject o;
    o.insert(QStringLiteral("mode"), nameOf(kModes, m.mode));
    if (!m.region) {
        o.insert(QStringLiteral("region"), QJsonValue(QJsonValue::Null));
        return o;
    }
    const MatchRegion& r = *m.region;
    const std::string rPath = at(path, "region");
    QJsonObject ro;
    ro.insert(QStringLiteral("centerXUm"), e.number(r.centerXUm, at(rPath, "centerXUm")));
    ro.insert(QStringLiteral("centerYUm"), e.number(r.centerYUm, at(rPath, "centerYUm")));
    ro.insert(QStringLiteral("widthUm"), e.number(r.widthUm, at(rPath, "widthUm")));
    ro.insert(QStringLiteral("heightUm"), e.number(r.heightUm, at(rPath, "heightUm")));
    if (r.mask.empty()) {
        ro.insert(QStringLiteral("mask"), QJsonValue(QJsonValue::Null));
    } else {
        if (r.mask.width > kMaxMaskSide || r.mask.height > kMaxMaskSide) e.fail(rPath + ".mask is too large");
        QJsonObject mo;
        mo.insert(QStringLiteral("width"), r.mask.width);
        mo.insert(QStringLiteral("height"), r.mask.height);
        QJsonArray runs;
        for (const MaskRun& run : r.mask.runs) runs.append(QJsonArray{run.row, run.colBegin, run.colEnd});
        mo.insert(QStringLiteral("runs"), runs);
        ro.insert(QStringLiteral("mask"), mo);
    }
    o.insert(QStringLiteral("region"), ro);
    return o;
}

QJsonObject encodeTypeWith(Encoder& e, const ComponentType& t, const std::string& path) {
    QJsonObject o;
    o.insert(QStringLiteral("id"), e.text(t.id, at(path, "id")));
    o.insert(QStringLiteral("libId"), e.text(t.libId, at(path, "libId")));
    o.insert(QStringLiteral("partNumber"), e.text(t.partNumber, at(path, "partNumber")));
    QJsonArray aliases;
    for (std::size_t i = 0; i < t.aliases.size(); ++i)
        aliases.append(e.text(t.aliases[i], at(at(path, "aliases"), static_cast<qsizetype>(i))));
    o.insert(QStringLiteral("aliases"), aliases);
    o.insert(QStringLiteral("name"), e.text(t.name, at(path, "name")));
    o.insert(QStringLiteral("category"), nameOf(kCategories, t.category));
    QJsonArray groups;
    for (std::size_t i = 0; i < t.groups.size(); ++i)
        groups.append(encodeGroup(e, t.groups[i], at(at(path, "groups"), static_cast<qsizetype>(i))));
    o.insert(QStringLiteral("groups"), groups);
    o.insert(QStringLiteral("referenceImage"), encodeImageWith(e, t.referenceImage, at(path, "referenceImage")));
    o.insert(QStringLiteral("match"), encodeMatchWith(e, t.match, at(path, "match")));
    return o;
}

// ===================================================================================================
// 讀
// ===================================================================================================

// 記下第一個錯誤；之後的讀取回傳預設值，呼叫端在適當的地方看 failed() 提早結束。
class Decoder {
public:
    bool failed() const { return error_.has_value(); }
    Error error() const { return makeError(ErrorCode::FormatInvalid, error_.value_or("?")); }
    void fail(const std::string& path, const std::string& what) {
        if (!error_) error_ = path + " " + what;
    }

    QJsonObject object(const QJsonValue& v, const std::string& path) {
        if (!v.isObject()) {
            fail(path, "must be an object");
            return {};
        }
        return v.toObject();
    }
    QJsonValue field(const QJsonObject& o, const char* key, const std::string& path) {
        const auto it = o.constFind(QLatin1StringView(key));
        if (it == o.constEnd()) {
            fail(at(path, key), "is missing");
            return {};
        }
        return *it;
    }
    QJsonArray array(const QJsonObject& o, const char* key, const std::string& path) {
        const QJsonValue v = field(o, key, path);
        if (failed()) return {};
        if (!v.isArray()) {
            fail(at(path, key), "must be an array");
            return {};
        }
        return v.toArray();
    }
    double numberValue(const QJsonValue& v, const std::string& path) {
        if (!v.isDouble()) {
            fail(path, "must be a number");
            return 0.0;
        }
        const double d = v.toDouble();
        if (!std::isfinite(d)) fail(path, "must be a finite number");
        return d;
    }
    double number(const QJsonObject& o, const char* key, const std::string& path) {
        const QJsonValue v = field(o, key, path);
        return failed() ? 0.0 : numberValue(v, at(path, key));
    }
    double integerValue(const QJsonValue& v, const std::string& path, double min, double max) {
        const double d = numberValue(v, path);
        if (failed()) return 0.0;
        if (std::floor(d) != d) fail(path, "must be a whole number");
        else if (d < min || d > max) fail(path, "is out of range");
        return d;
    }
    int integer(const QJsonObject& o, const char* key, const std::string& path, int min, int max) {
        const QJsonValue v = field(o, key, path);
        return failed() ? 0 : static_cast<int>(integerValue(v, at(path, key), min, max));
    }
    std::string textValue(const QJsonValue& v, const std::string& path) {
        if (!v.isString()) {
            fail(path, "must be a string");
            return {};
        }
        return v.toString().toUtf8().toStdString();
    }
    std::string text(const QJsonObject& o, const char* key, const std::string& path) {
        const QJsonValue v = field(o, key, path);
        return failed() ? std::string() : textValue(v, at(path, key));
    }
    template <class E, std::size_t N>
    E enumeration(const QJsonObject& o, const char* key, const std::string& path, const std::array<Name<E>, N>& table) {
        const std::string s = text(o, key, path);
        if (failed()) return table[0].value;
        for (const Name<E>& n : table) {
            if (s == n.text) return n.value;
        }
        fail(at(path, key), "has an unknown value '" + s + "'");
        return table[0].value;
    }

private:
    std::optional<std::string> error_;
};

constexpr int kMaxInt = 2147483647;

Pose2D decodePose(Decoder& d, const QJsonValue& v, const std::string& path) {
    const QJsonObject o = d.object(v, path);
    Pose2D p;
    p.x = d.number(o, "x", path);
    p.y = d.number(o, "y", path);
    p.angleDeg = d.number(o, "angleDeg", path);
    return p;
}

BoxGroup decodeGroup(Decoder& d, const QJsonValue& v, const std::string& path) {
    BoxGroup g;
    const QJsonObject o = d.object(v, path);
    if (d.failed()) return g;
    g.id = d.integer(o, "id", path, -kMaxInt, kMaxInt);
    g.kind = d.enumeration(o, "kind", path, kKinds);
    g.widthUm = d.number(o, "widthUm", path);
    g.heightUm = d.number(o, "heightUm", path);
    g.centerXUm = d.number(o, "centerXUm", path);
    g.centerYUm = d.number(o, "centerYUm", path);

    const std::string mPath = at(path, "matrix");
    const QJsonObject m = d.object(d.field(o, "matrix", path), mPath);
    if (d.failed()) return g;
    g.matrix.rows = d.integer(m, "rows", mPath, 0, 100000);
    g.matrix.cols = d.integer(m, "cols", mPath, 0, 100000);
    g.matrix.pitchXUm = d.number(m, "pitchXUm", mPath);
    g.matrix.pitchYUm = d.number(m, "pitchYUm", mPath);
    const QJsonArray skipped = d.array(m, "skipped", mPath);
    for (qsizetype i = 0; i < skipped.size() && !d.failed(); ++i) {
        const std::string cPath = at(at(mPath, "skipped"), i);
        const QJsonObject c = d.object(skipped.at(i), cPath);
        MatrixCell cell;
        cell.row = d.integer(c, "row", cPath, 0, 100000);
        cell.col = d.integer(c, "col", cPath, 0, 100000);
        g.matrix.skipped.push_back(cell);
    }

    const std::string wPath = at(path, "window");
    const QJsonObject w = d.object(d.field(o, "window", path), wPath);
    if (d.failed()) return g;
    g.window.parentGroupId = d.integer(w, "parentGroupId", wPath, -kMaxInt, kMaxInt);
    g.window.offsetXRatio = d.number(w, "offsetXRatio", wPath);
    g.window.offsetYRatio = d.number(w, "offsetYRatio", wPath);
    g.window.widthRatio = d.number(w, "widthRatio", wPath);
    g.window.heightRatio = d.number(w, "heightRatio", wPath);
    g.window.algo = d.text(w, "algo", wPath);
    return g;
}

ImageView decodeImageWith(Decoder& d, const QJsonValue& v, const std::string& path) {
    if (v.isNull()) return {};
    const QJsonObject o = d.object(v, path);
    if (d.failed()) return {};
    const int width = d.integer(o, "width", path, 1, kMaxReferenceSide);
    const int height = d.integer(o, "height", path, 1, kMaxReferenceSide);
    const PixelFormat format = d.enumeration(o, "format", path, kFormats);
    const double umPerPixel = d.number(o, "umPerPixel", path);
    const int significantBits = d.integer(o, "significantBits", path, 0, 16);
    const std::string encoding = d.text(o, "encoding", path);
    const std::string data = d.text(o, "data", path);
    if (d.failed()) return {};
    if (encoding != "base64") {
        d.fail(at(path, "encoding"), "has an unknown value '" + encoding + "'");
        return {};
    }
    if (umPerPixel < 0.0) {
        d.fail(at(path, "umPerPixel"), "must not be negative");
        return {};
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(bytesPerPixel(format));
    const std::size_t expected = rowBytes * static_cast<std::size_t>(height);
    if (expected > kMaxReferenceBytes) {
        d.fail(path, "is too large for a reference image");
        return {};
    }

    const QByteArray::FromBase64Result decoded =
        QByteArray::fromBase64Encoding(QByteArray::fromStdString(data), QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) {
        d.fail(at(path, "data"), "is not valid base64");
        return {};
    }
    // 不壓縮：Qt 6.8.0 的 qUncompress 遇到被截斷的資料會撞上 Q_UNREACHABLE（Debug 中止、Release 是未定義行為；
    // S06 的 tst_recipe_store 在 Debug 抓到），損壞的檔案不可以走到那裡。參考影像是樣本的小裁切，大小可以接受（ADR-015）。
    const QByteArray& packed = decoded.decoded;
    if (static_cast<std::size_t>(packed.size()) != expected) {
        d.fail(at(path, "data"), "does not match the image size");
        return {};
    }
    ImageBuffer buffer(width, height, format);
    if (!buffer.valid()) {
        d.fail(path, "needs more memory than is available");
        return {};
    }
    std::memcpy(buffer.data(), packed.constData(), expected);  // ImageBuffer 是緊密排列的，一次複製整塊（解碼，不算影像複製）
    ImageView view = buffer.view(umPerPixel);
    view.significantBits = significantBits;
    return view;
}

// 嚴格：欄位都要有；遮罩的格子 1..kMaxMaskSide、每段在格子內、同一列依序不重疊。違反就 FormatInvalid（帶路徑）。
MatchSettings decodeMatchWith(Decoder& d, const QJsonValue& v, const std::string& path) {
    MatchSettings m;
    const QJsonObject o = d.object(v, path);
    if (d.failed()) return m;
    m.mode = d.enumeration(o, "mode", path, kModes);
    const QJsonValue regionValue = d.field(o, "region", path);
    if (d.failed() || regionValue.isNull()) return m;
    const std::string rPath = at(path, "region");
    const QJsonObject ro = d.object(regionValue, rPath);
    if (d.failed()) return m;
    MatchRegion r;
    r.centerXUm = d.number(ro, "centerXUm", rPath);
    r.centerYUm = d.number(ro, "centerYUm", rPath);
    r.widthUm = d.number(ro, "widthUm", rPath);
    r.heightUm = d.number(ro, "heightUm", rPath);
    if (d.failed()) return m;
    if (!(r.widthUm > 0.0) || !(r.heightUm > 0.0)) {
        d.fail(rPath, "must have a positive size");
        return m;
    }
    const QJsonValue maskValue = d.field(ro, "mask", rPath);
    if (d.failed()) return m;
    if (!maskValue.isNull()) {
        const std::string mPath = at(rPath, "mask");
        const QJsonObject mo = d.object(maskValue, mPath);
        if (d.failed()) return m;
        r.mask.width = d.integer(mo, "width", mPath, 1, kMaxMaskSide);
        r.mask.height = d.integer(mo, "height", mPath, 1, kMaxMaskSide);
        const QJsonArray runs = d.array(mo, "runs", mPath);
        if (d.failed()) return m;
        if (runs.isEmpty()) {
            d.fail(at(mPath, "runs"), "must not be empty");
            return m;
        }
        int lastRow = -1;
        int lastEnd = 0;
        for (qsizetype i = 0; i < runs.size() && !d.failed(); ++i) {
            const std::string runPath = at(at(mPath, "runs"), i);
            const QJsonValue rv = runs.at(i);
            if (!rv.isArray() || rv.toArray().size() != 3) {
                d.fail(runPath, "must be [row, colBegin, colEnd]");
                break;
            }
            const QJsonArray a = rv.toArray();
            MaskRun run;
            run.row = static_cast<int>(d.integerValue(a.at(0), runPath + "[0]", 0, r.mask.height - 1));
            run.colBegin = static_cast<int>(d.integerValue(a.at(1), runPath + "[1]", 0, r.mask.width - 1));
            run.colEnd = static_cast<int>(d.integerValue(a.at(2), runPath + "[2]", 1, r.mask.width));
            if (d.failed()) break;
            if (run.colEnd <= run.colBegin) d.fail(runPath, "must have colBegin < colEnd");
            else if (run.row < lastRow || (run.row == lastRow && run.colBegin < lastEnd)) d.fail(runPath, "overlaps or is out of order");
            lastRow = run.row;
            lastEnd = run.colEnd;
            r.mask.runs.push_back(run);
        }
    }
    m.region = std::move(r);
    return m;
}

ComponentType decodeTypeWith(Decoder& d, const QJsonValue& v, const std::string& path) {
    ComponentType t;
    const QJsonObject o = d.object(v, path);
    if (d.failed()) return t;
    t.id = d.text(o, "id", path);
    t.libId = d.text(o, "libId", path);
    t.partNumber = d.text(o, "partNumber", path);
    const QJsonArray aliases = d.array(o, "aliases", path);
    for (qsizetype i = 0; i < aliases.size() && !d.failed(); ++i)
        t.aliases.push_back(d.textValue(aliases.at(i), at(at(path, "aliases"), i)));
    t.name = d.text(o, "name", path);
    t.category = d.enumeration(o, "category", path, kCategories);
    const QJsonArray groups = d.array(o, "groups", path);
    for (qsizetype i = 0; i < groups.size() && !d.failed(); ++i)
        t.groups.push_back(decodeGroup(d, groups.at(i), at(at(path, "groups"), i)));
    if (d.failed()) return t;
    t.referenceImage = decodeImageWith(d, d.field(o, "referenceImage", path), at(path, "referenceImage"));
    if (d.failed()) return t;
    t.match = decodeMatchWith(d, d.field(o, "match", path), at(path, "match"));
    return t;
}

ComponentInstance decodeComponent(Decoder& d, const QJsonValue& v, const std::string& path) {
    ComponentInstance c;
    const QJsonObject o = d.object(v, path);
    if (d.failed()) return c;
    c.id = d.integer(o, "id", path, -kMaxInt, kMaxInt);
    c.name = d.text(o, "name", path);
    c.pose = decodePose(d, d.field(o, "pose", path), at(path, "pose"));
    c.side = d.enumeration(o, "side", path, kSides);
    c.typeId = d.text(o, "typeId", path);
    c.state = d.enumeration(o, "state", path, kStates);
    const QJsonValue score = d.field(o, "score", path);
    if (!d.failed() && !score.isNull()) c.score = d.numberValue(score, at(path, "score"));
    c.origin = d.enumeration(o, "origin", path, kOrigins);
    return c;
}

}  // namespace

// ===================================================================================================

Result<QJsonValue> encodeImage(const ImageView& image) {
    Encoder e;
    QJsonValue v = encodeImageWith(e, image, "image");
    if (!e.ok()) return e.error();
    return v;
}

Result<QJsonObject> encodeComponentType(const ComponentType& type) {
    Encoder e;
    QJsonObject o = encodeTypeWith(e, type, "type");
    if (!e.ok()) return e.error();
    return o;
}

Result<QJsonObject> encodeSearchParams(const SearchParams& p) {
    Encoder e;
    QJsonObject o;
    o.insert(QStringLiteral("minScore"), e.number(p.minScore, "params.minScore"));
    o.insert(QStringLiteral("angleMinDeg"), e.number(p.angleMinDeg, "params.angleMinDeg"));
    o.insert(QStringLiteral("angleMaxDeg"), e.number(p.angleMaxDeg, "params.angleMaxDeg"));
    o.insert(QStringLiteral("maxMatches"), e.integer(p.maxMatches, "params.maxMatches"));
    if (!e.ok()) return e.error();
    return o;
}

Result<QJsonObject> encodeBoard(const Board& board) {
    Encoder e;
    std::set<std::string> typeIds;
    for (const ComponentType& t : board.types) {
        if (!typeIds.insert(t.id).second) e.fail("board.types has the id '" + t.id + "' twice");
    }
    std::set<int> componentIds;
    for (const ComponentInstance& c : board.components) {
        if (!componentIds.insert(c.id).second) e.fail("board.components has the id " + std::to_string(c.id) + " twice");
        if (typeIds.count(c.typeId) == 0) e.fail("component " + std::to_string(c.id) + " refers to the type '" + c.typeId + "', which is not on the board");
    }
    if (board.imageWidth < 0 || board.imageHeight < 0) e.fail("board image size is negative");
    if (board.umPerPixel < 0.0) e.fail("board.umPerPixel is negative");
    if (!validSignificantBitsSetting(board.significantBits)) e.fail("board.significantBits must be 0 or 8..16");

    QJsonObject o;
    o.insert(QStringLiteral("name"), e.text(board.name, "board.name"));
    o.insert(QStringLiteral("imageWidth"), board.imageWidth);
    o.insert(QStringLiteral("imageHeight"), board.imageHeight);
    o.insert(QStringLiteral("umPerPixel"), e.number(board.umPerPixel, "board.umPerPixel"));
    o.insert(QStringLiteral("significantBits"), board.significantBits);  // schema v2（S07）
    QJsonArray types;
    for (std::size_t i = 0; i < board.types.size(); ++i)
        types.append(encodeTypeWith(e, board.types[i], at("board.types", static_cast<qsizetype>(i))));
    o.insert(QStringLiteral("types"), types);

    QJsonArray components;
    for (std::size_t i = 0; i < board.components.size(); ++i) {
        const ComponentInstance& c = board.components[i];
        const std::string path = at("board.components", static_cast<qsizetype>(i));
        QJsonObject co;
        co.insert(QStringLiteral("id"), c.id);
        co.insert(QStringLiteral("name"), e.text(c.name, at(path, "name")));
        co.insert(QStringLiteral("pose"), encodePose(e, c.pose, at(path, "pose")));
        co.insert(QStringLiteral("side"), nameOf(kSides, c.side));
        co.insert(QStringLiteral("typeId"), e.text(c.typeId, at(path, "typeId")));
        co.insert(QStringLiteral("state"), nameOf(kStates, c.state));
        co.insert(QStringLiteral("score"), c.score ? e.number(*c.score, at(path, "score")) : QJsonValue(QJsonValue::Null));
        co.insert(QStringLiteral("origin"), nameOf(kOrigins, c.origin));
        components.append(co);
    }
    o.insert(QStringLiteral("components"), components);
    if (!e.ok()) return e.error();
    return o;
}

QJsonArray withDefaultMatch(const QJsonArray& types) {
    QJsonArray out;
    for (const QJsonValue& v : types) {
        if (!v.isObject() || v.toObject().contains(QStringLiteral("match"))) {
            out.append(v);
            continue;
        }
        QJsonObject t = v.toObject();
        t.insert(QStringLiteral("match"), QJsonObject{{QStringLiteral("mode"), QStringLiteral("intensity")},
                                                      {QStringLiteral("region"), QJsonValue(QJsonValue::Null)}});
        out.append(t);
    }
    return out;
}

Result<ImageView> decodeImage(const QJsonValue& value, const std::string& where) {
    Decoder d;
    ImageView image = decodeImageWith(d, value, where);
    if (d.failed()) return d.error();
    return image;
}

Result<ComponentType> decodeComponentType(const QJsonValue& value, const std::string& where) {
    Decoder d;
    ComponentType t = decodeTypeWith(d, value, where);
    if (d.failed()) return d.error();
    return t;
}

Result<SearchParams> decodeSearchParams(const QJsonValue& value, const std::string& where) {
    Decoder d;
    SearchParams p;
    const QJsonObject o = d.object(value, where);
    p.minScore = d.number(o, "minScore", where);
    p.angleMinDeg = d.number(o, "angleMinDeg", where);
    p.angleMaxDeg = d.number(o, "angleMaxDeg", where);
    const QJsonValue maxMatches = d.field(o, "maxMatches", where);
    if (!d.failed()) p.maxMatches = static_cast<std::size_t>(d.integerValue(maxMatches, at(where, "maxMatches"), 0.0, kMaxExactInteger));
    if (d.failed()) return d.error();
    return p;
}

Result<Board> decodeBoard(const QJsonValue& value, const std::string& where) {
    Decoder d;
    Board b;
    const QJsonObject o = d.object(value, where);
    if (d.failed()) return d.error();
    b.name = d.text(o, "name", where);
    b.imageWidth = d.integer(o, "imageWidth", where, 0, kMaxInt);
    b.imageHeight = d.integer(o, "imageHeight", where, 0, kMaxInt);
    b.umPerPixel = d.number(o, "umPerPixel", where);
    if (!d.failed() && b.umPerPixel < 0.0) d.fail(at(where, "umPerPixel"), "must not be negative");
    b.significantBits = d.integer(o, "significantBits", where, 0, 16);  // schema v2（S07）；v1 由遷移補 0
    if (!d.failed() && !validSignificantBitsSetting(b.significantBits)) d.fail(at(where, "significantBits"), "must be 0 or 8..16");

    const QJsonArray types = d.array(o, "types", where);
    std::set<std::string> typeIds;
    for (qsizetype i = 0; i < types.size() && !d.failed(); ++i) {
        const std::string path = at(at(where, "types"), i);
        b.types.push_back(decodeTypeWith(d, types.at(i), path));
        if (!d.failed() && !typeIds.insert(b.types.back().id).second) d.fail(at(path, "id"), "is used twice");
    }

    const QJsonArray components = d.array(o, "components", where);
    std::set<int> componentIds;
    b.components.reserve(static_cast<std::size_t>(components.size()));
    for (qsizetype i = 0; i < components.size() && !d.failed(); ++i) {
        const std::string path = at(at(where, "components"), i);
        b.components.push_back(decodeComponent(d, components.at(i), path));
        if (d.failed()) break;
        const ComponentInstance& c = b.components.back();
        if (!componentIds.insert(c.id).second) d.fail(at(path, "id"), "is used twice");
        else if (typeIds.count(c.typeId) == 0) d.fail(at(path, "typeId"), "refers to '" + c.typeId + "', which is not in board.types");
    }
    if (d.failed()) return d.error();
    return b;
}

}  // namespace aoi::infra::json
