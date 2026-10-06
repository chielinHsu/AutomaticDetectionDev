#pragma once

#include <string_view>

#include "application/Result.h"
#include "domain/recipe.h"

// application 層：port。JSON 實作：infrastructure/storage/JsonRecipeStore（S06，Qt Core）。
// 背景存檔：application/document/RecipeSaver（S06）。
namespace aoi {

// 契約（S03 定下前兩條，S06 補完；ADR-015）：
//  - load(save(r)) 還原出相同的內容：每個欄位都相等，浮點數逐位元相同；唯一的例外是 −0.0 讀回來是 +0.0（JSON 不分正負零，
//    數值仍然相等）。
//  - 損壞的檔案回 ErrorCode::FormatInvalid，不是當機也不是例外。檔案不存在回 ErrorCode::NotFound；
//    其餘 I/O 失敗回 ErrorCode::IoFailed。path 是 UTF-8。
//  - 檔案格式有版本（schemaVersion）。save 一律寫這個程式的目前版本（不看 recipe.schemaVersion）；
//    load 讀得懂舊版本（逐版遷移），比目前版本新的檔案回 FormatInvalid；load 之後 recipe.schemaVersion 是檔案原本的版本。
//  - save 是原子的：失敗時舊檔案原封不動，不會留下寫到一半的檔案。
//  - 存不下的內容（非有限的數字、不是合法 UTF-8 的字串）回 ErrorCode::InvalidArgument，不寫檔。
//  - 可重入：可以同時被多個執行緒呼叫。同一個路徑同時存幾份時，磁碟上留下其中完整的一份，有的呼叫可能回 IoFailed
//    （Windows 上兩個換名撞在一起）；要「依序、留下最後交出的那份」用 RecipeSaver。
class IRecipeStore {
public:
    virtual ~IRecipeStore() = default;
    virtual Result<void> save(const Recipe& recipe, std::string_view path) = 0;
    virtual Result<Recipe> load(std::string_view path) = 0;
};

}  // namespace aoi
