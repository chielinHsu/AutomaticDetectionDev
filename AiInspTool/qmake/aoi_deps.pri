# 模組的依賴表：docs/architecture.md「分層與依賴規則」的機器可讀版（CMake 時代由 target_link_libraries 表達）。
# 改分層要三邊一起改：這裡、docs/architecture.md 的依賴表、tools/check-deps.mjs 的 RULES。
# 違規的 #include 由 tools/check-deps.mjs 擋下；這張表決定「連得到什麼」：沒列的模組，它的程式庫連結不到，
# 沒列 Qt 的模組編譯時看不到 Qt 的標頭（CONFIG -= qt），誤用就編不過。
#
#   AOI_PRO_<模組>   .pro 的位置（相對於 AiInspTool/）；程式庫叫 aoi_<模組>.lib
#   AOI_DEPS_<模組>  它直接依賴的模組（間接的由 aoiClosure 展開）
#   AOI_QT_<模組>    它自己用到的 Qt 模組（沒寫 ＝ 不用 Qt）
#   AOI_EXT_<模組>   它自己用到的外部函式庫（opencv、halcon；設定在 aoi_opencv.pri、aoi_halcon.pri）
# 用到某個模組，就繼承它（與它所有依賴）的 Qt 模組與外部函式庫：標頭互相 include，所以編譯與連結都要。

# ---- domain：只用 C++ 標準庫 ----
AOI_PRO_domain = src/domain/domain.pro
AOI_DEPS_domain =

# ---- application：use case 與 port（只依賴 domain，不用 Qt、OpenCV、HALCON）----
AOI_PRO_application = src/application/application.pro
AOI_DEPS_application = domain

# ---- infrastructure：每個 adapter 一個程式庫，彼此不依賴 ----
AOI_PRO_backend_fake = src/infrastructure/backend_fake/backend_fake.pro
AOI_DEPS_backend_fake = application

AOI_PRO_image_qt = src/infrastructure/image_qt/image_qt.pro
AOI_DEPS_image_qt = application
AOI_QT_image_qt = core gui

AOI_PRO_storage = src/infrastructure/storage/storage.pro
AOI_DEPS_storage = application
AOI_QT_storage = core

AOI_PRO_perf = src/infrastructure/perf/perf.pro
AOI_DEPS_perf = application

AOI_PRO_threading = src/infrastructure/threading/threading.pro
AOI_DEPS_threading = application

AOI_PRO_threading_qt = src/infrastructure/threading/threading_qt.pro
AOI_DEPS_threading_qt = application
AOI_QT_threading_qt = core

AOI_PRO_backend_opencv = src/infrastructure/backend_opencv/backend_opencv.pro
AOI_DEPS_backend_opencv = application
AOI_EXT_backend_opencv = opencv

AOI_PRO_backend_halcon = src/infrastructure/backend_halcon/backend_halcon.pro
AOI_DEPS_backend_halcon = application
AOI_EXT_backend_halcon = halcon

# ---- presentation：Qt Widgets；依賴 application 與 domain，不依賴 infrastructure ----
AOI_PRO_presentation = src/presentation/presentation.pro
AOI_DEPS_presentation = domain application
AOI_QT_presentation = core gui widgets

# 合成板子與框（測試、效能量測與 --demo 用；不是產品功能）
AOI_PRO_presentation_dev = src/presentation/dev/presentation_dev.pro
AOI_DEPS_presentation_dev = presentation

# ---- app：組裝根（獨立成程式庫，讓測試能驗證組裝的結果）----
AOI_PRO_app_core = src/app/app_core.pro
AOI_DEPS_app_core = application backend_fake image_qt perf threading threading_qt
equals(AOI_WITH_OPENCV, ON): AOI_DEPS_app_core += backend_opencv
equals(AOI_WITH_HALCON, ON): AOI_DEPS_app_core += backend_halcon

# ---- 測試共用：合成板子產生器（標準庫）與存成 PNG＋JSON（Qt）----
AOI_PRO_test_support = tests/support/test_support.pro
AOI_DEPS_test_support = domain image_qt
AOI_QT_test_support = core gui

# 產品的模組（src/），依建置順序；AiInspTool.exe 另外列在 src/src.pro。
AOI_MODULES = domain application backend_fake image_qt storage perf threading threading_qt
equals(AOI_WITH_OPENCV, ON): AOI_MODULES += backend_opencv
equals(AOI_WITH_HALCON, ON): AOI_MODULES += backend_halcon
AOI_MODULES += presentation presentation_dev app_core

# 模組加上它直接與間接依賴的所有模組（MSVC 的連結器會重複搜尋程式庫，順序不影響結果）。依賴最深 4 層，展開 8 輪一定夠。
defineReplace(aoiClosure) {
    modules = $$1
    for(round, 1..8) {
        more =
        for(module, modules): more += $$eval(AOI_DEPS_$${module})
        modules = $$unique($$list($$modules $$more))
    }
    return($$modules)
}
