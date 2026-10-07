#include "infrastructure/backend_halcon/HalconRuntime.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <delayimp.h>

#include <HVersNum.h>

#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace aoi::infra {
namespace {

constexpr wchar_t kDllW[] = L"halconcpp.dll";
constexpr char kDllA[] = "halconcpp.dll";

// 結構化例外只能在沒有 C++ 解構子的函式裡攔（MSVC 的 __try 規則），所以獨立成一個只有純量的函式。
// 回傳 0 ＝ 成功；否則是例外碼或 HRESULT。
unsigned long loadAllImports() noexcept {
    __try {
        const HRESULT hr = __HrLoadAllImportsForDll(kDllA);
        return SUCCEEDED(hr) ? 0ul : static_cast<unsigned long>(hr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return GetExceptionCode();
    }
}

std::string hex(unsigned long v) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "0x%08lX", v);
    return buf;
}

// DLL 檔案的版本（VS_FIXEDFILEINFO），讀不到回 false。
bool fileVersion(HMODULE module, unsigned* major, unsigned* minor, unsigned* revision) {
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(module, path, MAX_PATH) == 0) return false;
    DWORD ignored = 0;
    const DWORD size = GetFileVersionInfoSizeW(path, &ignored);
    if (size == 0) return false;
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) return false;
    VS_FIXEDFILEINFO* info = nullptr;
    UINT length = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || info == nullptr) return false;
    *major = HIWORD(info->dwFileVersionMS);
    *minor = LOWORD(info->dwFileVersionMS);
    *revision = HIWORD(info->dwFileVersionLS);
    return true;
}

std::optional<std::string> check() {
    if (!halconRuntimeOnPath()) {
        return std::string("The HALCON runtime (halconcpp.dll) is not on the PATH. Install HALCON ") + halconSdkVersion() +
               " or add %HALCONROOT%\\bin\\x64-win64 to the PATH.";
    }
    // 先自己載一次：失敗時 GetLastError 比延遲載入的例外碼好懂（126 ＝ 它依賴的 halcon.dll 等找不到）。
    HMODULE module = LoadLibraryW(kDllW);
    if (module == nullptr) {
        return "Cannot load halconcpp.dll (Windows error " + std::to_string(GetLastError()) +
               "). A DLL it depends on (halcon.dll) may be missing.";
    }
    unsigned major = 0;
    unsigned minor = 0;
    unsigned revision = 0;
    if (fileVersion(module, &major, &minor, &revision) && (major != HLIB_MAJOR_NUM || minor != HLIB_MINOR_NUM)) {
        return "halconcpp.dll is version " + std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(revision) +
               ", but this program was built with the HALCON " + halconSdkVersion() + " headers. Install HALCON " +
               std::to_string(HLIB_MAJOR_NUM) + "." + std::to_string(HLIB_MINOR_NUM) + ".";
    }
    // 延遲載入的匯入一次解析完：少了任何一個函式，就在這裡（受保護）失敗，而不是之後在搜尋中當掉。
    const unsigned long code = loadAllImports();
    if (code != 0) {
        return "Cannot bind the functions of halconcpp.dll (" + hex(code) + "). The HALCON runtime does not match the headers.";
    }
    return std::nullopt;
}

}  // namespace

bool halconRuntimeOnPath() {
    wchar_t found[MAX_PATH] = {};
    return SearchPathW(nullptr, kDllW, nullptr, MAX_PATH, found, nullptr) != 0;
}

std::optional<std::string> halconRuntimeProblem() {
    static std::once_flag once;
    static std::optional<std::string> problem;
    std::call_once(once, [] { problem = check(); });
    return problem;
}

std::string halconSdkVersion() {
    return std::to_string(HLIB_MAJOR_NUM) + "." + std::to_string(HLIB_MINOR_NUM) + "." + std::to_string(HLIB_REVISION_NUM);
}

}  // namespace aoi::infra
