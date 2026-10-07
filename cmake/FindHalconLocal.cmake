# HALCON 的 C++ 介面（S08）。只在 AOI_WITH_HALCON=ON 時 include。
#
# 編譯要兩樣東西，執行要另外兩樣，來源不同：
#   編譯：標頭（include/、include/halconcpp/）與匯入程式庫（lib/x64-win64/halconcpp.lib）
#         → 預設用 repo 裡的 third_party/halcon（從使用者的 CV_Algorithm 專案複製，見那裡的 README.md）；
#           AOI_HALCON_SDK_DIR 可以指到別的 SDK（例如裝了開發元件的 %HALCONROOT%）。
#   執行：halconcpp.dll、halcon.dll（%HALCONROOT%\bin\x64-win64，安裝程式會放進 PATH）與授權。
#
# halconcpp.dll 用延遲載入（/DELAYLOAD）：沒有安裝 HALCON 的電腦，程式照樣啟動（只是 halcon 不可用），
# 不會在啟動時就因為找不到 DLL 而被 Windows 擋下。第一次呼叫 HALCON 之前，adapter 先確認 DLL 載得起來（HalconRuntime.h）。
#
# 結果：imported target Halcon::HalconCpp，以及
#   HALCON_SDK_VERSION    標頭的版本（HVersNum.h，例如 24.11.2）
#   HALCON_RUNTIME_DIR    %HALCONROOT%\bin\<arch>（找不到時是空的：可以建置，但測試會跳過）
#   HALCON_RUNTIME_VERSION  %HALCONROOT%\version.txt（例如 24.11.2.0）

set(AOI_HALCON_SDK_DIR "${PROJECT_SOURCE_DIR}/third_party/halcon" CACHE PATH
    "HALCON SDK with include/ and lib/x64-win64/ (default: the copy in third_party/halcon)")

find_path(HALCON_INCLUDE_DIR halconcpp/HalconCpp.h
  PATHS "${AOI_HALCON_SDK_DIR}/include" "$ENV{HALCONROOT}/include"
  NO_DEFAULT_PATH)
find_library(HALCON_CPP_LIBRARY halconcpp
  PATHS "${AOI_HALCON_SDK_DIR}/lib/x64-win64" "$ENV{HALCONROOT}/lib/x64-win64"
  NO_DEFAULT_PATH)

if(NOT HALCON_INCLUDE_DIR OR NOT HALCON_CPP_LIBRARY)
  message(FATAL_ERROR
    "AOI_WITH_HALCON=ON, but the HALCON C++ interface was not found "
    "(looked for include/halconcpp/HalconCpp.h and lib/x64-win64/halconcpp.lib in "
    "${AOI_HALCON_SDK_DIR} and $ENV{HALCONROOT}). Set AOI_HALCON_SDK_DIR to a HALCON SDK.")
endif()

# 標頭的版本。
file(STRINGS "${HALCON_INCLUDE_DIR}/HVersNum.h" _aoi_hv REGEX "define HLIB_(MAJOR|MINOR|REVISION)_NUM ")
string(REGEX REPLACE ".*HLIB_MAJOR_NUM ([0-9]+).*" "\\1" _aoi_hv_major "${_aoi_hv}")
string(REGEX REPLACE ".*HLIB_MINOR_NUM ([0-9]+).*" "\\1" _aoi_hv_minor "${_aoi_hv}")
string(REGEX REPLACE ".*HLIB_REVISION_NUM ([0-9]+).*" "\\1" _aoi_hv_rev "${_aoi_hv}")
set(HALCON_SDK_VERSION "${_aoi_hv_major}.${_aoi_hv_minor}.${_aoi_hv_rev}")

# 執行時的 DLL 與版本（只用來提醒與給測試的 PATH；建置不需要）。
set(HALCON_RUNTIME_DIR "")
set(HALCON_RUNTIME_VERSION "")
if(DEFINED ENV{HALCONROOT})
  file(TO_CMAKE_PATH "$ENV{HALCONROOT}" _aoi_hroot)
  if(EXISTS "${_aoi_hroot}/bin/x64-win64/halconcpp.dll")
    set(HALCON_RUNTIME_DIR "${_aoi_hroot}/bin/x64-win64")
  endif()
  if(EXISTS "${_aoi_hroot}/version.txt")
    file(STRINGS "${_aoi_hroot}/version.txt" HALCON_RUNTIME_VERSION LIMIT_COUNT 1)
    string(STRIP "${HALCON_RUNTIME_VERSION}" HALCON_RUNTIME_VERSION)
  endif()
endif()
if(NOT HALCON_RUNTIME_DIR)
  message(WARNING "HALCON runtime (halconcpp.dll) not found under %HALCONROOT%: the program builds, "
                  "but the HALCON back end is unavailable at run time and its tests are skipped.")
elseif(NOT HALCON_RUNTIME_VERSION MATCHES "^${_aoi_hv_major}\\.${_aoi_hv_minor}\\.")
  message(WARNING "HALCON headers are ${HALCON_SDK_VERSION}, the runtime is ${HALCON_RUNTIME_VERSION}: "
                  "the major and minor versions must match.")
endif()

add_library(Halcon::HalconCpp UNKNOWN IMPORTED)
set_target_properties(Halcon::HalconCpp PROPERTIES
  IMPORTED_LOCATION "${HALCON_CPP_LIBRARY}"
  # 標頭用 <halconcpp/HalconCpp.h>；HALCON 自己的標頭以 include/ 與 include/halconcpp/ 互相引用，兩個都要。
  INTERFACE_INCLUDE_DIRECTORIES "${HALCON_INCLUDE_DIR};${HALCON_INCLUDE_DIR}/halconcpp"
  # HalconCpp.h 會帶進 <windows.h>：沒有 NOMINMAX，min／max 巨集會打壞 std::min、std::max。
  INTERFACE_COMPILE_DEFINITIONS "NOMINMAX"
  INTERFACE_LINK_OPTIONS "/DELAYLOAD:halconcpp.dll"
  INTERFACE_LINK_LIBRARIES "delayimp")

message(STATUS "HALCON SDK ${HALCON_SDK_VERSION} (${HALCON_INCLUDE_DIR}); runtime ${HALCON_RUNTIME_VERSION} (${HALCON_RUNTIME_DIR})")
