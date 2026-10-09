#include "infrastructure/backend_halcon/HalconLicense.h"

#include <halconcpp/HalconCpp.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>

#include "infrastructure/backend_halcon/HalconRuntime.h"

namespace aoi::infra {
namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string_view withoutCodeMeterPrefix(std::string_view id) {
    if (id.size() > 3 && (id[0] == 'C' || id[0] == 'c') && (id[1] == 'M' || id[1] == 'm') && id[2] == ':') id.remove_prefix(3);
    return id;
}

// HTuple 的一個元素 → 字串（字串不加引號，數字用 HALCON 的格式）。
std::string text(const HalconCpp::HTuple& t, Hlong i) {
    if (i >= t.Length()) return {};
    const HalconCpp::HTuple e = t[i];
    if (e.Type() == HalconCpp::eTupleTypeString) return e.S().Text();
    return e.ToString().Text();
}

std::vector<std::string> texts(const HalconCpp::HTuple& t) {
    std::vector<std::string> out;
    for (Hlong i = 0; i < t.Length(); ++i) out.push_back(text(t, i));
    return out;
}

// get_system 的一個值；讀不到（例如沒有授權時某些查詢會失敗）回空的 tuple。
HalconCpp::HTuple system(const char* query) {
    try {
        HalconCpp::HTuple value;
        HalconCpp::GetSystem(query, &value);
        return value;
    } catch (const HalconCpp::HException&) {
        return {};
    }
}

std::string readFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

std::string joinIds(const std::vector<std::string>& ids) {
    std::string s;
    for (const std::string& id : ids) s += (s.empty() ? "" : ", ") + id;
    return s.empty() ? std::string("(none)") : s;
}

bool present(const std::vector<std::string>& hostIds, const std::string& id) {
    return std::any_of(hostIds.begin(), hostIds.end(), [&](const std::string& h) { return sameHalconHostId(h, id); });
}

std::string environment(const char* name) {
    char* value = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&value, &size, name) != 0 || value == nullptr) return {};
    std::string out(value);
    std::free(value);
    return out;
}

// %HALCONROOT%\license 底下的 *.dat（依檔名排序）。沒有設定或讀不到時是空的。
std::vector<std::string> licenseFilesUnderHalconRoot() {
    std::vector<std::string> out;
    const std::string root = environment("HALCONROOT");
    if (root.empty()) return out;
    std::error_code ec;
    const std::filesystem::path dir = std::filesystem::path(root) / "license";
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && lower(it->path().extension().string()) == ".dat") out.push_back(it->path().string());
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

bool HalconLicenseFile::development() const {
    return std::any_of(features.begin(), features.end(), [](const std::string& f) { return lower(f) == "mvtec_hdevelop"; });
}

bool sameHalconHostId(std::string_view a, std::string_view b) {
    return !a.empty() && lower(withoutCodeMeterPrefix(a)) == lower(withoutCodeMeterPrefix(b));
}

HalconLicenseFile parseHalconLicenseFile(std::string path, std::string_view text) {
    HalconLicenseFile file;
    file.path = std::move(path);
    // 續行（行尾的反斜線）接起來；註解（#）不看。
    std::vector<std::string> logical;
    std::string current;
    const auto flush = [&] {
        if (!current.empty()) logical.push_back(current);
        current.clear();
    };
    std::istringstream lines{std::string(text)};
    std::string line;
    while (std::getline(lines, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        const std::size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] == '#') {
            flush();
            continue;
        }
        const bool continues = line.back() == '\\';
        if (continues) line.pop_back();
        current += line.substr(first) + " ";
        if (!continues) flush();
    }
    flush();

    for (const std::string& l : logical) {
        std::istringstream words(l);
        std::string keyword;
        std::string feature;
        words >> keyword >> feature;
        if (keyword != "LICENSE" || feature.empty()) continue;
        file.features.push_back(feature);
        const std::size_t id = l.find(" ID=");
        if (id != std::string::npos) {
            const std::size_t from = id + 4;
            const std::size_t to = l.find_first_of(" \t", from);
            file.ids.push_back(l.substr(from, to == std::string::npos ? std::string::npos : to - from));
        }
        const std::size_t info = l.find("INFO=\"");
        if (info != std::string::npos && file.info.empty()) {
            const std::size_t from = info + 6;
            const std::size_t to = l.find('"', from);
            file.info = l.substr(from, to == std::string::npos ? std::string::npos : to - from);
        }
    }
    return file;
}

