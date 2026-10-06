#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "application/Result.h"
#include "domain/component.h"

// application 層：port。實作：infrastructure/storage 的 JsonLibraryStore（一個檔案 ＝ 一層）與
// LayeredLibraryStore（TRI 的分層：專案 → 使用者 → 系統，使用者 2026-10-06 決定；ADR-006 §4、ADR-015）。
namespace aoi {

// 契約：
//  - find 先比料號（主名稱），再比別名（不分大小寫）；找不到回成功的 nullopt，不是錯誤。
//    分層時一層一層往下找，上層先（LayeredLibraryStore 的說明）。
//  - save 以料號辨識條目：同料號覆寫（存入樣本庫是明確的動作，ADR-006 §5）。條目的 id 由庫指派、永不重複使用；
//    存完用 find(料號) 取回條目（含 id）。分層時存到哪一層見 LayeredLibraryStore。
//  - remove 不影響已有的板子（板子有自己的複本）。條目不存在回 ErrorCode::NotFound。
//  - 庫存的長度單位是 µm（domain/component.h）。不丟例外。檔案損壞回 ErrorCode::FormatInvalid，而且不覆蓋它。
class ILibraryStore {
public:
    virtual ~ILibraryStore() = default;
    virtual Result<std::vector<ComponentType>> loadAll() = 0;
    virtual Result<std::optional<ComponentType>> find(std::string_view partNumberOrAlias) = 0;
    virtual Result<void> save(const ComponentType& type) = 0;
    virtual Result<void> remove(std::string_view libId) = 0;
};

}  // namespace aoi
