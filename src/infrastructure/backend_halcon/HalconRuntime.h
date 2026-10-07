#pragma once

#include <optional>
#include <string>

// infrastructure/backend_halcon：執行時的 HALCON 程式庫（S08）。本檔不 include HALCON 的標頭，組裝根可以直接用。
//
// halconcpp.dll 是延遲載入的（AiInspTool/qmake/aoi_target.pri 的 /DELAYLOAD）：沒有安裝 HALCON 的電腦，程式照樣啟動。
// 代價是「第一次呼叫 HALCON 時才載入」：DLL 不在、或版本不合（少了某個函式），延遲載入會丟出結構化例外，程式當掉。
// 所以任何 HALCON 呼叫之前，先問 halconRuntimeProblem()：它在受保護的地方把 DLL 與所有匯入的函式一次載好。
namespace aoi::infra {

// 只看找不找得到 halconcpp.dll（SearchPath：執行檔旁邊、系統資料夾、PATH），不載入。微秒級，啟動時列 back end 用。
bool halconRuntimeOnPath();

// 載入 halconcpp.dll（與它依賴的 halcon.dll）並解析所有延遲載入的匯入；檢查 DLL 的版本與編譯時的標頭相同（主、次版本）。
// 有問題回原因（英文：給 log 與 ErrorCode::BackendUnavailable）；沒問題回 nullopt。
// 第一次呼叫做實際的工作（冷啟動約 0.25 秒，halcon.dll 有 80 MB），之後回快取的結果。執行緒安全。
std::optional<std::string> halconRuntimeProblem();

// 編譯時的 HALCON 標頭版本，例如 "24.11.2"。
std::string halconSdkVersion();

}  // namespace aoi::infra
