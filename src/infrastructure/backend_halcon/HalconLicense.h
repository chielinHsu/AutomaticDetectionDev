#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// infrastructure/backend_halcon：HALCON 的授權（S08）。本檔不 include HALCON 的標頭，組裝根可以直接用。
//
// HALCON 的授權檔（%HALCONROOT%\license\*.dat）綁在一個主機 ID 上（這台機器是 CodeMeter 的 dongle，ID 寫成 CM:3-…）。
// 同一個版本有兩種 dongle（使用者 2026-10-07 說明）：
//   - runtime（RTL）：部署用，可以執行 HALCON 程式；
//   - development（SDK）：開發用，另外有 HDevelop（授權檔多一個 MVTec_HDevelop 的 feature）。
// HALCON 用找得到 dongle 的那一個。兩個授權檔都在、只插了 RTL 的 dongle 時，HALCON 照樣可以跑（用 runtime 授權），
// 不會報錯 —— 所以「插錯 dongle」要我們自己看出來：get_system('current_license_info') 說用的是哪一種，
// 授權檔說有哪些 dongle，get_system('hostids') 說這台電腦現在有哪些 ID。
namespace aoi::infra {

// 一個授權檔的摘要（只讀 feature、ID、INFO；簽章不讀）。
struct HalconLicenseFile {
    std::string path;
    std::vector<std::string> features;  // 例如 MVTec_HALCON、MVTec_HDevelop
    std::vector<std::string> ids;       // 例如 CM:3-7081033（同一個檔案可以有幾行 LICENSE）
    std::string info;                   // 第一個 INFO="…"，例如 HALCON 24.11 Steady SDK, Free U
    bool development() const;           // 有 MVTec_HDevelop 的 feature
};

// 授權檔的文字 → 摘要。行尾的反斜線是續行；值可以用雙引號包住（含空白）。
HalconLicenseFile parseHalconLicenseFile(std::string path, std::string_view text);

// 探測的結果：get_system 的幾個值，與第一次呼叫運算子的成敗。
struct HalconLicenseInfo {
    std::optional<std::string> runtimeProblem;  // halconcpp.dll 載不起來（HalconRuntime.h）：這時其他欄位都沒有意義
    bool valid = false;                         // 第一個運算子（gen_image_const）成功
    long errorCode = 0;                         // valid ＝ false 時 HALCON 的錯誤碼與訊息
    std::string errorText;
    // get_system('current_license_info')：
    std::string file;        // 用的是哪一個授權檔
    std::string kind;        // 'runtime'、'development' 或 'sasl'
    std::string version;     // 授權的最高版本
    std::string expiration;  // ISO 8601 或 'permanent'
    std::string modules;     // 模組清單（' | ' 分隔）
    std::string hostId;      // 用的是哪一個主機 ID
    std::string info;        // 授權檔的 INFO
    std::string edition;     // get_system('licensed_product_edition')：Progress、Steady…
    std::vector<std::string> hostIds;        // get_system('hostids')：這台電腦現在有的 ID（dongle、網卡）
    std::vector<HalconLicenseFile> files;    // get_system('available_license_files') 的每個檔案
    bool shapeMatchingLicensed = false;      // create_shape_model、find_shape_model(s) 都不在 'unlicensed_operators'
    double checkMs = 0.0;                    // 探測的時間（第一次包含 HALCON 的初始化與授權檢查）
};

// 授權沒問題、但使用者應該知道的事。
enum class HalconLicenseNoticeKind {
    SdkDongleMissing,  // 用的是 runtime 授權；開發（SDK）授權的檔案在，但它的 dongle 沒插
};

struct HalconLicenseNotice {
    HalconLicenseNoticeKind kind = HalconLicenseNoticeKind::SdkDongleMissing;
    std::string inUseId;    // 現在用的授權的主機 ID
    std::string missingId;  // 沒插的 dongle 的 ID
    std::string message;    // 英文說明（stderr、log、測試）
};

struct HalconLicenseVerdict {
    bool usable = false;
    std::string reason;  // usable ＝ false 時的原因（英文，給 ErrorCode::BackendUnavailable）
    std::vector<HalconLicenseNotice> notices;
};

// 規則（純函式，沒有 HALCON 也能測）：
//  - 程式庫載不起來、沒有有效的授權、或授權不含 shape matching → 不能用，原因寫清楚（含找到的授權檔與這台電腦的 ID）。
//  - 能用，但用的是 runtime 授權，而且某個 development 授權檔的 ID 全都不在這台電腦上 → SdkDongleMissing。
HalconLicenseVerdict judgeHalconLicense(const HalconLicenseInfo& info);

// 現在探測一次（不快取）：載入程式庫、呼叫第一個運算子、讀 get_system 與授權檔。效能量測用。
// recheck ＝ true：先呼叫 get_system('is_license_valid')（HALCON 的說明：「立刻重新檢查授權」），之前失敗過時用。
HalconLicenseInfo probeHalconLicense(bool recheck = false);

// 「能用才記住」的授權快取（2026-10-09）。能用的結果記住，之後回同一份（授權在程式執行中很少改變；執行中失效時，
// 運算子會回授權的錯誤碼，見 HalconErrors）。**不能用的不記住**：下一次問會重新探測（recheck）。
// 注意 HALCON 自己也可能記住失敗（2026-10-09 實測：找不到授權檔的錯誤 2036 在行程裡一直留著；dongle 讀不到的錯誤 2300
// 每次呼叫都花 100–150 ms 重新找 dongle，恢復與否沒有驗證），所以組裝根在這個行程碰 HALCON 之前先用另一個行程確認授權
// （app/CompositionRoot.h 的 createHalconBackendSafely）；這個快取是第二道。
// 探測的函式可以換（測試用假的，不需要 HALCON）。執行緒安全：同時問的人等同一次探測。
class HalconLicenseCache {
public:
    using Probe = std::function<HalconLicenseInfo(bool recheck)>;
    explicit HalconLicenseCache(Probe probe);

    HalconLicenseInfo get();
    int probes() const;  // 已經探測了幾次

private:
    mutable std::mutex mutex_;
    Probe probe_;
    std::optional<HalconLicenseInfo> usable_;
    int probes_ = 0;
};

// 整個程式共用的那一個（probeHalconLicense）。第一次約 1.5 秒（載入 DLL、HALCON 初始化、授權檢查）；不能用時每次問都重新探測。
HalconLicenseInfo halconLicense();

// 這個行程已經碰過 HALCON 了嗎（halconLicense 探測過至少一次）。HALCON 第一次初始化失敗之後可能不會再讀授權（2026-10-09 實測：
// 找不到授權檔的錯誤 2036 會一直留著，改了 HALCONROOT、呼叫 is_license_valid 都一樣），所以組裝根在碰之前先用另一個行程確認。
bool halconProbedInProcess();

// 兩個主機 ID 是不是同一個（大小寫不分；CodeMeter 的 "CM:" 前綴可有可無）。
bool sameHalconHostId(std::string_view a, std::string_view b);

}  // namespace aoi::infra
