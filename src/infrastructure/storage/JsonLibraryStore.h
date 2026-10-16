#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "application/ports/ILibraryStore.h"

// infrastructure/storage：Qt Core。
namespace aoi::infra {

// ILibraryStore 的 JSON 實作：一個檔案就是一層樣本庫（S06，ADR-015；契約見 ILibraryStore.h）。
// 幾層（專案、使用者、系統）的組合在 LayeredLibraryStore（使用者決定照 TRI 的做法分層，2026-10-06）。
//
// 檔案（UTF-8，縮排）：
//   { "format": "aoi-library", "schemaVersion": 1, "nextEntryNumber": 3, "entries": [ ComponentType... ] }
//
// 規則：
//  - 條目的 id 由庫指派（idPrefix ＋ 流水號，例如 "lib-1"、"user-2"），**永不重複使用**：刪掉的 id 不會給下一個新條目
//    （板子的複本用 libId 記得來源，重複使用會讓舊板子指到別的類型）。下一個號碼存在檔案裡。
//  - save：以料號辨識（去掉前後空白、不分大小寫）。同料號就覆寫內容、保留原本的 id 與位置；否則新增在最後。
//    存進庫的條目 libId 一律是空字串（庫是母版，不是從別處複製來的）。料號是空的 → InvalidArgument。
//    存完之後用 find(料號) 取回條目（含 id）。
//  - find：先比料號，再比別名（去掉前後空白、Unicode 不分大小寫）；同一個別名出現在幾個條目時，取檔案裡最前面的。
//    不分大小寫用 Qt 的 toCaseFolded，是「簡單折疊」（一個字元對一個字元）：Ä ＝ ä，但 ß ≠ ss。
//  - 每次 save、remove 都整份原子寫入；寫入失敗時記憶體裡的內容也不變（和磁碟一致）。
//  - 檔案不存在 ＝ 空的庫（不是錯誤）；第一次 save 時建立。檔案損壞時每個呼叫都回 FormatInvalid，
//    而且 save、remove **不會**覆蓋它（不毀掉使用者的資料）。
//  - 第一次呼叫時讀檔，之後用記憶體裡的內容：別的程式同時改這個檔案，這裡不會知道（v0.1 的限制）。
//  - 執行緒安全（內部有鎖）。
class JsonLibraryStore final : public ILibraryStore {
public:
    // v2（ADR-028）：每個條目的類型多了 match；v1 → v2 的遷移補整框、灰階。
    static constexpr int kSchemaVersion = 2;
    static constexpr const char* kFormat = "aoi-library";

    // idPrefix：這一層指派的 id 的開頭（分層時每層不同，id 才不會撞在一起）。
    explicit JsonLibraryStore(std::string path, std::string idPrefix = "lib-");

    Result<std::vector<ComponentType>> loadAll() override;
    Result<std::optional<ComponentType>> find(std::string_view partNumberOrAlias) override;
    Result<void> save(const ComponentType& type) override;
    Result<void> remove(std::string_view libId) override;

    // 只比料號（不比別名）：分層時判斷「這一層有沒有這個料號」。
    Result<std::optional<ComponentType>> findPart(std::string_view partNumber);
    // 這一層有沒有這個 id 的條目。
    Result<bool> hasEntry(std::string_view libId);

    const std::string& path() const noexcept { return path_; }

private:
    struct Entry {
        ComponentType type;
        std::string partKey;                 // 正規化的料號
        std::vector<std::string> aliasKeys;  // 正規化的別名
    };

    Result<void> ensureLoaded();
    Result<void> write(const std::vector<Entry>& entries, int nextEntryNumber) const;
    static Entry makeEntry(ComponentType type);

    std::string path_;
    std::string idPrefix_;
    std::mutex mutex_;
    bool loaded_ = false;
    std::vector<Entry> entries_;
    int nextEntryNumber_ = 1;
};

}  // namespace aoi::infra
