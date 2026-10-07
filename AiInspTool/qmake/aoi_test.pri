# Qt Test 執行檔（ADR-003：一個測試類別一個執行檔）。用法（各資料夾的 *_tests.pri 設好 AOI_LINK 再 include 這個檔）：
#   TARGET = tst_domain
#   AOI_LINK_EXTRA = presentation          這個測試另外要連結的模組（間接依賴自動展開）
#   AOI_LINK_IF_OPENCV = backend_opencv    只在 AOI_WITH_OPENCV=ON 時連結的模組
#   include(core_tests.pri)
#   SOURCES += $$AOI_ROOT/tests/core/tst_domain.cpp
# 哪些測試要建置（例如只在 OpenCV 開時才有的）列在 tests/tests.pro；執行由 scripts/dev/test.ps1 或 Qt Creator 的測試面板。

TEMPLATE = app
include($$PWD/aoi_options.pri)
AOI_LINK += $$AOI_LINK_EXTRA
equals(AOI_WITH_OPENCV, ON): AOI_LINK += $$AOI_LINK_IF_OPENCV
AOI_CLOSURE = $$aoiClosure($$AOI_LINK)
AOI_QT_EXTRA += testlib
include($$PWD/aoi_target.pri)

CONFIG += console
CONFIG -= windows

# tests/core 的 QtTestPrinters.h 等共用標頭、tests/support 的合成板子產生器。
INCLUDEPATH += $$AOI_ROOT/tests/core $$AOI_ROOT/tests/support

# 在 main 之前設好 offscreen 平台與字型資料夾（已經有設定時不覆寫）：Qt Creator 直接執行測試時，
# 也和 scripts/dev/test.ps1 得到一樣的環境（以前由 CTest 的 ENVIRONMENT 設定）。
SOURCES += $$AOI_ROOT/tests/support/test_environment.cpp

# 效能量測寫結果（perf/results/）、讀預算（perf/budgets.json）與文件裡的資料時用到 repo 的位置。
DEFINES += AOI_SOURCE_DIR=\\\"$$AOI_ROOT\\\" AOI_BUILD_TYPE=\\\"$$AOI_BUILD_TYPE\\\"
# 字型資料夾（tst_cli 給它啟動的 AiInspTool.exe）；用 / 分隔，放進 C 字串不必跳脫。
AOI_FONT_DIR = $$clean_path($$(WINDIR)/Fonts)
# Debug 的數字沒有意義（沒最佳化），所以只有 Release、Profile 才把效能預算當成失敗條件。
!CONFIG(debug, debug|release): DEFINES += AOI_PERF_ENFORCE=1
