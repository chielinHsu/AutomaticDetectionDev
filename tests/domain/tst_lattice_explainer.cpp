// S05 說明頁（docs/steps/S05-lattice/explainer.html）的資料：由 C++ 的晶格估計真的算出來，不是頁面自己算的。
//
// 兩種用法：
//   平常（test.ps1）：重新算一遍，和頁面裡 <script id="lattice-data"> 的內容比對；不一樣就失敗
//                  （演算法改了而頁面沒更新）。座標四捨五入到 0.1 像素，比對時容許 0.11 的差（Debug／Release 的最後一位可能不同）。
//   更新頁面：     設環境變數 AOI_EXPORT_EXPLAINER=1 再執行，把新的資料寫進頁面。
#include <QtTest>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <cmath>
#include <vector>

#include "LatticeTestSupport.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_predictor.h"

using namespace aoi;
using namespace aoi::test;

namespace {

const QString kPage = QStringLiteral(AOI_SOURCE_DIR "/docs/steps/S05-lattice/explainer.html");
const QString kOpen = QStringLiteral("<script type=\"application/json\" id=\"lattice-data\">");
const QString kClose = QStringLiteral("</script>");
const Rect kBoard{0.0, 0.0, 640.0, 420.0};  // 說明頁的畫面範圍（影像像素）

double r1(double v) { return std::round(v * 10.0) / 10.0; }
double r3(double v) { return std::round(v * 1000.0) / 1000.0; }

QString reasonName(NoLatticeReason r) {
    switch (r) {
        case NoLatticeReason::None: return QStringLiteral("none");
        case NoLatticeReason::TooFewPoints: return QStringLiteral("tooFewPoints");
        case NoLatticeReason::NoDirection: return QStringLiteral("noDirection");
        case NoLatticeReason::NoConsistentLattice: return QStringLiteral("noConsistentLattice");
        case NoLatticeReason::TooManyHoles: return QStringLiteral("tooManyHoles");
        case NoLatticeReason::LowConfidence: return QStringLiteral("lowConfidence");
    }
    return QStringLiteral("?");
}

QJsonArray pointsJson(const std::vector<Point2D>& points) {
    QJsonArray a;
    for (const Point2D& p : points) {
        a.append(r1(p.x));
        a.append(r1(p.y));
    }
    return a;
}

// fitAll 的結果：每組晶格的基底、品質、每個點是不是內點、預測的缺格（畫面範圍內）。
QJsonObject fitJson(const LatticeFit& fit, std::size_t pointCount) {
    QJsonObject o;
    o["reason"] = reasonName(fit.reason);
    if (fit.models.empty()) o["rejectedConfidence"] = r3(fit.rejectedConfidence);
    QJsonArray models;
    for (const LatticeModel& m : fit.models) {
        QJsonObject j;
        j["o"] = QJsonArray{r1(m.basis.origin.x), r1(m.basis.origin.y)};
        j["u"] = QJsonArray{r3(m.basis.u.x), r3(m.basis.u.y)};
        j["v"] = QJsonArray{r3(m.basis.v.x), r3(m.basis.v.y)};
        j["hasV"] = m.hasV();
        j["size"] = QJsonArray{m.maxI - m.minI + 1, m.maxJ - m.minJ + 1};
        j["rms"] = r3(m.rmsPx);
        j["tol"] = r3(m.tolerancePx);
        j["coverage"] = r3(m.coverage);
        j["consistency"] = r3(m.consistency);
        j["confidence"] = r3(m.confidence);
        j["check"] = m.suggestCheck;
        QString flags(static_cast<qsizetype>(pointCount), QLatin1Char('0'));
        for (const std::size_t k : m.inliers) flags[static_cast<qsizetype>(k)] = QLatin1Char('1');
        j["inliers"] = flags;
        std::vector<Point2D> holes;
        for (const PredictedCell& h : LatticePredictor(m, kBoard).holes()) holes.push_back(h.position);
        j["holes"] = pointsJson(holes);
        models.append(j);
    }
    o["models"] = models;
    return o;
}

GridSpec sweepGrid(double tilt) {
    GridSpec g;
    g.cols = 10;
    g.rows = 6;
    g.pitchX = 40.0;
    g.pitchY = 30.0;
    g.angleDeg = tilt;
    // 陣列中心放在畫面中心。
    const Point2D half = rotate({(g.cols - 1) * g.pitchX * 0.5, (g.rows - 1) * g.pitchY * 0.5}, tilt);
    g.origin = {320.0 - half.x, 210.0 - half.y};
    return g;
}

QJsonObject special(const char* id, const std::vector<Point2D>& points) {
    QJsonObject o;
    o["id"] = QString::fromLatin1(id);
    o["pts"] = pointsJson(points);
    o["fit"] = fitJson(LatticeEstimator().fitAll(points), points.size());
    return o;
}

std::vector<Point2D> row(int count, double pitch, double angle, Point2D origin) {
    GridSpec g;
    g.cols = count;
    g.rows = 1;
    g.pitchX = pitch;
    g.angleDeg = angle;
    g.origin = origin;
    return makePoints(g);
}

QJsonObject buildData() {
    QJsonObject root;
    root["schema"] = 1;
    root["generator"] = QStringLiteral("tests/domain/tst_lattice_explainer.cpp");
    root["board"] = QJsonArray{kBoard.w, kBoard.h};
    root["grid"] = QJsonObject{{"cols", 10}, {"rows", 6}, {"pitchX", 40}, {"pitchY", 30}, {"center", QJsonArray{320, 210}}};
    const std::vector<double> noises{0.0, 1.5, 3.0, 4.5, 6.0};
    const std::vector<double> missings{0.0, 0.15, 0.3, 0.45, 0.6};
    const std::vector<double> tilts{0.0, 10.0, 30.0};
    const std::vector<int> outlierCounts{0, 8};
    QJsonObject axes;
    axes["noise"] = QJsonArray{0.0, 1.5, 3.0, 4.5, 6.0};
    axes["missing"] = QJsonArray{0.0, 0.15, 0.3, 0.45, 0.6};
    axes["tilt"] = QJsonArray{0.0, 10.0, 30.0};
    axes["outliers"] = QJsonArray{0, 8};
    root["axes"] = axes;

    QJsonArray sweep;
    int index = 0;
    for (const double noise : noises) {
        for (const double missing : missings) {
            for (const double tilt : tilts) {
                for (const int outliers : outlierCounts) {
                    Scenario s;
                    s.grid = sweepGrid(tilt);
                    s.noisePx = noise;
                    s.missingFraction = missing;
                    s.outliers = outliers;
                    s.seed = 500u + static_cast<std::uint64_t>(index++);
                    const Observed o = generate(s);
                    const LatticeFit fit = LatticeEstimator().fitAll(o.points);
                    QJsonObject e;
                    e["pts"] = pointsJson(o.points);
                    QJsonArray truth;  // 每個點的標準答案格子（col*rows＋row），離群點 −1
                    for (std::size_t k = 0; k < o.points.size(); ++k) truth.append(o.col[k] < 0 ? -1 : o.col[k] * s.grid.rows + o.row[k]);
                    e["truth"] = truth;
                    QJsonArray miss;
                    for (const auto& [c, r] : o.missing) miss.append(c * s.grid.rows + r);
                    e["missing"] = miss;
                    e["fit"] = fitJson(fit, o.points.size());
                    if (!fit.models.empty()) {
                        const Agreement a = compare(fit.models.front(), o, s.grid, 0.25 * 30.0, kBoard);
                        e["eval"] = QJsonObject{{"wrong", static_cast<int>(a.wrongCells)},
                                                {"falseHoles", static_cast<int>(a.holes - a.trueHoles)},
                                                {"missed", static_cast<int>(a.missedHoles)}};
                    }
                    sweep.append(e);
                }
            }
        }
    }
    root["sweep"] = sweep;

    // 特殊情境：退化情況決策樹與已知會失敗的情況（spec.md）各一個例子。
    QJsonArray specials;
    {
        GridSpec far;  // 間距差 20 倍（這頁的比例：12 × 120 已經 10 倍）
        far.cols = 20;
        far.rows = 3;
        far.pitchX = 12.0;
        far.pitchY = 140.0;
        far.origin = {206.0, 70.0};
        specials.append(special("ratio", makePoints(far, {{8, 1}})));
    }
    {
        std::vector<Point2D> p = row(12, 34.0, 0.0, {130.0, 110.0});
        const std::vector<Point2D> b = row(9, 30.0, -10.0, {170.0, 330.0});
        p.insert(p.end(), b.begin(), b.end());
        specials.append(special("twoDirections", p));
    }
    {
        std::vector<Point2D> p;
        for (int k = 0; k < 3; ++k) {
            const std::vector<Point2D> r = row(10, 26.0 + 9.0 * k, 0.0, {150.0, 110.0 + 110.0 * k});
            p.insert(p.end(), r.begin(), r.end());
        }
        specials.append(special("threePitches", p));
    }
    {
        std::vector<Point2D> p = row(14, 34.0, 0.0, {100.0, 210.0});
        p.push_back({100.0 + 6 * 34.0 + 5.0, 210.0 - 40.0});
        specials.append(special("rowAndStray", p));
    }
    {
        Rng rng(31);
        std::vector<Point2D> p;
        for (int k = 0; k < 45; ++k) p.push_back({40.0 + rng.uniform() * 560.0, 40.0 + rng.uniform() * 340.0});
        specials.append(special("random", p));
    }
    {
        GridSpec left;
        left.cols = 5;
        left.rows = 4;
        left.pitchX = 34.0;
        left.pitchY = 30.0;
        left.origin = {60.0, 150.0};
        GridSpec right = left;
        right.origin = {60.0 + 12 * 34.0, 150.0};
        std::vector<Point2D> p = makePoints(left, {{2, 1}});
        const std::vector<Point2D> b = makePoints(right, {{1, 2}});
        p.insert(p.end(), b.begin(), b.end());
        specials.append(special("twoArrays", p));
    }
    {
        GridSpec g;
        g.cols = 12;
        g.rows = 2;
        g.pitchX = 40.0;
        g.pitchY = 60.0;
        g.origin = {100.0, 170.0};
        std::vector<std::pair<int, int>> skip;
        for (int c = 0; c < 12; ++c) {
            if (c != 3 && c != 8) skip.push_back({c, 1});
        }
        specials.append(special("sparseRow", makePoints(g, skip)));
    }
    specials.append(special("twoPoints", {{280.0, 210.0}, {360.0, 210.0}}));
    specials.append(special("overlapping", {{320.0, 210.0}, {320.0, 210.0}, {320.0, 210.0}}));
    root["specials"] = specials;
    return root;
}

// 兩份 JSON 一樣嗎？結構與字串完全相同；數字容許 0.11 的差（座標四捨五入到 0.1）。不一樣時回第一個差異的位置。
QString differs(const QJsonValue& a, const QJsonValue& b, const QString& path) {
    if (a.type() != b.type()) return path + QStringLiteral(": type");
    if (a.isDouble()) return std::abs(a.toDouble() - b.toDouble()) <= 0.11 ? QString() : path;
    if (a.isObject()) {
        const QJsonObject x = a.toObject(), y = b.toObject();
        if (x.keys() != y.keys()) return path + QStringLiteral(": keys");
        for (const QString& k : x.keys()) {
            const QString d = differs(x.value(k), y.value(k), path + QLatin1Char('.') + k);
            if (!d.isEmpty()) return d;
        }
        return {};
    }
    if (a.isArray()) {
        const QJsonArray x = a.toArray(), y = b.toArray();
        if (x.size() != y.size()) return path + QStringLiteral(": size");
        for (qsizetype k = 0; k < x.size(); ++k) {
            const QString d = differs(x.at(k), y.at(k), path + QStringLiteral("[%1]").arg(k));
            if (!d.isEmpty()) return d;
        }
        return {};
    }
    return a == b ? QString() : path;
}

}  // namespace

