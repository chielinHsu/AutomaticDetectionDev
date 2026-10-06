// S06：JsonRecipeStore（IRecipeStore 的 JSON 實作）的契約：往返（逐位元）、v1 範例檔永遠讀得進來、遷移掛勾、
// 損壞的檔案回 FormatInvalid（不當機、不丟例外；含隨機破壞）、原子寫入、存不下的內容回 InvalidArgument。
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <atomic>
#include <cmath>
#include <functional>
#include <limits>
#include <thread>
#include <vector>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "RecipeTestSupport.h"
#include "infrastructure/storage/JsonRecipeStore.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

QString fixture(const char* name) { return QDir(QStringLiteral(AOI_INFRA_FIXTURES)).filePath(QString::fromLatin1(name)); }

QJsonObject fixtureObject() { return QJsonDocument::fromJson(readBytes(fixture("recipe-v1.json"))).object(); }

// 改 JSON 的某個地方：路徑是 "board.components.0.pose"（陣列用數字）。
using Mutation = std::function<void(QJsonObject&)>;

QJsonValue get(const QJsonValue& v, const QStringList& path, int i = 0) {
    if (i == path.size()) return v;
    bool isIndex = false;
    const int index = path[i].toInt(&isIndex);
    return get(isIndex ? v.toArray().at(index) : v.toObject().value(path[i]), path, i + 1);
}
QJsonValue set(const QJsonValue& v, const QStringList& path, const QJsonValue& value, int i = 0) {
    if (i == path.size()) return value;
    bool isIndex = false;
    const int index = path[i].toInt(&isIndex);
    if (isIndex) {
        QJsonArray a = v.toArray();
        a[index] = set(a.at(index), path, value, i + 1);
        return a;
    }
    QJsonObject o = v.toObject();
    if (i + 1 == path.size() && value.isUndefined()) o.remove(path[i]);
    else o.insert(path[i], set(o.value(path[i]), path, value, i + 1));
    return o;
}
Mutation setAt(const QString& path, const QJsonValue& value) {
    return [path, value](QJsonObject& root) { root = set(root, path.split(QLatin1Char('.')), value).toObject(); };
}
Mutation removeAt(const QString& path) { return setAt(path, QJsonValue(QJsonValue::Undefined)); }

struct BrokenCase {
    Mutation mutate;
    QString expectedInMessage;  // 錯誤訊息要指出的地方
};

}  // namespace

Q_DECLARE_METATYPE(BrokenCase)

