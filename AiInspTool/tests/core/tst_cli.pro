# 命令列：從外面用真的 AiInspTool.exe 行程驗證 --backend（沒編進程式的 back end 要明確失敗、結束碼 2）。
# AiInspTool.exe 在 src/src.pro，tests/ 整個在 src/ 之後建置（AiInspTool.pro 的 tests.depends）。
# S07：OpenCV 編進程式時，--backend=opencv 要能啟動；S08：HALCON 編進程式時，有授權要能啟動、沒有授權或沒有 DLL 時要明確失敗。

TARGET = tst_cli
include(core_tests.pri)
SOURCES += $$AOI_ROOT/tests/core/tst_cli.cpp
DEFINES += AOI_EXE_PATH=\\\"$$AOI_BIN_DIR/AiInspTool.exe\\\" AOI_FONT_DIR=\\\"$$AOI_FONT_DIR\\\"