class TstLatticeExplainer : public QObject {
    Q_OBJECT

private slots:
    void thePageShowsWhatTheCodeComputes() {
        const QJsonObject data = buildData();
        QFile f(kPage);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(QStringLiteral("cannot read %1").arg(kPage)));
        QString html = QString::fromUtf8(f.readAll());
        f.close();
        const qsizetype start = html.indexOf(kOpen);
        QVERIFY2(start >= 0, "the page has no <script id=\"lattice-data\"> block");
        const qsizetype bodyStart = start + kOpen.size();
        const qsizetype end = html.indexOf(kClose, bodyStart);
        QVERIFY(end > bodyStart);

        if (qEnvironmentVariableIntValue("AOI_EXPORT_EXPLAINER") == 1) {
            const QString json = QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
            html = html.left(bodyStart) + QLatin1Char('\n') + json + QLatin1Char('\n') + html.mid(end);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(html.toUtf8());
            f.close();
            qInfo().noquote() << "wrote the lattice data into" << kPage << QStringLiteral("(%1 bytes)").arg(json.size());
            return;
        }
        QJsonParseError error{};
        const QJsonDocument embedded = QJsonDocument::fromJson(html.mid(bodyStart, end - bodyStart).trimmed().toUtf8(), &error);
        QVERIFY2(error.error == QJsonParseError::NoError,
                 "the page has no valid lattice data: run tst_lattice_explainer with AOI_EXPORT_EXPLAINER=1");
        const QString d = differs(QJsonValue(data), QJsonValue(embedded.object()), QStringLiteral("data"));
        QVERIFY2(d.isEmpty(), qPrintable(QStringLiteral("the page is out of date at %1: run tst_lattice_explainer with AOI_EXPORT_EXPLAINER=1").arg(d)));
    }
};

QTEST_APPLESS_MAIN(TstLatticeExplainer)
#include "tst_lattice_explainer.moc"
