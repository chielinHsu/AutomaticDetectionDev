#pragma once

#include <string_view>

#include "application/Result.h"
#include "domain/recipe.h"

// application 層：port。JSON 實作在 S06（Qt Core）。
namespace aoi {

// 契約：load(save(r)) 還原出相同的內容。損壞的檔案回 ErrorCode::FormatInvalid，不是當機也不是例外。
// 檔案不存在回 ErrorCode::NotFound。path 是 UTF-8。
class IRecipeStore {
public:
    virtual ~IRecipeStore() = default;
    virtual Result<void> save(const Recipe& recipe, std::string_view path) = 0;
    virtual Result<Recipe> load(std::string_view path) = 0;
};

}  // namespace aoi