HalconLicenseVerdict judgeHalconLicense(const HalconLicenseInfo& info) {
    HalconLicenseVerdict v;
    if (info.runtimeProblem) {
        v.reason = *info.runtimeProblem;
        return v;
    }
    // 每個授權檔：「檔名 needs ID (runtime|development)」；同一個檔案的幾行 LICENSE 通常是同一個 ID，只列一次。
    std::vector<std::string> fileIds;
    for (const HalconLicenseFile& f : info.files) {
        std::vector<std::string> ids;
        for (const std::string& id : f.ids) {
            if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.push_back(id);
        }
        const std::string name = std::filesystem::path(f.path).filename().string();
        for (const std::string& id : ids) fileIds.push_back(name + " needs " + id + (f.development() ? " (development)" : " (runtime)"));
    }
    if (!info.valid) {
        const std::string hosts = info.hostIds.empty()
                                      ? std::string("(HALCON cannot list them without a license; run hhostid.exe in %HALCONROOT%\\bin\\x64-win64)")
                                      : joinIds(info.hostIds);
        v.reason = "HALCON has no valid license (error " + std::to_string(info.errorCode) + ": " + info.errorText +
                   "). License files: " + joinIds(fileIds) + ". Host IDs of this computer: " + hosts +
                   ". Attach the dongle of one of the license files, or check %HALCONROOT%\\license.";
        return v;
    }
    if (!info.shapeMatchingLicensed) {
        v.reason = "The HALCON license in use (" + info.kind + ", " + info.hostId + ", modules " + info.modules +
                   ") does not include shape-based matching (create_shape_model, find_shape_model).";
        return v;
    }
    v.usable = true;
    if (info.kind == "runtime") {
        for (const HalconLicenseFile& f : info.files) {
            if (!f.development() || f.ids.empty()) continue;
            const bool anyAttached = std::any_of(f.ids.begin(), f.ids.end(), [&](const std::string& id) { return present(info.hostIds, id); });
            if (anyAttached) continue;
            HalconLicenseNotice n;
            n.kind = HalconLicenseNoticeKind::SdkDongleMissing;
            n.inUseId = info.hostId;
            n.missingId = f.ids.front();
            n.message = "HALCON runs with the runtime license (" + info.hostId + "). The development (SDK) license in " + f.path +
                        " needs the dongle " + n.missingId + ", which is not attached.";
            v.notices.push_back(std::move(n));
        }
    }
    return v;
}

HalconLicenseInfo probeHalconLicense(bool recheck) {
    HalconLicenseInfo info;
    const auto started = std::chrono::steady_clock::now();
    info.runtimeProblem = halconRuntimeProblem();
    if (info.runtimeProblem) return info;
    // 之前失敗過：先要 HALCON 立刻重新檢查授權（get_system 的說明：is_license_valid「triggers an immediate license check」）。
    // 結果不看：下面第一個運算子的成敗與錯誤碼才是判斷的依據。
    if (recheck) (void)system("is_license_valid");
    try {
        // 第一個運算子：HALCON 在這裡初始化並檢查授權。失敗的錯誤碼比 is_license_valid 的 'false' 有用。
        HalconCpp::HObject image;
        HalconCpp::GenImageConst(&image, "byte", 1, 1);
        info.valid = true;
    } catch (const HalconCpp::HException& e) {
        info.errorCode = static_cast<long>(e.ErrorCode());
        info.errorText = e.ErrorMessage().Text();
    }
    const HalconCpp::HTuple current = system("current_license_info");
    if (info.valid && current.Length() >= 10) {
        info.file = text(current, 0);
        info.kind = text(current, 1);
        info.version = text(current, 2);
        info.expiration = text(current, 3);
        info.modules = text(current, 4);
        info.hostId = text(current, 7);
        info.info = text(current, 9);
    }
    info.edition = text(system("licensed_product_edition"), 0);
    info.hostIds = texts(system("hostids"));
    for (const std::string& path : texts(system("available_license_files"))) {
        info.files.push_back(parseHalconLicenseFile(path, readFile(path)));
    }
    // 沒有有效授權時，get_system 查不到授權檔（S08 實測：讀不到 dongle 時是空的）。自己讀 %HALCONROOT%\license\*.dat，
    // 原因裡才寫得出「哪個檔案要哪個 dongle」。
    if (info.files.empty()) {
        for (const std::string& path : licenseFilesUnderHalconRoot()) info.files.push_back(parseHalconLicenseFile(path, readFile(path)));
    }
    if (info.valid) {
        const std::vector<std::string> unlicensed = texts(system("unlicensed_operators"));
        info.shapeMatchingLicensed = std::none_of(unlicensed.begin(), unlicensed.end(), [](const std::string& op) {
            return op == "create_shape_model" || op == "find_shape_model" || op == "find_shape_models";
        });
    }
    info.checkMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    return info;
}

HalconLicenseCache::HalconLicenseCache(Probe probe) : probe_(std::move(probe)) {}

HalconLicenseInfo HalconLicenseCache::get() {
    std::lock_guard lock(mutex_);  // 同時問的人等同一次探測（HALCON 的初始化不要並行做兩次）
    if (usable_) return *usable_;
    HalconLicenseInfo info = probe_(probes_ > 0);
    ++probes_;
    if (judgeHalconLicense(info).usable) usable_ = info;
    return info;
}

int HalconLicenseCache::probes() const {
    std::lock_guard lock(mutex_);
    return probes_;
}

namespace {

HalconLicenseCache& sharedLicenseCache() {
    static HalconLicenseCache cache([](bool recheck) { return probeHalconLicense(recheck); });
    return cache;
}

}  // namespace

HalconLicenseInfo halconLicense() { return sharedLicenseCache().get(); }

bool halconProbedInProcess() { return sharedLicenseCache().probes() > 0; }

}  // namespace aoi::infra
