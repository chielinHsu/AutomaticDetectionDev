#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/ILibraryStore.h"
#include "infrastructure/storage/JsonLibraryStore.h"

// infrastructure/storage：Qt Core。
namespace aoi::infra {

// 一層樣本庫：一個 JsonLibraryStore 檔案。
struct LibraryLayer {
    std::string name;      // "project"、"user"、"system"（訊息與 layerOf 用）
    std::string path;      // 檔案（UTF-8）；不存在 ＝ 空的一層
    std::string idPrefix;  // 這一層指派的 id 的開頭："proj-"、"user-"、"sys-"（id 跨層不重複）
    bool writable = true;  // 系統層（原廠）唯讀
};

// 分層的樣本庫（S06；使用者決定照 TRI 的做法，2026-10-06；ADR-015）。ILibraryStore 的契約之外，加上分層的規則：
//
// TRI（TR7700 手冊 §4.9.1、§4.9.7.1）：三層 System（原廠、唯讀）、Project（和程式放在一起）、User（使用者自己的，
// 可以複製到別台）；查找順序 Project → User → System；存檔時，從 Project 載入的存回 Project，其他存到 User。
//
//  - 層的順序就是查找順序（standard() 是 TRI 的 專案 → 使用者 → 系統）。
//  - find：一層一層往下找；同一層先比料號、再比別名；第一個找到的就回傳（上層的別名也勝過下層的料號）。
//  - loadAll：所有層的條目，依層的順序；同一個料號（正規化之後）出現在好幾層時，只留最上層的那一個（下層的被遮住）。
//  - save：存到哪一層——
//      1. 已經有這個料號的最高可寫層（存完 find(料號) 一定回傳剛存的內容）；
//      2. 都沒有：type.libId 指到的那一層，如果它可寫（TRI：從 Project 載入的存回 Project）；
//      3. 其他（新的類型、從唯讀的系統層來的）：預設層（standard() 是使用者層；TRI：其他存到 User）。
//    層內照 JsonLibraryStore 的規則：同料號覆寫、保留 id；否則新增，id 用那一層的前綴。
//    最上層的同料號在唯讀層時（不會發生在 standard() 的順序裡）照 2、3 處理。
//  - remove：刪掉 libId 所在那一層的條目；在唯讀層 → InvalidArgument（檔案不動）；找不到 → NotFound。
//  - 任何一層的檔案損壞：用到那一層的呼叫回它的錯誤，訊息帶層的名稱；不會略過它（略過會讓查找悄悄換成下一層的條目）。
//  - 執行緒安全：所有呼叫在同一把鎖裡（「先看哪一層有、再寫」是一個動作）。
class LayeredLibraryStore final : public ILibraryStore {
public:
    // layers 依查找順序；defaultLayer 是新類型存去的那一層（要可寫）。
    LayeredLibraryStore(std::vector<LibraryLayer> layers, std::size_t defaultLayer);

    // TRI 的三層：專案 → 使用者 → 系統。projectPath 空字串 ＝ 沒有開啟專案（沒有專案層）；systemPath 空字串 ＝ 沒有系統層。
    // 預設層是使用者層。
    static std::unique_ptr<LayeredLibraryStore> standard(std::string projectPath, std::string userPath, std::string systemPath = {});

    Result<std::vector<ComponentType>> loadAll() override;
    Result<std::optional<ComponentType>> find(std::string_view partNumberOrAlias) override;
    Result<void> save(const ComponentType& type) override;
    Result<void> remove(std::string_view libId) override;

    // 條目在哪一層（層的名稱）；不在任何一層回成功的 nullopt。給 UI 顯示「專案／使用者／系統」。
    Result<std::optional<std::string>> layerOf(std::string_view libId);
    std::size_t layerCount() const noexcept { return layers_.size(); }

private:
    struct Layer {
        LibraryLayer config;
        std::unique_ptr<JsonLibraryStore> store;
    };
    static Error withLayer(const Layer& layer, Error e);
    // 有 libId 的那一層的索引；找不到回 nullopt。
    Result<std::optional<std::size_t>> layerIndexOf(std::string_view libId);

    std::vector<Layer> layers_;
    std::size_t defaultLayer_;
    std::mutex mutex_;
};

}  // namespace aoi::infra
