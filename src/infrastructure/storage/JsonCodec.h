#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <string>

#include "application/Result.h"
#include "domain/component.h"
#include "domain/image_view.h"
#include "domain/search_params.h"

// infrastructure/storage：Qt Core。domain 型別與 JSON 之間的轉換（S06，ADR-015）；配方與樣本庫共用。
//
// 格式的原則：
//  - 每個欄位都寫（包括目前用不到的，例如 Body 群組的 matrix），load(save(x)) 才能還原出完全相同的內容。
//  - 列舉寫成小寫英文字串（"found"、"pin"…），不寫數字：檔案看得懂，列舉的順序改了也不會讀錯。
//  - 長度單位照 domain：類型與框群組是 µm（欄位名稱以 Um 結尾），元件位姿是影像像素。
//  - 參考影像：{width, height, format, umPerPixel, significantBits, encoding: "base64", data}，像素逐列緊密排列（Gray16 是小端）、
//    不壓縮直接 base64；沒有參考影像寫 null。不用 qCompress：Qt 6.8.0 的 qUncompress 遇到截斷的資料會撞上
//    Q_UNREACHABLE（ADR-015），損壞的檔案不可以讓程式當掉。
//  - 讀的時候嚴格檢查：缺欄位、型別不對、列舉不認得、數字不是有限值、整數不是整數或超出範圍、參照不存在，
//    都回 FormatInvalid，訊息帶 JSON 路徑（例如 board.components[12].pose.x）。不認得的多餘欄位略過。
//  - 寫的時候檢查存不存得下：非有限的數字、不是合法 UTF-8 的字串、元件參照不存在的類型 → InvalidArgument。
namespace aoi::infra::json {

Result<QJsonObject> encodeBoard(const Board& board);
Result<QJsonObject> encodeComponentType(const ComponentType& type);
Result<QJsonObject> encodeSearchParams(const SearchParams& params);
// 參考影像；無效的 view（沒有參考影像）寫成 null。
Result<QJsonValue> encodeImage(const ImageView& image);

// where 是這個值在檔案裡的路徑，只用在錯誤訊息。
Result<Board> decodeBoard(const QJsonValue& value, const std::string& where);
Result<ComponentType> decodeComponentType(const QJsonValue& value, const std::string& where);
Result<SearchParams> decodeSearchParams(const QJsonValue& value, const std::string& where);
Result<ImageView> decodeImage(const QJsonValue& value, const std::string& where);

// 遷移用（ADR-026：配方 v2 → v3、樣本庫 v1 → v2）：陣列裡每個類型物件沒有 "match" 的，補上整框、灰階的預設值。
// 不是物件的元素照原樣（留給解碼器報錯）。
QJsonArray withDefaultMatch(const QJsonArray& types);

// 參考影像的限制（防止損壞或惡意的檔案要求配置巨大的記憶體）。
inline constexpr int kMaxReferenceSide = 16384;
inline constexpr std::size_t kMaxReferenceBytes = std::size_t{256} << 20;  // 256 MiB

}  // namespace aoi::infra::json
