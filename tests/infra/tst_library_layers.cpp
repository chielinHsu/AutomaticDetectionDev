// S06：LayeredLibraryStore（TRI 的分層：專案 → 使用者 → 系統；使用者 2026-10-06 決定）的規則。
// 每個測試開三個檔案：系統層先寫好（唯讀），專案層與使用者層由測試寫入。
#include <QDir>
#include <QtTest>
#include <memory>
#include <set>

#include "InfraTestSupport.h"
#include "QtTestPrinters.h"
#include "RecipeTestSupport.h"
#include "infrastructure/storage/JsonLibraryStore.h"
#include "infrastructure/storage/LayeredLibraryStore.h"

using namespace aoi;
using namespace aoi::infra;
using namespace aoi::test;

namespace {

ComponentType entry(const std::string& partNumber, std::vector<std::string> aliases = {}, const std::string& name = "") {
    ComponentType t = richType("x", 2);
    t.partNumber = partNumber;
    t.aliases = std::move(aliases);
    t.name = name.empty() ? partNumber : name;
    t.libId.clear();
    return t;
}

struct Layers {
    QTemporaryDir dir = makeTempDir();
    std::string project = utf8(dir.filePath(QStringLiteral("project.json")));
    std::string user = utf8(dir.filePath(QStringLiteral("user.json")));
    std::string system = utf8(dir.filePath(QStringLiteral("system.json")));
    std::unique_ptr<LayeredLibraryStore> store;

    Layers() {
        // 系統層（原廠）：8371、0603，唯讀。直接用一層的 store 寫好檔案。
        JsonLibraryStore factory(system, "sys-");
        (void)factory.save(entry("8371", {"SOP20"}, "8371 from the factory"));
        (void)factory.save(entry("0603", {}, "0603 from the factory"));
        store = LayeredLibraryStore::standard(project, user, system);
    }
    std::string nameOf(std::string_view key) { return store->find(key).value().value().name; }
};

}  // namespace

class TstLibraryLayers : public QObject {
    Q_OBJECT

private slots:
    void theStandardLayersAreProjectUserSystem() {
        Layers l;
        QCOMPARE(l.store->layerCount(), std::size_t{3});
        QCOMPARE(LayeredLibraryStore::standard("", l.user)->layerCount(), std::size_t{1});  // 沒有專案、沒有系統層
        QCOMPARE(l.nameOf("8371"), std::string("8371 from the factory"));
        QCOMPARE(l.store->layerOf("sys-1").value().value(), std::string("system"));
    }

    // 查找順序：專案 → 使用者 → 系統；上層遮住下層的同料號。
    void upperLayersShadowLowerOnes() {
        Layers l;
        QVERIFY(l.store->save(entry("8371", {}, "8371 of the user")).ok());  // 新的 → 使用者層
        QCOMPARE(l.nameOf("8371"), std::string("8371 of the user"));
        // 專案層也有一份（例如跟著程式資料夾帶來的）
        JsonLibraryStore(l.project, "proj-").save(entry("8371", {}, "8371 of the project"));
        auto fresh = LayeredLibraryStore::standard(l.project, l.user, l.system);
        QCOMPARE(fresh->find("8371").value()->name, std::string("8371 of the project"));
        QCOMPARE(fresh->find("0603").value()->name, std::string("0603 from the factory"));  // 上層沒有：用系統層的

        const auto all = fresh->loadAll().value();
        QCOMPARE(all.size(), std::size_t{2});  // 8371 只出現一次（最上層的）、0603
        QCOMPARE(all[0].name, std::string("8371 of the project"));
        QCOMPARE(all[1].name, std::string("0603 from the factory"));
    }

    // 上層的別名也勝過下層的料號（一層一層找，第一個找到的就回傳）。
    void anAliasInAnUpperLayerWinsOverAPartNumberBelow() {
        Layers l;
        QVERIFY(l.store->save(entry("U-777", {"0603"}, "user alias 0603")).ok());
        QCOMPARE(l.nameOf("0603"), std::string("user alias 0603"));
        QCOMPARE(l.nameOf("sop20"), std::string("8371 from the factory"));  // 系統層的別名
    }

    // 存檔規則 3：新的類型 → 使用者層（id 用使用者層的前綴），系統層的檔案不動。
    void aNewTypeGoesToTheUserLayer() {
        Layers l;
        const QByteArray systemBefore = readBytes(QString::fromStdString(l.system));
        QVERIFY(l.store->save(entry("R-100")).ok());
        const ComponentType saved = *l.store->find("R-100").value();
        QCOMPARE(saved.id, std::string("user-1"));
        QCOMPARE(l.store->layerOf(saved.id).value().value(), std::string("user"));
        QCOMPARE(readBytes(QString::fromStdString(l.system)), systemBefore);
    }

    // 系統層唯讀：改一個系統層的類型再存，存到使用者層（遮住系統層的那一份）；系統層的檔案不動。
    void editingAFactoryTypeSavesAUserCopy() {
        Layers l;
        const QByteArray systemBefore = readBytes(QString::fromStdString(l.system));
        ComponentType t = *l.store->find("0603").value();
        QCOMPARE(t.id, std::string("sys-2"));
        t.libId = t.id;  // 板子的複本記得來源
        t.name = "0603, my version";
        QVERIFY(l.store->save(t).ok());
        const ComponentType now = *l.store->find("0603").value();
        QCOMPARE(now.name, std::string("0603, my version"));
        QCOMPARE(l.store->layerOf(now.id).value().value(), std::string("user"));
        QCOMPARE(readBytes(QString::fromStdString(l.system)), systemBefore);
    }

