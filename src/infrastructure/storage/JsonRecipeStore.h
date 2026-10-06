#pragma once

#include <vector>

#include "application/ports/IRecipeStore.h"
#include "infrastructure/storage/JsonFile.h"

// infrastructure/storage：Qt Core。
namespace aoi::infra {

// IRecipeStore 的 JSON 實作（S06，ADR-015；契約見 IRecipeStore.h）。
//
// 檔案（UTF-8，縮排）：
//   { "format": "aoi-recipe", "schemaVersion": 2,
//     "board": { name, imageWidth, imageHeight, umPerPixel, significantBits, types: [...], components: [...] },
//     "params": { minScore, angleMinDeg, angleMaxDeg, maxMatches } }
// 欄位的寫法見 JsonCodec.h；範例是 tests/infra/fixtures/recipe-v1.json、recipe-v2.json（它們要永遠讀得進來，不要改）。
//
// 版本：
//   v1（S06）第一版。
//   v2（S07）board 多了 significantBits（影像的有效位元數：0 ＝ 自動，8..16 ＝ 使用者指定；Board::significantBits）。
//       v1 → v2 的遷移補 0（v1 的時候只有自動）。
// 遷移掛勾：格式改了，kSchemaVersion 加 1，並在 builtInMigrations() 加一個「舊版 → 新版」的函式（改 JSON，不改 domain）；
// 舊檔案讀進來時逐版升級。
//
// 可重入：沒有成員狀態（每次呼叫自己讀寫檔案）。
class JsonRecipeStore final : public IRecipeStore {
public:
    static constexpr int kSchemaVersion = 2;
    static constexpr const char* kFormat = "aoi-recipe";

    JsonRecipeStore();
    // 測試遷移機制用：current 是「這個程式的版本」、migrations[k − 1] 把 k 版升成 k + 1 版。
    // 注意：寫出的內容永遠是 kSchemaVersion 的格式（編碼器只有一個版本），所以 current ≠ kSchemaVersion 只用來測試讀取。
    JsonRecipeStore(int current, std::vector<json::Migration> migrations);

    Result<void> save(const Recipe& recipe, std::string_view path) override;
    Result<Recipe> load(std::string_view path) override;

    static std::vector<json::Migration> builtInMigrations();

private:
    int current_;
    std::vector<json::Migration> migrations_;
};

}  // namespace aoi::infra
