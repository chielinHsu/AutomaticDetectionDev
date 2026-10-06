#pragma once

#include <QJsonObject>
#include <QtGlobal>
#include <functional>
#include <string_view>
#include <vector>

#include "application/Result.h"

// infrastructure/storage：Qt Core。有版本的 JSON 文件檔：讀、原子寫入、逐版升級（S06，ADR-015）。
// 配方（"aoi-recipe"）與樣本庫（"aoi-library"）共用。頂層一定是物件，帶兩個欄位：
//   "format": 文件的種類（拿樣本庫的檔案當配方開，會被認出來）
//   "schemaVersion": 正整數；程式能讀所有舊版本，比自己新的不讀
namespace aoi::infra::json {

inline constexpr qint64 kMaxFileBytes = qint64{1} << 30;  // 1 GiB：超過的不是我們寫的檔案

// 讀一個文件檔。檔案不存在 → NotFound；是資料夾、打不開、讀不完 → IoFailed；
// 太大、不是 JSON、頂層不是物件、format 不對 → FormatInvalid。開頭的 UTF-8 BOM 略過（記事本存過的檔案）。
Result<QJsonObject> readDocument(std::string_view path, std::string_view format);

// 原子寫入（QSaveFile：先寫暫存檔，全部成功才換名）：失敗時舊檔案原封不動，不會留下寫到一半的檔案。
// 失敗（資料夾不存在、沒有權限、磁碟滿、目標是唯讀）→ IoFailed。縮排輸出，方便比對與人工檢查。
Result<void> writeDocument(std::string_view path, const QJsonObject& root);

// 一次升一版：migrations[k − 1] 把 k 版的 root 改成 k + 1 版（改欄位名稱、補新欄位…）。
using Migration = std::function<Result<void>(QJsonObject& root)>;

// 讀 root 的 schemaVersion，逐版升到 current（每升一版更新 root 的 schemaVersion），回傳檔案原本的版本。
// 版本不是正整數、比 current 新、或缺少某一版的遷移 → FormatInvalid；遷移本身失敗 → 它回的錯誤（改成 FormatInvalid）。
Result<int> migrate(QJsonObject& root, int current, const std::vector<Migration>& migrations);

}  // namespace aoi::infra::json
