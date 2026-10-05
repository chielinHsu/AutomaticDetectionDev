#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "application/Result.h"
#include "domain/component.h"

// application 層：port。實作在 S06（Project＋User 兩層，料號與別名索引；ADR-006 §4）。
namespace aoi {

// 契約：
//  - find 先比料號（主名稱），再比別名（不分大小寫）；找不到回成功的 nullopt，不是錯誤。
//  - save 以料號辨識條目：同料號覆寫（存入樣本庫是明確的動作，ADR-006 §5）。
//  - remove 不影響已有的板子（板子有自己的複本）。條目不存在回 ErrorCode::NotFound。
//  - 庫存的長度單位是 µm（domain/component.h）。不丟例外。
class ILibraryStore {
public:
    virtual ~ILibraryStore() = default;
    virtual Result<std::vector<ComponentType>> loadAll() = 0;
    virtual Result<std::optional<ComponentType>> find(std::string_view partNumberOrAlias) = 0;
    virtual Result<void> save(const ComponentType& type) = 0;
    virtual Result<void> remove(std::string_view libId) = 0;
};

}  // namespace aoi
