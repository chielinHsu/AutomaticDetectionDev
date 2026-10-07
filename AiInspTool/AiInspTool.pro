# AiInspTool：AOI 教學工具的唯一建置（qmake）。在 Qt Creator 開這個檔，就能建置、執行、除錯程式與所有 Qt Test。
#
# 結構（ADR-018）：所有 .pro／.pri 都在 AiInspTool/ 底下，對照 repo 的 src/、tests/；原始碼只有一份，在 ../src、../tests。
#   .qmake.conf        AOI_ROOT（repo 根目錄）、AOI_QMAKE、AOI_BUILD_ROOT（建置資料夾）
#   qmake/             共用的 .pri：選項（aoi_options）、依賴表（aoi_deps）、OpenCV、HALCON、旗標（aoi_target）、程式庫與測試的範本
#   src/src.pro        五層的程式庫與 AiInspTool.exe
#   tests/tests.pro    所有 Qt Test（AOI_BUILD_TESTS=ON）
# 輸出：<建置>/bin（AiInspTool.exe、測試、OpenCV 的 DLL）、<建置>/lib（靜態程式庫）。
#
# 選項（預設全開；關掉的寫法：qmake AiInspTool.pro AOI_WITH_HALCON=OFF，Qt Creator 填在 qmake 步驟的「其他引數」）：
#   AOI_WITH_OPENCV、AOI_WITH_HALCON、AOI_BUILD_TESTS、AOI_DEMO_SCENE；位置：AOI_OPENCV_DIR、AOI_HALCON_SDK_DIR（見 qmake/aoi_options.pri）。
# 命令列建置與測試：scripts/dev/test.ps1（Release／Debug、可以分別關掉 OpenCV、HALCON）。

TEMPLATE = subdirs
include($$AOI_QMAKE/aoi_options.pri)

SUBDIRS += src
src.file = src/src.pro

equals(AOI_BUILD_TESTS, ON) {
    SUBDIRS += tests
    tests.file = tests/tests.pro
    tests.depends = src
}

# 這次建置有哪些功能，寫在 qmake 的輸出（Qt Creator 的〔編譯輸出〕）：功能少了要看得出來。
equals(AOI_WITH_OPENCV, ON): aoi_cv = "ON ($$AOI_OPENCV_LIB)"
else: aoi_cv = "OFF (no TIFF, no OpenCV back end)"
equals(AOI_WITH_HALCON, ON): aoi_hl = "ON (SDK $$AOI_HALCON_SDK_VERSION, runtime $$AOI_HALCON_RUNTIME_VERSION)"
else: aoi_hl = "OFF (no HALCON back end)"
message("AiInspTool $$AOI_BUILD_TYPE: OpenCV $$aoi_cv; HALCON $$aoi_hl; tests $$AOI_BUILD_TESTS; output $$AOI_BIN_DIR")

equals(AOI_WITH_HALCON, ON) {
    isEmpty(AOI_HALCON_RUNTIME_DIR) {
        warning("HALCON runtime (halconcpp.dll) not found under %HALCONROOT%: the program builds, \
but the HALCON back end is unavailable at run time and its tests are skipped.")
    } else: !contains(AOI_HALCON_RUNTIME_VERSION, "^$$re_escape($$section(AOI_HALCON_SDK_VERSION, ., 0, 1))\\..*") {
        warning("HALCON headers are $$AOI_HALCON_SDK_VERSION, the runtime is $$AOI_HALCON_RUNTIME_VERSION: \
the major and minor versions must match.")
    }
}
