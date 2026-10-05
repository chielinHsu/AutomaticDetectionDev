# AiInspTool：用 qmake 建置 repo 裡的 AOI 程式（S02 的 Qt Widgets 主視窗與影像畫布）。
#
# 程式碼只有一份，在 ../src（五層架構，見 ../docs/architecture.md）。這個檔只是讓 Qt Creator 可以用 qmake
# 開啟、建置與執行；它會自動納入 ../src 底下的檔案，之後在 ../src/presentation 新增檔案不必改這裡。
# 正式的建置與測試走 CMake（../CMakeLists.txt、../scripts/dev/test.ps1），單元測試也只在 CMake 那邊。
#
# 注意：S03 之後 ../src 會多出 application、infrastructure（OpenCV、HALCON）。到時候這個檔要跟著調整，
#       不要只依賴它確認「全部都能建置」。
#
# 精靈原本產生的 main.cpp、mainwindow.h、mainwindow.cpp、mainwindow.ui 現在沒有被使用（入口是 ../src/app/main.cpp）。

QT += core gui widgets

TEMPLATE = app
TARGET = AiInspTool
CONFIG += c++20

INCLUDEPATH += $$PWD/../src

SOURCES += $$PWD/../src/app/main.cpp
SOURCES += $$files($$PWD/../src/presentation/*.cpp, true)

# Q_OBJECT 的標頭一定要列進 HEADERS，qmake 才會對它們執行 moc。
HEADERS += $$files($$PWD/../src/domain/*.h, true)
HEADERS += $$files($$PWD/../src/presentation/*.h, true)

# tokens.json 與 copy.json 是畫面顏色與文案的唯一來源，內嵌進執行檔。
RESOURCES += $$PWD/aoi_ux.qrc

# 開啟 --demo（合成板子與框，在 src/presentation/dev）。
DEFINES += AOI_DEMO_SCENE=1

msvc {
    # 原始碼是 UTF-8（含中文註解）；__cplusplus 要回報真正的標準版本。
    QMAKE_CXXFLAGS += /utf-8 /Zc:__cplusplus /permissive-
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
