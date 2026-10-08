# 建置選項與組態。每個 .pro（含 subdirs）都 include 這個檔（程式庫、測試經 aoi_lib.pri、aoi_test.pri）。
#
# 選項用 qmake 的命令列變數設定（命令列的變數會傳到每個子專案）；Qt Creator 填在〔專案 › 建置 › 建置步驟 › qmake › 其他引數〕：
#   qmake AiInspTool.pro AOI_WITH_HALCON=OFF
#   AOI_WITH_OPENCV   ON／OFF（預設 ON）OpenCV back end 與 TIFF、16 bit 影像的讀取
#   AOI_WITH_HALCON   ON／OFF（預設 ON）HALCON back end 與授權檢查
#   AOI_BUILD_TESTS   ON／OFF（預設 ON）所有 Qt Test
#   AOI_DEMO_SCENE    ON／OFF（預設 ON）--demo（合成板子與框）
#   AOI_OPENCV_DIR    OpenCV 的 build 資料夾（預設 D:/opencv/opencv/build，見 aoi_opencv.pri）
#   AOI_HALCON_SDK_DIR  HALCON 的標頭與匯入程式庫（預設 repo 的 third_party/halcon，見 aoi_halcon.pri）
# 打開的選項找不到函式庫時直接報錯，不會悄悄關掉（ADR-018：功能悄悄不見就是這次要解決的問題）。

isEmpty(AOI_ROOT): error("AOI_ROOT is not set: open AiInspTool/AiInspTool.pro (its .qmake.conf defines the paths).")

isEmpty(AOI_WITH_OPENCV): AOI_WITH_OPENCV = ON
isEmpty(AOI_WITH_HALCON): AOI_WITH_HALCON = ON
isEmpty(AOI_BUILD_TESTS): AOI_BUILD_TESTS = ON
isEmpty(AOI_DEMO_SCENE): AOI_DEMO_SCENE = ON
for(option, $$list(AOI_WITH_OPENCV AOI_WITH_HALCON AOI_BUILD_TESTS AOI_DEMO_SCENE)) {
    !equals($$option, ON):!equals($$option, OFF): \
        error("$$option must be ON or OFF, not '$$eval($$option)'.")
}

# 單一組態：建置資料夾裡沒有 debug/、release/ 子資料夾（Qt Creator 的 Debug、Release、Profile 各用自己的建置資料夾）。
# Debug：CONFIG+=debug；Release：預設；Profile：Release ＋ CONFIG+=force_debug_info（Qt Creator 的預設參數）。
CONFIG -= debug_and_release debug_and_release_target
CONFIG(debug, debug|release) {
    AOI_BUILD_TYPE = Debug
} else: force_debug_info {
    AOI_BUILD_TYPE = Profile
} else {
    AOI_BUILD_TYPE = Release
}

# 輸出位置：程式庫 <建置>/lib，執行檔（AiInspTool.exe 與所有測試）<建置>/bin。
AOI_LIB_DIR = $$AOI_BUILD_ROOT/lib
AOI_BIN_DIR = $$AOI_BUILD_ROOT/bin

include($$PWD/aoi_deps.pri)
equals(AOI_WITH_OPENCV, ON): include($$PWD/aoi_opencv.pri)
equals(AOI_WITH_HALCON, ON): include($$PWD/aoi_halcon.pri)
