// S06：JsonLibraryStore（ILibraryStore 的 JSON 實作，一個檔案 ＝ 一層樣本庫）的契約與規則。
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>
#include <atomic>
#include <set>
#include <thread>
#include <vector>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "RecipeTestSupport.h"
#include "infrastructure/storage/JsonLibraryStore.h"
#include "infrastructure/storage/JsonRecipeStore.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

QString fixture(const char* name) { return QDir(QStringLiteral(AOI_INFRA_FIXTURES)).filePath(QString::fromLatin1(name)); }

ComponentType typeWithPart(const std::string& partNumber, std::vector<std::string> aliases = {}, const std::string& name = "") {
    ComponentType t = richType("board-copy-id", 1);
    t.partNumber = partNumber;
    t.aliases = std::move(aliases);
    t.name = name.empty() ? "name of " + partNumber : name;
    t.libId = "something-from-a-board";
    return t;
}

}  // namespace

class TstLibraryStore : public QObject {
    Q_OBJECT

private slots:
    void aMissingFileIsAnEmptyLibrary() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        const auto all = store.loadAll();
        QVERIFY(all.ok());
        QVERIFY(all.value().empty());
        const auto found = store.find("8371");
        QVERIFY(found.ok());
        QVERIFY(!found.value().has_value());
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("library.json"))));  // 讀不會建立檔案
    }

    // 存進庫：庫指派 id，libId 清成空字串（庫是母版）；存完用 find 取回。
    void savingAssignsAnIdAndClearsTheLibId() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        QVERIFY(store.save(typeWithPart("8371", {"SOP20"})).ok());
        QVERIFY(store.save(typeWithPart("0603-R")).ok());
        const auto found = store.find("8371");
        QVERIFY(found.ok() && found.value().has_value());
        QCOMPARE(found.value()->id, std::string("lib-1"));
        QCOMPARE(found.value()->libId, std::string());
        QCOMPARE(store.find("0603-R").value()->id, std::string("lib-2"));
        // 其餘內容原樣保存（框群組、參考影像）
        ComponentType expected = typeWithPart("8371", {"SOP20"});
        expected.id = "lib-1";
        expected.libId.clear();
        QVERIFY2(diffType(expected, *found.value(), QStringLiteral("entry")).isEmpty(), qPrintable(diffType(expected, *found.value(), QStringLiteral("entry"))));
    }

    void findMatchesThePartNumberThenTheAliasesIgnoringCaseAndSpaces() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        QVERIFY(store.save(typeWithPart("8371", {"SOP20", "20-SOIC"})).ok());
        QVERIFY(store.save(typeWithPart("SOP20", {}, "the real SOP20")).ok());  // 另一個條目的料號正好是 8371 的別名
        QVERIFY(store.save(typeWithPart("Ärger-1", {"Straße"})).ok());

        QCOMPARE(store.find("8371").value()->partNumber, std::string("8371"));
        QCOMPARE(store.find("  20-soic ").value()->partNumber, std::string("8371"));  // 別名、不分大小寫、去掉空白
        QCOMPARE(store.find("sop20").value()->name, std::string("the real SOP20"));   // 料號優先於別名
        QCOMPARE(store.find("ärger-1").value()->partNumber, std::string("Ärger-1"));   // Unicode 不分大小寫
        QCOMPARE(store.find("STRAßE").value()->partNumber, std::string("Ärger-1"));    // 別名也不分大小寫
        // Qt 的 toCaseFolded 是「簡單折疊」（一個字元對一個字元）：ß 不等於 ss。記在 JsonLibraryStore.h。
        QVERIFY(!store.find("STRASSE").value().has_value());
        QVERIFY(!store.find("8372").value().has_value());
        QVERIFY(!store.find("   ").value().has_value());
    }

    void savingTheSamePartNumberOverwritesAndKeepsTheIdAndPosition() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        QVERIFY(store.save(typeWithPart("A")).ok());
        QVERIFY(store.save(typeWithPart("B")).ok());
        QVERIFY(store.save(typeWithPart(" a ", {"new alias"}, "A version 2")).ok());  // 同料號（正規化之後）
        const auto all = store.loadAll().value();
        QCOMPARE(all.size(), std::size_t{2});
        QCOMPARE(all[0].id, std::string("lib-1"));  // 位置與 id 不變
        QCOMPARE(all[0].name, std::string("A version 2"));
        QCOMPARE(store.find("new alias").value()->id, std::string("lib-1"));
    }

    void aPartNumberIsRequired() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        QCOMPARE_CODE(store.save(typeWithPart("  ")).error().code, ErrorCode::InvalidArgument);
        QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("library.json"))));
    }

    // 刪掉的 id 永不重複使用：舊板子的複本用 libId 記得來源，重複使用會讓它指到別的類型。跨程式重新啟動也一樣。
    void removedIdsAreNeverReused() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("library.json")));
        {
            JsonLibraryStore store(path);
            QVERIFY(store.save(typeWithPart("A")).ok());
            QVERIFY(store.save(typeWithPart("B")).ok());
            QVERIFY(store.remove("lib-2").ok());
            QVERIFY(store.save(typeWithPart("C")).ok());
            QCOMPARE(store.find("C").value()->id, std::string("lib-3"));
        }
        JsonLibraryStore reopened(path);  // 重新讀檔
        QCOMPARE(reopened.loadAll().value().size(), std::size_t{2});
        QVERIFY(reopened.save(typeWithPart("D")).ok());
        QCOMPARE(reopened.find("D").value()->id, std::string("lib-4"));
    }

    void removingAnUnknownEntryIsNotFound() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        QVERIFY(store.save(typeWithPart("A")).ok());
        QCOMPARE_CODE(store.remove("lib-99").error().code, ErrorCode::NotFound);
        QCOMPARE(store.loadAll().value().size(), std::size_t{1});
    }

    // 刪庫不影響板子：板子的複本在配方裡（ADR-006 §5），庫刪了，複本與它的 libId 原封不動。
    void removingAnEntryDoesNotTouchABoardCopy() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore library(utf8(dir.filePath(QStringLiteral("library.json"))));
        QVERIFY(library.save(typeWithPart("8371")).ok());
        ComponentType copy = *library.find("8371").value();
        copy.libId = copy.id;  // 板子的複本記得來源
        copy.id = "T1";
        Recipe recipe;
        recipe.board.types = {copy};
        const std::string recipePath = utf8(dir.filePath(QStringLiteral("board.aoi.json")));
        JsonRecipeStore recipes;
        QVERIFY(recipes.save(recipe, recipePath).ok());

        QVERIFY(library.remove("lib-1").ok());
        const Result<Recipe> loaded = recipes.load(recipePath);
        QVERIFY(loaded.ok());
        QCOMPARE(loaded.value().board.types[0].libId, std::string("lib-1"));
        QCOMPARE(loaded.value().board.types[0].partNumber, std::string("8371"));
    }

    void entriesPersistAcrossInstances() {
        QTemporaryDir dir = makeTempDir();
        const std::string path = utf8(dir.filePath(QStringLiteral("library.json")));
        {
            JsonLibraryStore store(path);
            for (int i = 0; i < 20; ++i) QVERIFY(store.save(typeWithPart("P" + std::to_string(i), {"alias" + std::to_string(i)})).ok());
        }
        JsonLibraryStore store(path);
        const auto all = store.loadAll().value();
        QCOMPARE(all.size(), std::size_t{20});
        QCOMPARE(all[7].partNumber, std::string("P7"));
        QCOMPARE(store.find("ALIAS13").value()->id, std::string("lib-14"));
        const QJsonObject root = QJsonDocument::fromJson(readBytes(QString::fromStdString(path))).object();
        QCOMPARE(root.value(QStringLiteral("format")).toString(), QStringLiteral("aoi-library"));
        QCOMPARE(root.value(QStringLiteral("nextEntryNumber")).toInt(), 21);
    }

    void theV1LibraryFixtureLoads() {
        JsonLibraryStore store(utf8(fixture("library-v1.json")));
        const auto all = store.loadAll();
        QVERIFY2(all.ok(), all.ok() ? "" : all.error().message.c_str());
        QCOMPARE(all.value().size(), std::size_t{2});
        QCOMPARE(store.find("sop20").value()->id, std::string("lib-1"));
        QCOMPARE(store.find("0603").value()->id, std::string("lib-3"));
        QVERIFY(store.find("0603").value()->category == ComponentCategory::Chip);
    }

    // 損壞的檔案：每個呼叫都回 FormatInvalid，而且 save、remove 不覆蓋它（不毀掉使用者的資料）。
    void aDamagedLibraryIsReportedAndNeverOverwritten_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("not json") << QByteArray("{ broken");
        QTest::newRow("a recipe file") << readBytes(QDir(QStringLiteral(AOI_INFRA_FIXTURES)).filePath(QStringLiteral("recipe-v1.json")));
        QTest::newRow("no entries") << QByteArray(R"({"format": "aoi-library", "schemaVersion": 1, "nextEntryNumber": 1})");
        QTest::newRow("bad next number") << QByteArray(R"({"format": "aoi-library", "schemaVersion": 1, "nextEntryNumber": 0, "entries": []})");
        QTest::newRow("duplicate ids") << QByteArray(
            R"({"format": "aoi-library", "schemaVersion": 1, "nextEntryNumber": 3, "entries": [)"
            R"({"id": "lib-1", "libId": "", "partNumber": "A", "aliases": [], "name": "", "category": "chip", "groups": [], "referenceImage": null},)"
            R"({"id": "lib-1", "libId": "", "partNumber": "B", "aliases": [], "name": "", "category": "chip", "groups": [], "referenceImage": null}]})");
        QTest::newRow("newer version") << QByteArray(R"({"format": "aoi-library", "schemaVersion": 2, "nextEntryNumber": 1, "entries": []})");
    }
    void aDamagedLibraryIsReportedAndNeverOverwritten() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("library.json"));
        QVERIFY(writeBytes(path, bytes));
        JsonLibraryStore store(utf8(path));
        QCOMPARE_CODE(store.loadAll().error().code, ErrorCode::FormatInvalid);
        QCOMPARE_CODE(store.find("A").error().code, ErrorCode::FormatInvalid);
        QCOMPARE_CODE(store.save(typeWithPart("Z")).error().code, ErrorCode::FormatInvalid);
        QCOMPARE_CODE(store.remove("lib-1").error().code, ErrorCode::FormatInvalid);
        QCOMPARE(readBytes(path), bytes);
    }

    // 寫入失敗（目標唯讀）時，記憶體裡的內容也不變：之後的讀取與磁碟一致。
    void aFailedWriteLeavesTheLibraryAsItWas() {
        QTemporaryDir dir = makeTempDir();
        const QString path = dir.filePath(QStringLiteral("library.json"));
        JsonLibraryStore store(utf8(path));
        QVERIFY(store.save(typeWithPart("A")).ok());
        QVERIFY(QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser));
        const Result<void> saved = store.save(typeWithPart("B"));
        const Result<void> removed = store.remove("lib-1");
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);
        QCOMPARE_CODE(saved.error().code, ErrorCode::IoFailed);
        QCOMPARE_CODE(removed.error().code, ErrorCode::IoFailed);
        QCOMPARE(store.loadAll().value().size(), std::size_t{1});
        QVERIFY(!store.find("B").value().has_value());
        QVERIFY(store.find("A").value().has_value());
        // 能寫之後，下一個新條目拿到的是 lib-2（失敗的那次沒有吃掉號碼）
        QVERIFY(store.save(typeWithPart("C")).ok());
        QCOMPARE(store.find("C").value()->id, std::string("lib-2"));
    }

    void concurrentUseIsSafe() {
        QTemporaryDir dir = makeTempDir();
        JsonLibraryStore store(utf8(dir.filePath(QStringLiteral("library.json"))));
        std::atomic<int> failures{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&, t] {
                for (int i = 0; i < 10; ++i) {
                    if (!store.save(typeWithPart("T" + std::to_string(t) + "-" + std::to_string(i))).ok()) ++failures;
                    if (!store.find("T0-0").ok()) ++failures;
                    if (!store.loadAll().ok()) ++failures;
                }
            });
        }
        for (std::thread& th : threads) th.join();
        QCOMPARE(failures.load(), 0);
        const auto all = store.loadAll().value();
        QCOMPARE(all.size(), std::size_t{40});
        std::set<std::string> ids;
        for (const ComponentType& t : all) ids.insert(t.id);
        QCOMPARE(ids.size(), std::size_t{40});  // 沒有重複的 id
    }
};

QTEST_GUILESS_MAIN(TstLibraryStore)
#include "tst_library_store.moc"
