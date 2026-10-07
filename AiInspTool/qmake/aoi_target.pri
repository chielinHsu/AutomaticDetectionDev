# 每個程式庫、程式與測試共用的設定（由 aoi_lib.pri、aoi_app.pri、aoi_test.pri include）。
# include 之前要先 include aoi_options.pri，並設好：
#   TARGET        目標名稱（中間檔的資料夾也用它）
#   AOI_CLOSURE   這個目標用到的模組，含間接依賴（程式庫含它自己）；由 aoiClosure() 展開
#   AOI_QT_EXTRA  依賴表以外的 Qt 模組（測試的 testlib）

CONFIG += c++20

# 中間檔各目標分開放：同一個資料夾有好幾個 .pro 時（例如 tests/core/），共用的 moc_predefs.h 在 jom 並行建置時會互相踩到。
OBJECTS_DIR = $$OUT_PWD/.obj/$$TARGET
MOC_DIR = $$OUT_PWD/.moc/$$TARGET
RCC_DIR = $$OUT_PWD/.rcc/$$TARGET
UI_DIR = $$OUT_PWD/.uic/$$TARGET

# 所有 #include 都從 src/ 算起（例如 "application/jobs/JobRunner.h"）。
INCLUDEPATH += $$AOI_ROOT/src

# 自家程式碼：/W4 /WX，第三方標頭（角括號 include：Qt、OpenCV、HALCON）不受影響。
# qmake 的 msvc mkspec 已經給 -permissive-、-Zc:__cplusplus、-utf-8；這裡換掉它的 -W3 與個別警告的設定。
msvc {
    QMAKE_CXXFLAGS_WARN_ON = -W4 -WX
    QMAKE_CXXFLAGS += -external:W0 -external:anglebrackets
}

# ---- 用到的模組：Qt 模組與外部函式庫（含間接依賴的）----
QT = $$AOI_QT_EXTRA
for(module, AOI_CLOSURE): QT += $$eval(AOI_QT_$${module})
QT = $$unique(QT)
isEmpty(QT): CONFIG -= qt
AOI_EXTERNALS =
for(module, AOI_CLOSURE): AOI_EXTERNALS += $$eval(AOI_EXT_$${module})

contains(AOI_EXTERNALS, opencv) {
    # INCLUDEPATH 給 Qt Creator 的程式碼模型；-external:I 讓編譯器把它當第三方標頭（不受 /W4 /WX 影響）。
    INCLUDEPATH += $$AOI_OPENCV_INCLUDE
    QMAKE_CXXFLAGS += -external:I $$shell_quote($$system_path($$AOI_OPENCV_INCLUDE))
}
contains(AOI_EXTERNALS, halcon) {
    # 標頭用 <halconcpp/HalconCpp.h>；HALCON 自己的標頭以 include/ 與 include/halconcpp/ 互相引用，兩個都要。
    INCLUDEPATH += $$AOI_HALCON_INCLUDE $$AOI_HALCON_INCLUDE/halconcpp
    QMAKE_CXXFLAGS += -external:I $$shell_quote($$system_path($$AOI_HALCON_INCLUDE)) \
                      -external:I $$shell_quote($$system_path($$AOI_HALCON_INCLUDE/halconcpp))
    # HalconCpp.h 會帶進 <windows.h>：沒有 NOMINMAX，min／max 巨集會打壞 std::min、std::max。
    DEFINES += NOMINMAX
}

# 組裝根與測試依這兩個定義決定登記哪些 back end、跑哪些測試。
contains(AOI_CLOSURE, backend_opencv): DEFINES += AOI_WITH_OPENCV=1
contains(AOI_CLOSURE, backend_halcon): DEFINES += AOI_WITH_HALCON=1

# 程式（AiInspTool.exe 與測試）要連結所有用到的程式庫；程式庫本身不連結（靜態程式庫）。
equals(TEMPLATE, app) {
    DESTDIR = $$AOI_BIN_DIR
    for(module, AOI_CLOSURE) {
        LIBS += $$AOI_LIB_DIR/aoi_$${module}.lib
        PRE_TARGETDEPS += $$AOI_LIB_DIR/aoi_$${module}.lib
    }
    contains(AOI_EXTERNALS, opencv): LIBS += $$AOI_OPENCV_LIB
    contains(AOI_EXTERNALS, halcon) {
        # delayimp：/DELAYLOAD 的輔助程式；version：GetFileVersionInfo（HalconRuntime.cpp 檢查 DLL 的版本）。
        LIBS += $$AOI_HALCON_LIB delayimp.lib version.lib
        # LNK4199（「沒有從 halconcpp.dll 匯入任何東西，/DELAYLOAD 被忽略」）：連了組裝根、但沒用到 HALCON 的測試一定會有，無害；
        # 不關掉的話，Qt Creator 的〔問題〕面板每次建置都有約 30 個這種警告，真的警告反而看不到。
        QMAKE_LFLAGS += /DELAYLOAD:halconcpp.dll /IGNORE:4199
    }
    # tokens.json 與 copy.json 是畫面顏色與文案的唯一來源，內嵌進用到 presentation 的程式。
    # 放在程式這邊，不放在 presentation 的靜態程式庫：程式庫裡的資源沒有被參考，會被連結器丟掉。
    contains(AOI_CLOSURE, presentation): RESOURCES += $$clean_path($$AOI_QMAKE/../src/presentation/aoi_ux.qrc)
} else {
    DESTDIR = $$AOI_LIB_DIR
}
