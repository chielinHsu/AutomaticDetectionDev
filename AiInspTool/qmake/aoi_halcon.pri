# HALCON 的 C++ 介面（AOI_WITH_HALCON=ON 時由 aoi_options.pri include；S08，ADR-017）。
#
# 編譯要兩樣東西，執行要另外兩樣，來源不同：
#   編譯：標頭（include/、include/halconcpp/）與匯入程式庫（lib/x64-win64/halconcpp.lib）
#         → 預設用 repo 裡的 third_party/halcon（從使用者的 CV_Algorithm 專案複製，見那裡的 README.md）；
#           AOI_HALCON_SDK_DIR 可以指到別的 SDK（例如裝了開發元件的 %HALCONROOT%）。
#   執行：halconcpp.dll、halcon.dll（%HALCONROOT%\bin\x64-win64，安裝程式會放進 PATH）與授權。
#
# halconcpp.dll 用延遲載入（/DELAYLOAD）：沒有安裝 HALCON 的電腦，程式照樣啟動（只是 halcon 不可用），
# 不會在啟動時就因為找不到 DLL 而被 Windows 擋下。第一次呼叫 HALCON 之前，adapter 先確認 DLL 載得起來（HalconRuntime.h）。
# 結果：AOI_HALCON_INCLUDE、AOI_HALCON_LIB、AOI_HALCON_SDK_VERSION、AOI_HALCON_RUNTIME_DIR（找不到時是空的）。

isEmpty(AOI_HALCON_SDK_DIR): AOI_HALCON_SDK_DIR = $$AOI_ROOT/third_party/halcon
AOI_HALCON_SDK_DIR = $$clean_path($$AOI_HALCON_SDK_DIR)
AOI_HALCONROOT = $$clean_path($$(HALCONROOT))

AOI_HALCON_INCLUDE =
AOI_HALCON_LIB =
for(dir, $$list($$AOI_HALCON_SDK_DIR $$AOI_HALCONROOT)) {
    isEmpty(AOI_HALCON_INCLUDE):exists($$dir/include/halconcpp/HalconCpp.h):exists($$dir/lib/x64-win64/halconcpp.lib) {
        AOI_HALCON_INCLUDE = $$dir/include
        AOI_HALCON_LIB = $$dir/lib/x64-win64/halconcpp.lib
    }
}
isEmpty(AOI_HALCON_INCLUDE) {
    error("AOI_WITH_HALCON=ON, but the HALCON C++ interface was not found \
(looked for include/halconcpp/HalconCpp.h and lib/x64-win64/halconcpp.lib in '$$AOI_HALCON_SDK_DIR' and '$$AOI_HALCONROOT'). \
Set AOI_HALCON_SDK_DIR to a HALCON SDK, or build without HALCON: qmake AiInspTool.pro AOI_WITH_HALCON=OFF.")
}

# 標頭的版本（HVersNum.h 的 HLIB_MAJOR_NUM、HLIB_MINOR_NUM、HLIB_REVISION_NUM）。
# 比對的樣式不寫井字號：qmake 把它當成註解的開頭，引號裡也是。
aoi_hvers = $$cat($$AOI_HALCON_INCLUDE/HVersNum.h, lines)
for(line, aoi_hvers) {
    contains(line, ".*define HLIB_MAJOR_NUM [0-9].*"): aoi_hv_major = $$section(line, " ", -1)
    contains(line, ".*define HLIB_MINOR_NUM [0-9].*"): aoi_hv_minor = $$section(line, " ", -1)
    contains(line, ".*define HLIB_REVISION_NUM [0-9].*"): aoi_hv_rev = $$section(line, " ", -1)
}
AOI_HALCON_SDK_VERSION = $${aoi_hv_major}.$${aoi_hv_minor}.$${aoi_hv_rev}

# 執行時的 DLL 與版本（只用來提醒與給 scripts/dev/test.ps1 的 PATH；建置不需要）。
AOI_HALCON_RUNTIME_DIR =
AOI_HALCON_RUNTIME_VERSION =
!isEmpty(AOI_HALCONROOT) {
    exists($$AOI_HALCONROOT/bin/x64-win64/halconcpp.dll): AOI_HALCON_RUNTIME_DIR = $$AOI_HALCONROOT/bin/x64-win64
    exists($$AOI_HALCONROOT/version.txt): AOI_HALCON_RUNTIME_VERSION = $$first($$list($$cat($$AOI_HALCONROOT/version.txt, lines)))
}