    // 存檔規則 2（TRI）：從專案層載入的存回專案層。
    void aTypeLoadedFromTheProjectGoesBackToTheProject() {
        Layers l;
        JsonLibraryStore(l.project, "proj-").save(entry("IC-1", {}, "project IC"));
        auto store = LayeredLibraryStore::standard(l.project, l.user, l.system);
        ComponentType copy = *store->find("IC-1").value();
        QCOMPARE(copy.id, std::string("proj-1"));
        copy.libId = copy.id;
        copy.id = "T7";  // 板子上的 id
        copy.name = "project IC, edited on the board";
        QVERIFY(store->save(copy).ok());
        QCOMPARE(store->find("IC-1").value()->id, std::string("proj-1"));  // 覆寫專案層的條目，id 不變
        QCOMPARE(store->find("IC-1").value()->name, std::string("project IC, edited on the board"));
        QVERIFY(!JsonLibraryStore(l.user, "user-").findPart("IC-1").value().has_value());  // 使用者層沒有多一份
    }

    // 存檔規則 2 的變化：從專案層來、但改了料號 → 存在專案層的新條目（專案層的前綴）。
    void aRenamedProjectTypeStaysInTheProject() {
        Layers l;
        JsonLibraryStore(l.project, "proj-").save(entry("IC-1"));
        auto store = LayeredLibraryStore::standard(l.project, l.user, l.system);
        ComponentType copy = *store->find("IC-1").value();
        copy.libId = copy.id;
        copy.partNumber = "IC-2";
        QVERIFY(store->save(copy).ok());
        QCOMPARE(store->find("IC-2").value()->id, std::string("proj-2"));
        QCOMPARE(store->layerOf("proj-2").value().value(), std::string("project"));
    }

    // 存檔規則 1：已經有這個料號的最高可寫層。新的類型（沒有 libId）但專案層已經有同料號 → 覆寫專案層的，
    // 不在使用者層多一份被遮住的（否則存完 find 回傳的不是剛存的）。
    void savingKeepsFindConsistent() {
        Layers l;
        JsonLibraryStore(l.project, "proj-").save(entry("IC-9", {}, "old"));
        auto store = LayeredLibraryStore::standard(l.project, l.user, l.system);
        QVERIFY(store->save(entry("ic-9 ", {}, "new")).ok());  // 料號正規化之後相同
        QCOMPARE(store->find("IC-9").value()->name, std::string("new"));
        QCOMPARE(store->find("IC-9").value()->id, std::string("proj-1"));
        QVERIFY(!JsonLibraryStore(l.user, "user-").findPart("IC-9").value().has_value());
    }

    void removingFollowsTheLayer() {
        Layers l;
        QVERIFY(l.store->save(entry("R-1")).ok());
        QVERIFY(l.store->remove("user-1").ok());
        QVERIFY(!l.store->find("R-1").value().has_value());
        const Result<void> factory = l.store->remove("sys-1");  // 唯讀層
        QVERIFY(!factory.ok());
        QCOMPARE_CODE(factory.error().code, ErrorCode::InvalidArgument);
        QVERIFY(l.store->find("8371").value().has_value());
        QCOMPARE_CODE(l.store->remove("user-99").error().code, ErrorCode::NotFound);
    }

    // 損壞的一層：用到它的呼叫回錯誤（訊息帶層名），不悄悄略過；其他層的檔案不動。
    void aDamagedLayerIsReportedWithItsName() {
        Layers l;
        QVERIFY(writeBytes(QString::fromStdString(l.project), "{ not json"));
        auto store = LayeredLibraryStore::standard(l.project, l.user, l.system);
        const auto found = store->find("8371");
        QVERIFY(!found.ok());
        QCOMPARE_CODE(found.error().code, ErrorCode::FormatInvalid);
        QVERIFY2(found.error().message.find("project library") != std::string::npos, found.error().message.c_str());
        QCOMPARE_CODE(store->save(entry("NEW")).error().code, ErrorCode::FormatInvalid);
        QVERIFY(!QFile::exists(QString::fromStdString(l.user)));  // 使用者層沒有被寫
        QCOMPARE(readBytes(QString::fromStdString(l.project)), QByteArray("{ not json"));
    }

    void idsAreUniqueAcrossLayers() {
        Layers l;
        JsonLibraryStore(l.project, "proj-").save(entry("P-1"));
        auto store = LayeredLibraryStore::standard(l.project, l.user, l.system);
        QVERIFY(store->save(entry("U-1")).ok());
        std::set<std::string> ids;
        const std::vector<ComponentType> all = store->loadAll().value();  // 先存起來：range-for 不會延長 Result 暫存的生命
        for (const ComponentType& t : all) QVERIFY2(ids.insert(t.id).second, t.id.c_str());
        QCOMPARE(ids.size(), std::size_t{4});  // proj-1、user-1、sys-1、sys-2
    }
};

QTEST_GUILESS_MAIN(TstLibraryLayers)
#include "tst_library_layers.moc"
