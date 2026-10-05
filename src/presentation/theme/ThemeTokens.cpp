#include "presentation/theme/ThemeTokens.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

namespace aoi::ui {
namespace {

constexpr const char* kStateKeys[kOverlayStateCount] = {"sample", "found", "inferred", "rejected"};

// 解析過程中的錯誤累積器：第一個錯誤就夠了，記下路徑方便找。
struct Reader {
    QString error;

    bool failed() const { return !error.isEmpty(); }
    void fail(const QString& path, const QString& what) {
        if (error.isEmpty()) error = path + ": " + what;
    }

    QJsonObject object(const QJsonObject& parent, const char* key, const QString& path) {
        const QJsonValue v = parent.value(QLatin1String(key));
        if (!v.isObject()) {
            fail(path + "." + key, "object expected");
            return {};
        }
        return v.toObject();
    }
    QColor color(const QJsonObject& parent, const char* key, const QString& path) {
        const QString text = parent.value(QLatin1String(key)).toString();
        const QColor c(text);
        if (!c.isValid()) fail(path + "." + key, "color expected, got '" + text + "'");
        return c;
    }
    double number(const QJsonObject& parent, const char* key, const QString& path) {
        const QJsonValue v = parent.value(QLatin1String(key));
        if (!v.isDouble()) {
            fail(path + "." + key, "number expected");
            return 0;
        }
        return v.toDouble();
    }
    StrokeColors strokeColors(const QJsonObject& parent, const char* key, const QString& path) {
        const QJsonObject o = object(parent, key, path);
        const QString p = path + "." + key;
        return {color(o, "stroke", p), color(o, "casing", p)};
    }
    PerState<StrokeColors> stateColors(const QJsonObject& state, const QString& path) {
        PerState<StrokeColors> out;
        for (int i = 0; i < kOverlayStateCount; ++i) out[static_cast<size_t>(i)] = strokeColors(state, kStateKeys[i], path);
        return out;
    }
    ThemeColors themeColors(const QJsonObject& theme, const QString& path) {
        ThemeColors t;
        const QJsonObject s = object(theme, "surface", path);
        const QString sp = path + ".surface";
        t.surface = {color(s, "window", sp),    color(s, "panel", sp),  color(s, "canvas", sp),
                     color(s, "text", sp),      color(s, "textMuted", sp), color(s, "border", sp),
                     color(s, "accent", sp),    color(s, "onAccent", sp),  color(s, "focusRing", sp)};
        t.state = stateColors(object(theme, "state", path), path + ".state");
        return t;
    }
};

}  // namespace

ThemeTokens ThemeTokens::parse(const QByteArray& json) {
    ThemeTokens t;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (!doc.isObject()) {
        t.error = "tokens.json: " + parseError.errorString();
        return t;
    }
    const QJsonObject root = doc.object();
    Reader r;

    const QJsonObject themes = r.object(root, "themes", "tokens");
    t.light = r.themeColors(r.object(themes, "light", "themes"), "themes.light");
    t.dark = r.themeColors(r.object(themes, "dark", "themes"), "themes.dark");

    const QJsonObject ov = r.object(root, "overlay", "tokens");
    t.overlay.state = r.stateColors(r.object(ov, "state", "overlay"), "overlay.state");
    const QJsonObject board = r.object(ov, "board", "overlay");
    t.overlay.boardGreen = r.color(board, "green", "overlay.board");
    t.overlay.boardBlue = r.color(board, "blue", "overlay.board");
    t.overlay.boardBlack = r.color(board, "black", "overlay.board");
    const QJsonObject region = r.object(ov, "region", "overlay");
    t.overlay.regionStroke = r.color(region, "stroke", "overlay.region");
    t.overlay.regionCasing = r.color(region, "casing", "overlay.region");
    t.overlay.regionFillOpacity = r.number(region, "fillOpacity", "overlay.region");
    t.overlay.regionWidth = r.number(region, "width", "overlay.region");
    const QJsonObject def = r.object(ov, "definition", "overlay");
    t.overlay.defStroke = r.color(def, "stroke", "overlay.definition");
    t.overlay.defCasing = r.color(def, "casing", "overlay.definition");
    t.overlay.defWidthBody = r.number(def, "widthBody", "overlay.definition");
    t.overlay.defWidthPin = r.number(def, "widthPin", "overlay.definition");
    t.overlay.defPinFillOpacity = r.number(def, "pinFillOpacity", "overlay.definition");

    const QJsonObject ss = r.object(root, "stateStyle", "tokens");
    for (int i = 0; i < kOverlayStateCount; ++i) {
        const QJsonObject o = r.object(ss, kStateKeys[i], "stateStyle");
        LineSpec& spec = t.line[static_cast<size_t>(i)];
        spec.width = r.number(o, "width", "stateStyle");
        for (const QJsonValue& d : o.value("dash").toArray()) spec.dashPx.append(d.toDouble());
        spec.cap = o.value("cap").toString() == "round" ? Qt::RoundCap : Qt::FlatCap;
        spec.marker = o.value("marker").toString();
    }
    t.selectedWidthDelta = r.number(r.object(ss, "selected", "stateStyle"), "widthDelta", "stateStyle.selected");
    t.casingExtraWidth = r.number(ss, "casingExtraWidth", "stateStyle");

    const QJsonObject sz = r.object(root, "sizes", "tokens");
    t.sizes.handleVisual = r.number(sz, "handleVisual", "sizes");
    t.sizes.handleHit = r.number(sz, "handleHit", "sizes");
    t.sizes.handlesMoveOutsideBelow = r.number(sz, "handlesMoveOutsideBelow", "sizes");
    t.sizes.toolbarButton = static_cast<int>(r.number(sz, "toolbarButton", "sizes"));
    t.sizes.statusBarHeight = static_cast<int>(r.number(sz, "statusBarHeight", "sizes"));
    t.sizes.dockMinWidth = static_cast<int>(r.number(sz, "dockMinWidth", "sizes"));
    t.sizes.messageBarHeight = static_cast<int>(r.number(sz, "messageBarHeight", "sizes"));
    const QJsonArray wm = sz.value("windowMin").toArray();
    if (wm.size() != 2) r.fail("sizes.windowMin", "[width, height] expected");
    else t.sizes.windowMin = QSize(wm.at(0).toInt(), wm.at(1).toInt());

    const QJsonObject in = r.object(root, "interaction", "tokens");
    t.interaction.zoomStepPerNotch = r.number(in, "zoomStepPerNotch", "interaction");
    t.interaction.zoomMinPercent = r.number(in, "zoomMinPercent", "interaction");
    t.interaction.zoomMaxPercent = r.number(in, "zoomMaxPercent", "interaction");
    t.interaction.fitMarginPx = r.number(in, "fitMarginPx", "interaction");
    t.interaction.minSampleSidePx = r.number(in, "minSampleSidePx", "interaction");

    const QJsonObject tm = r.object(root, "timing", "tokens");
    t.timing.visualFeedbackMax = static_cast<int>(r.number(tm, "visualFeedbackMax", "timing"));
    t.timing.busyAfter = static_cast<int>(r.number(tm, "busyAfter", "timing"));
    t.timing.busyMinShow = static_cast<int>(r.number(tm, "busyMinShow", "timing"));
    t.timing.progressAfter = static_cast<int>(r.number(tm, "progressAfter", "timing"));
    t.timing.cancelToIdleMax = static_cast<int>(r.number(tm, "cancelToIdleMax", "timing"));
    t.timing.paramDebounce = static_cast<int>(r.number(tm, "paramDebounce", "timing"));
    t.timing.toast = static_cast<int>(r.number(tm, "toast", "timing"));

    const QJsonObject font = r.object(root, "font", "tokens");
    t.fontBasePt = r.number(font, "basePt", "font");
    for (const QJsonValue& v : font.value("scaleOptions").toArray()) t.fontScaleOptions.append(v.toDouble());
    if (t.fontScaleOptions.isEmpty()) r.fail("font.scaleOptions", "non-empty array expected");

    if (r.failed()) {
        t.error = r.error;
        return t;
    }
    t.valid = true;
    return t;
}

const ThemeTokens& ThemeTokens::builtin() {
    static const ThemeTokens tokens = [] {
        QFile f(QStringLiteral(":/aoi/ux/tokens.json"));
        if (!f.open(QIODevice::ReadOnly)) qFatal("tokens.json resource is missing");
        ThemeTokens t = parse(f.readAll());
        if (!t.valid) qFatal("tokens.json is invalid: %s", qPrintable(t.error));
        return t;
    }();
    return tokens;
}

}  // namespace aoi::ui
