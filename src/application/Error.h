#pragma once

#include <string>
#include <string_view>
#include <utility>

// application 層：只依賴 domain 與標準庫（不 include Qt、OpenCV、HALCON）。
//
// 錯誤模型：port 的方法用 Result<T>（見 Result.h）回報失敗，**不讓例外穿過 port**。
// adapter 在邊界把函式庫丟出的例外轉成 Error（通常是 BackendFailed 或 Internal）。
// 「搜尋沒有結果」不是錯誤：它是正常結果（空的候選＋NoMatchInfo，見 domain/object_state.h）。
namespace aoi {

enum class ErrorCode {
    Cancelled,           // 呼叫端取消了這項工作
    InvalidArgument,     // 輸入不合法：影像無效、矩形不合法、參數超出範圍
    BackendUnavailable,  // back end 沒有編進程式，或沒有授權
    BackendMismatch,     // 範本與搜尋器不是同一個 back end 建的
    BackendFailed,       // 視覺函式庫回報失敗
    IoFailed,            // 檔案讀寫失敗
    FormatInvalid,       // 檔案內容不合法（Recipe、樣本庫）
    NotFound,            // 找不到指定的項目
    Internal,            // 未預期的例外被邊界攔下，或程式錯誤
};

constexpr std::string_view toString(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Cancelled: return "Cancelled";
        case ErrorCode::InvalidArgument: return "InvalidArgument";
        case ErrorCode::BackendUnavailable: return "BackendUnavailable";
        case ErrorCode::BackendMismatch: return "BackendMismatch";
        case ErrorCode::BackendFailed: return "BackendFailed";
        case ErrorCode::IoFailed: return "IoFailed";
        case ErrorCode::FormatInvalid: return "FormatInvalid";
        case ErrorCode::NotFound: return "NotFound";
        case ErrorCode::Internal: return "Internal";
    }
    return "Unknown";
}

struct Error {
    ErrorCode code = ErrorCode::Internal;
    // 給開發者看的英文說明（log 與測試用）。使用者看到的文案由 presentation 依 code 從 copy.json 取。
    std::string message;
};

inline Error makeError(ErrorCode code, std::string message) { return Error{code, std::move(message)}; }

}  // namespace aoi