class TstRecipeStore : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(QFile::exists(fixture("recipe-v1.json"))); }

    // ---- 往返（IRecipeStore 契約第 1 條）----
    void saveThenLoadRestoresEveryFieldBitForBit() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("rich.aoi.json")));
        const Recipe original = richRecipe(24);
        JsonRecipeStore store;
        const Result<void> saved = store.save(original, path);
        QVERIFY2(saved.ok(), saved.ok() ? "" : saved.error().message.c_str());
        const Result<Recipe> loaded = store.load(path);
        QVERIFY2(loaded.ok(), loaded.ok() ? "" : loaded.error().message.c_str());
        const QString diff = diffRecipe(original, loaded.value());
        QVERIFY2(diff.isEmpty(), qPrintable(diff));
        QCOMPARE(loaded.value().schemaVersion, JsonRecipeStore::kSchemaVersion);
        QCOMPARE(loaded.value().board.significantBits, 12);
        // 參考影像讀回來是自己擁有的緊密緩衝
        const ImageView& ref = loaded.value().board.types[1].referenceImage;
        QVERIFY(ref.owner != nullptr);
        QVERIFY(ref.packed());
    }

    // 特別的浮點數：最小的次正規數、最大的有限值…都逐位元相同；只有 −0.0 讀回來是 +0.0（JSON 不分正負零，契約寫明）。
    void unusualNumbersSurvive_data() {
        QTest::addColumn<double>("value");
        QTest::newRow("negative zero") << -0.0;
        QTest::newRow("denorm min") << std::numeric_limits<double>::denorm_min();
        QTest::newRow("max") << std::numeric_limits<double>::max();
        QTest::newRow("lowest") << std::numeric_limits<double>::lowest();
        QTest::newRow("0.1 + 0.2") << 0.1 + 0.2;
        QTest::newRow("2^53 + 2") << 9007199254740994.0;
    }
    void unusualNumbersSurvive() {
        QFETCH(double, value);
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("n.json")));
        Recipe r = richRecipe(1);
        r.board.components[0].pose.angleDeg = value;
        JsonRecipeStore store;
        QVERIFY(store.save(r, path).ok());
        const Result<Recipe> loaded = store.load(path);
        QVERIFY(loaded.ok());
        const double back = loaded.value().board.components[0].pose.angleDeg;
        if (value == 0.0 && std::signbit(value)) {
            QCOMPARE(back, 0.0);  // 數值相等
            QVERIFY(!std::signbit(back));
            return;
        }
        QVERIFY2(sameBits(back, value), qPrintable(QStringLiteral("%1 came back as %2").arg(value, 0, 'g', 17).arg(back, 0, 'g', 17)));
    }

    void anEmptyBoardRoundTrips() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("empty.json")));
        JsonRecipeStore store;
        const Recipe empty;
        QVERIFY(store.save(empty, path).ok());
        const Result<Recipe> loaded = store.load(path);
        QVERIFY(loaded.ok());
        QVERIFY2(diffRecipe(empty, loaded.value()).isEmpty(), qPrintable(diffRecipe(empty, loaded.value())));
    }

    // 5 000 個元件（效能另外量，這裡只看正確）。
    void aLargeBoardRoundTrips() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("large.json")));
        const Recipe original = richRecipe(5000);
        JsonRecipeStore store;
        QVERIFY(store.save(original, path).ok());
        const Result<Recipe> loaded = store.load(path);
        QVERIFY(loaded.ok());
        QVERIFY2(diffRecipe(original, loaded.value()).isEmpty(), qPrintable(diffRecipe(original, loaded.value())));
    }

    // ---- v1 範例檔（tests/infra/fixtures/recipe-v1.json：手寫，之後的版本也要讀得進來）----
    void theV1FixtureLoadsWithTheExpectedValues() {
        JsonRecipeStore store;
        const Result<Recipe> r = store.load(utf8(fixture("recipe-v1.json")));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const Recipe& recipe = r.value();
        QCOMPARE(recipe.schemaVersion, 1);
        const Board& b = recipe.board;
        QCOMPARE(QString::fromStdString(b.name), QStringLiteral("板子 A.png"));
        QCOMPARE(b.imageWidth, 2592);
        QCOMPARE(b.imageHeight, 1944);
        QCOMPARE(b.umPerPixel, 12.5);
        QCOMPARE(b.significantBits, 0);  // v1 沒有這個欄位：遷移補 0（自動）
        QCOMPARE(b.types.size(), std::size_t{2});
        const ComponentType& ic = b.types[0];
        QCOMPARE(ic.id, std::string("T1"));
        QCOMPARE(ic.libId, std::string("lib-7"));
        QCOMPARE(ic.partNumber, std::string("8371"));
        QCOMPARE(ic.aliases, (std::vector<std::string>{"SOP20", "20-SOIC"}));
        QVERIFY(ic.category == ComponentCategory::IC);
        QCOMPARE(ic.groups.size(), std::size_t{3});
        QVERIFY(ic.groups[1].kind == BoxGroupKind::Pin);
        QCOMPARE(ic.groups[1].matrix.cols, 10);
        QCOMPARE(ic.groups[1].matrix.skipped.size(), std::size_t{1});
        QCOMPARE(ic.groups[1].matrix.skipped[0].col, 3);
        QVERIFY(ic.groups[2].kind == BoxGroupKind::Window);
        QCOMPARE(ic.groups[2].window.parentGroupId, 2);
        QCOMPARE(ic.groups[2].window.algo, std::string("Solder"));
        QCOMPARE(ic.groups[2].window.offsetYRatio, -0.25);
        // 參考影像：Gray8 3×2 的 10、20、30／40、50、60
        const ImageView& ref = ic.referenceImage;
        QVERIFY(ref.valid());
        QVERIFY(ref.format == PixelFormat::Gray8);
        QCOMPARE(ref.width, 3);
        QCOMPARE(ref.height, 2);
        QCOMPARE(ref.umPerPixel, 12.5);
        QCOMPARE(byteAt(ref, 0, 0, 0), 10);
        QCOMPARE(byteAt(ref, 2, 1, 0), 60);
        // Gray16 2×1（小端）：0x1234、0xFFFF
        const ImageView& ref16 = b.types[1].referenceImage;
        QVERIFY(ref16.format == PixelFormat::Gray16);
        QCOMPARE(ref16.significantBits, 16);
        QCOMPARE(ref.significantBits, 0);
        QCOMPARE(gray16At(ref16, 0, 0), 0x1234);
        QCOMPARE(gray16At(ref16, 1, 0), 0xFFFF);

        QCOMPARE(b.components.size(), std::size_t{5});
        const ComponentInstance& u2 = b.components[1];
        QCOMPARE(u2.id, 2);
        QCOMPARE(u2.pose.y, 433.125);
        QCOMPARE(u2.pose.angleDeg, -0.5);
        QVERIFY(u2.side == ComponentSide::Bottom);
        QVERIFY(u2.state == ObjectState::Found);
        QCOMPARE(u2.score.value(), 0.875);
        const ComponentInstance& manual = b.components[3];
        QVERIFY(manual.origin == ObjectOrigin::Manual);
        QVERIFY(!manual.score.has_value());
        QVERIFY(b.components[2].state == ObjectState::Inferred);
        QVERIFY(b.components[4].state == ObjectState::Rejected);
        QCOMPARE(recipe.params.minScore, 0.7);
        QCOMPARE(recipe.params.angleMinDeg, -10.0);
        QCOMPARE(recipe.params.maxMatches, std::size_t{5000});
    }

    // ---- v2 範例檔（S07：board.significantBits；tests/infra/fixtures/recipe-v2.json，之後的版本也要讀得進來）----
    void theV2FixtureLoadsAndOnlyAddsTheBitDepth() {
        JsonRecipeStore store;
        const Result<Recipe> v2 = store.load(utf8(fixture("recipe-v2.json")));
        QVERIFY2(v2.ok(), v2.ok() ? "" : v2.error().message.c_str());
        QCOMPARE(v2.value().schemaVersion, 2);
        QCOMPARE(v2.value().board.significantBits, 12);
        // 除了位元數，和 v1 範例是同一份配方。
        Recipe v1 = store.load(utf8(fixture("recipe-v1.json"))).value();
        v1.board.significantBits = 12;
        QVERIFY2(diffRecipe(v1, v2.value()).isEmpty(), qPrintable(diffRecipe(v1, v2.value())));
    }

    void theV1FixtureSurvivesASaveAndLoad() {
        JsonRecipeStore store;
        const Result<Recipe> first = store.load(utf8(fixture("recipe-v1.json")));
        QVERIFY(first.ok());
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("again.json")));
        QVERIFY(store.save(first.value(), path).ok());
        const Result<Recipe> second = store.load(path);
        QVERIFY(second.ok());
        QVERIFY2(diffRecipe(first.value(), second.value()).isEmpty(), qPrintable(diffRecipe(first.value(), second.value())));
    }

    void savedFilesAreTaggedAndIndented() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("tag.json"));
        JsonRecipeStore store;
        Recipe r = richRecipe(2);
        r.schemaVersion = 7;  // save 不看它：一律寫目前的版本
        QVERIFY(store.save(r, utf8(path)).ok());
        const QByteArray bytes = readBytes(path);
        const QJsonObject root = QJsonDocument::fromJson(bytes).object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("aoi-recipe"));
        QCOMPARE(root.value(QStringLiteral("schemaVersion")).toInt(), JsonRecipeStore::kSchemaVersion);
        QCOMPARE(root.value(QStringLiteral("board")).toObject().value(QStringLiteral("significantBits")).toInt(), 12);
        QVERIFY(bytes.contains("\n    "));  // 縮排：方便比對與人工檢查
        QVERIFY(!bytes.startsWith("\xEF\xBB\xBF"));
    }

    // ---- 遷移掛勾 ----
    void olderFilesAreMigratedStepByStep() {
        std::vector<int> calls;
        std::vector<int> seenVersions;
        // 內建的 v1 → v2，再加兩個假的（v2 → v3、v3 → v4）：假裝這個程式是 v4。
        std::vector<json::Migration> migrations = JsonRecipeStore::builtInMigrations();
        QCOMPARE(migrations.size(), std::size_t{1});
        migrations.push_back([&](QJsonObject& root) -> Result<void> {
            calls.push_back(1);
            seenVersions.push_back(root.value(QStringLiteral("schemaVersion")).toInt());
            root.insert(QStringLiteral("addedByMigration1"), true);
            return {};
        });
        migrations.push_back([&](QJsonObject& root) -> Result<void> {
            calls.push_back(2);
            seenVersions.push_back(root.value(QStringLiteral("schemaVersion")).toInt());
            if (!root.value(QStringLiteral("addedByMigration1")).toBool()) return makeError(ErrorCode::Internal, "order");
            return {};
        });
        JsonRecipeStore store(4, migrations);
        const Result<Recipe> r = store.load(utf8(fixture("recipe-v1.json")));
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        QCOMPARE(calls, (std::vector<int>{1, 2}));
        QCOMPARE(seenVersions, (std::vector<int>{2, 3}));  // 每升一版，schemaVersion 跟著改
        QCOMPARE(r.value().schemaVersion, 1);              // 回報檔案原本的版本
    }

    void aMissingOrFailingMigrationIsAFormatError() {
        JsonRecipeStore missing(2, {});
        const Result<Recipe> a = missing.load(utf8(fixture("recipe-v1.json")));
        QCOMPARE_CODE(a.error().code, ErrorCode::FormatInvalid);
        QVERIFY(a.error().message.find("No migration from schema version 1") != std::string::npos);

        JsonRecipeStore failing(2, {[](QJsonObject&) -> Result<void> { return makeError(ErrorCode::Internal, "cannot convert"); }});
        const Result<Recipe> b = failing.load(utf8(fixture("recipe-v1.json")));
        QCOMPARE_CODE(b.error().code, ErrorCode::FormatInvalid);
        QVERIFY(b.error().message.find("cannot convert") != std::string::npos);
    }

    void aFileFromANewerProgramIsRefused() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("v3.json"));
        QJsonObject root = fixtureObject();
        root.insert(QStringLiteral("schemaVersion"), JsonRecipeStore::kSchemaVersion + 1);
        QVERIFY(writeBytes(path, QJsonDocument(root).toJson()));
        const Result<Recipe> r = JsonRecipeStore().load(utf8(path));
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
        QVERIFY2(r.error().message.find("newer program") != std::string::npos, r.error().message.c_str());
    }

    // ---- 壞檔（契約第 2 條）----
    void brokenFilesAreFormatErrors_data() {
        QTest::addColumn<BrokenCase>("broken");
        auto row = [](const char* name, Mutation m, const char* where) {
            QTest::newRow(name) << BrokenCase{std::move(m), QString::fromLatin1(where)};
        };
        row("wrong format tag", setAt(QStringLiteral("format"), QStringLiteral("aoi-library")), "is not an 'aoi-recipe' file");
        row("no format tag", removeAt(QStringLiteral("format")), "is not an 'aoi-recipe' file");
        row("version as text", setAt(QStringLiteral("schemaVersion"), QStringLiteral("1")), "schemaVersion");
        row("version zero", setAt(QStringLiteral("schemaVersion"), 0), "schemaVersion");
        row("version 1.5", setAt(QStringLiteral("schemaVersion"), 1.5), "schemaVersion");
        row("no board", removeAt(QStringLiteral("board")), "board must be an object");
        row("no params", removeAt(QStringLiteral("params")), "params must be an object");
        row("components not an array", setAt(QStringLiteral("board.components"), 3), "board.components must be an array");
        row("missing pose", removeAt(QStringLiteral("board.components.1.pose")), "board.components[1].pose");
        row("pose.x is text", setAt(QStringLiteral("board.components.1.pose.x"), QStringLiteral("12")), "board.components[1].pose.x must be a number");
        row("unknown state", setAt(QStringLiteral("board.components.0.state"), QStringLiteral("lost")), "board.components[0].state has an unknown value 'lost'");
        row("unknown kind", setAt(QStringLiteral("board.types.0.groups.1.kind"), QStringLiteral("lead")), "board.types[0].groups[1].kind");
        row("id is not whole", setAt(QStringLiteral("board.components.0.id"), 1.5), "board.components[0].id must be a whole number");
        row("id too large", setAt(QStringLiteral("board.components.0.id"), 1e12), "board.components[0].id is out of range");
        row("score is text", setAt(QStringLiteral("board.components.1.score"), QStringLiteral("high")), "board.components[1].score must be a number");
        row("dangling type", setAt(QStringLiteral("board.components.4.typeId"), QStringLiteral("T9")), "board.components[4].typeId refers to 'T9'");
        row("duplicate component id", setAt(QStringLiteral("board.components.1.id"), 1), "board.components[1].id is used twice");
        row("duplicate type id", setAt(QStringLiteral("board.types.1.id"), QStringLiteral("T1")), "board.types[1].id is used twice");
        row("negative image width", setAt(QStringLiteral("board.imageWidth"), -5), "board.imageWidth is out of range");
        row("negative calibration", setAt(QStringLiteral("board.umPerPixel"), -1), "board.umPerPixel must not be negative");
        row("7 significant bits", setAt(QStringLiteral("board.significantBits"), 7), "board.significantBits must be 0 or 8..16");
        row("17 significant bits", setAt(QStringLiteral("board.significantBits"), 17), "board.significantBits is out of range");
        row("negative matrix rows", setAt(QStringLiteral("board.types.0.groups.1.matrix.rows"), -1), "matrix.rows is out of range");
        row("aliases not text", setAt(QStringLiteral("board.types.0.aliases"), QJsonArray{1, 2}), "board.types[0].aliases[0] must be a string");
        row("image not base64", setAt(QStringLiteral("board.types.0.referenceImage.data"), QStringLiteral("***")), "referenceImage.data is not valid base64");
        row("image size mismatch", setAt(QStringLiteral("board.types.0.referenceImage.width"), 4), "referenceImage.data does not match the image size");
        row("image unknown encoding", setAt(QStringLiteral("board.types.0.referenceImage.encoding"), QStringLiteral("png")), "referenceImage.encoding has an unknown value");
        row("image zero width", setAt(QStringLiteral("board.types.0.referenceImage.width"), 0), "referenceImage.width is out of range");
        row("image too wide", setAt(QStringLiteral("board.types.0.referenceImage.width"), 100000), "referenceImage.width is out of range");
        row("image unknown format", setAt(QStringLiteral("board.types.0.referenceImage.format"), QStringLiteral("Rgb48")), "referenceImage.format has an unknown value");
        row("image 17 bits", setAt(QStringLiteral("board.types.0.referenceImage.significantBits"), 17), "referenceImage.significantBits is out of range");
        // 資料被截斷（只剩 3 個 byte）。S06 早先用 qCompress 時，這一列在 Debug 撞上 Qt 的 Q_UNREACHABLE（見 JsonCodec.cpp）。
        row("image truncated", setAt(QStringLiteral("board.types.0.referenceImage.data"), QStringLiteral("ChQe")), "referenceImage.data does not match the image size");
        row("image too much data", setAt(QStringLiteral("board.types.0.referenceImage.data"), QStringLiteral("ChQeKDI8Rg==")), "referenceImage.data does not match the image size");
        row("missing referenceImage", removeAt(QStringLiteral("board.types.0.referenceImage")), "board.types[0].referenceImage is missing");
        row("maxMatches negative", setAt(QStringLiteral("params.maxMatches"), -1), "params.maxMatches is out of range");
    }
    void brokenFilesAreFormatErrors() {
        QFETCH(BrokenCase, broken);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("broken.json"));
        QJsonObject root = fixtureObject();
        broken.mutate(root);
        QVERIFY(writeBytes(path, QJsonDocument(root).toJson()));
        const Result<Recipe> r = JsonRecipeStore().load(utf8(path));
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
        const QString message = QString::fromStdString(r.error().message);
        QVERIFY2(message.contains(broken.expectedInMessage), qPrintable(message));
    }

    void bytesThatAreNotARecipeAreFormatErrors_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("text") << QByteArray("hello");
        QTest::newRow("array") << QByteArray("[1, 2, 3]");
        QTest::newRow("truncated") << readBytes(fixture("recipe-v1.json")).left(300);
        QTest::newRow("binary") << QByteArray("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16);
        QTest::newRow("null") << QByteArray("null");
    }
    void bytesThatAreNotARecipeAreFormatErrors() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("x.json"));
        QVERIFY(writeBytes(path, bytes));
        QCOMPARE_CODE(JsonRecipeStore().load(utf8(path)).error().code, ErrorCode::FormatInvalid);
    }

    // 隨機破壞（固定 seed）：翻 byte、刪一段、插入垃圾、截斷。每一次都要是成功或 FormatInvalid，不可以當機或丟例外。
    void randomlyDamagedFilesNeverCrash() {
        const QByteArray original = readBytes(fixture("recipe-v1.json"));
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("fuzz.json"));
        const std::string p = utf8(path);
        JsonRecipeStore store;
        std::uint32_t s = 12345;
        auto next = [&] {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        };
        int ok = 0, invalid = 0;
        const int rounds = 500;  // 每一輪都寫一次檔：Windows 上約 8 ms 一輪（2 000 輪是 17 秒）
        for (int round = 0; round < rounds; ++round) {
            QByteArray bytes = original;
            const int edits = 1 + static_cast<int>(next() % 4);
            for (int e = 0; e < edits && !bytes.isEmpty(); ++e) {
                const qsizetype at = static_cast<qsizetype>(next() % static_cast<std::uint32_t>(bytes.size()));
                switch (next() % 4) {
                    case 0: bytes[at] = static_cast<char>(next() & 0xFF); break;
                    case 1: bytes.remove(at, 1 + static_cast<qsizetype>(next() % 20)); break;
                    case 2: bytes.insert(at, QByteArray(1 + static_cast<qsizetype>(next() % 8), static_cast<char>("{}[],:\"0e-"[next() % 10]))); break;
                    case 3: bytes.truncate(at); break;
                }
            }
            QVERIFY(writeBytes(path, bytes));
            const Result<Recipe> r = store.load(p);
            if (r.ok()) ++ok;
            else if (r.error().code == ErrorCode::FormatInvalid) ++invalid;
            else QFAIL(qPrintable(QStringLiteral("round %1: unexpected error %2").arg(round).arg(QString::fromStdString(r.error().message))));
        }
        qInfo("damaged files: %d still valid, %d rejected as FormatInvalid", ok, invalid);
        QCOMPARE(ok + invalid, rounds);
        QVERIFY(invalid > rounds / 2);
    }

    void aFileWithAByteOrderMarkLoads() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("bom.json"));
        QVERIFY(writeBytes(path, QByteArray("\xEF\xBB\xBF") + readBytes(fixture("recipe-v1.json"))));
        QVERIFY(JsonRecipeStore().load(utf8(path)).ok());
    }

    void fileProblemsGiveTheContractErrorCodes() {
        QTemporaryDir dir = makeTempDir();
        JsonRecipeStore store;
        QCOMPARE_CODE(store.load(utf8(dir.filePath(QStringLiteral("missing.json")))).error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(store.load("").error().code, ErrorCode::NotFound);
        QCOMPARE_CODE(store.load(utf8(dir.path())).error().code, ErrorCode::IoFailed);
        QCOMPARE_CODE(store.save(richRecipe(1), utf8(dir.filePath(QStringLiteral("no/such/folder/r.json")))).error().code, ErrorCode::IoFailed);
    }

    void aPathWithChineseCharactersWorks() {
        QTemporaryDir dir = makeTempDir();
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("配方 資料夾")));
        const std::string path = utf8(dir.filePath(QStringLiteral("配方 資料夾/板子 一號.aoi.json")));
        JsonRecipeStore store;
        QVERIFY(store.save(richRecipe(3), path).ok());
        QVERIFY(store.load(path).ok());
    }

    // ---- 存不下的內容（契約第 5 條）：不寫檔 ----
    void unsavableContentIsRefusedBeforeWriting_data() {
        QTest::addColumn<int>("which");
        QTest::newRow("NaN pose") << 0;
        QTest::newRow("infinite score") << 1;
        QTest::newRow("invalid UTF-8 name") << 2;
        QTest::newRow("dangling type") << 3;
        QTest::newRow("duplicate component id") << 4;
        QTest::newRow("NaN in a box group") << 5;
        QTest::newRow("NaN calibration of a reference image") << 6;
    }
    void unsavableContentIsRefusedBeforeWriting() {
        QFETCH(int, which);
        Recipe r = richRecipe(4);
        switch (which) {
            case 0: r.board.components[0].pose.x = std::numeric_limits<double>::quiet_NaN(); break;
            case 1: r.board.components[1].score = std::numeric_limits<double>::infinity(); break;
            case 2: r.board.components[2].name = std::string("\xC3\x28", 2); break;
            case 3: r.board.components[3].typeId = "nope"; break;
            case 4: r.board.components[3].id = r.board.components[0].id; break;
            case 5: r.board.types[2].groups[1].matrix.pitchXUm = std::numeric_limits<double>::quiet_NaN(); break;
            case 6: {
                ImageBuffer b(4, 4, PixelFormat::Gray8);
                r.board.types[2].referenceImage = b.view(std::numeric_limits<double>::quiet_NaN());
                break;
            }
        }
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("refused.json"));
        const Result<void> saved = JsonRecipeStore().save(r, utf8(path));
        QVERIFY(!saved.ok());
        QCOMPARE_CODE(saved.error().code, ErrorCode::InvalidArgument);
        QVERIFY(!QFile::exists(path));
    }

    // ---- 原子寫入（契約第 4 條）----
    void savingOverAnExistingFileReplacesItCompletely() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("r.json"));
        JsonRecipeStore store;
        QVERIFY(store.save(richRecipe(50), utf8(path)).ok());
        const Recipe smaller = richRecipe(2);
        QVERIFY(store.save(smaller, utf8(path)).ok());
        QVERIFY(diffRecipe(smaller, store.load(utf8(path)).value()).isEmpty());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), qsizetype{1});  // 沒有留下暫存檔
    }

    void aFailedSaveLeavesTheOldFileUntouched() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("locked.json"));
        JsonRecipeStore store;
        QVERIFY(store.save(richRecipe(5), utf8(path)).ok());
        const QByteArray before = readBytes(path);
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));  // 唯讀
        const Result<void> r = store.save(richRecipe(9), utf8(path));
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::IoFailed);
        QCOMPARE(readBytes(path), before);
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), qsizetype{1});
    }

    // 可重入：幾個執行緒同時存同一個路徑，最後的檔案是其中完整的一份。
    void concurrentSavesLeaveOneCompleteFile() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("race.json")));
        JsonRecipeStore store;
        std::vector<Recipe> versions;
        for (int i = 0; i < 6; ++i) versions.push_back(richRecipe(10 + i * 40));
        std::atomic<int> failures{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 6; ++i) {
            threads.emplace_back([&, i] {
                for (int k = 0; k < 5; ++k)
                    if (!store.save(versions[static_cast<std::size_t>(i)], path).ok()) ++failures;
            });
        }
        for (std::thread& t : threads) t.join();
        // Windows 上兩個 QSaveFile 同時換名，後到的可能失敗（IoFailed）：這是預期的，檔案仍然完整。
        qInfo("concurrent saves that failed: %d of 30", failures.load());
        const Result<Recipe> loaded = store.load(path);
        QVERIFY(loaded.ok());
        bool matchesOne = false;
        for (const Recipe& v : versions) matchesOne = matchesOne || diffRecipe(v, loaded.value()).isEmpty();
        QVERIFY(matchesOne);
    }
};

QTEST_GUILESS_MAIN(TstRecipeStore)
#include "tst_recipe_store.moc"
