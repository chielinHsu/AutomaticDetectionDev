# AiInspTool：用 qmake 建置 repo 裡的 AOI 程式（S02 的 Qt Widgets 主視窗與影像畫布）。
#
# 程式碼只有一份，在 ../src（五層架構，見 ../docs/architecture.md）。這個檔只是讓 Qt Creator 可以用 qmake
# 開啟、建置與執行；它會自動納入 ../src 底下的檔案，之後在 ../src/presentation 新增檔案不必改這裡。
# 正式的建置與測試走 CMake（../CMakeLists.txt、../scripts/dev/test.ps1），單元測試也只在 CMake 那邊。
#
# 注意：S03 起 ../src 有 application、infrastructure、組裝根，這個檔已經調整；S04 新增的 application/document、interaction，
#       infrastructure/threading/QtThreadPoolExecutor 與 presentation/presenters、objects、params 都在已納入的資料夾底下（萬用字元遞迴納入），
#       不必改。S05 起 domain 有 .cpp（domain/lattice：晶格估計、NMS），已加入 SOURCES。
#       S06 加了 infrastructure/image_qt（QImage 的影像來源，Qt Gui）與 infrastructure/storage（配方與樣本庫的 JSON，Qt Core），
#       已用萬用字元加入；domain 的 image_buffer、image_copy 與 application/image、document/RecipeSaver 在已納入的資料夾底下。
#       infrastructure/backend_opencv（S06 起有影像來源）要 OpenCV，這個 qmake 專案不建置它：組裝根在沒有定義 AOI_WITH_OPENCV 時
#       只用 Qt 的影像來源（讀不了 TIFF）。S07／S08 有 OpenCV／HALCON 的搜尋器，到時候要跟著調整，不要只依賴它確認「全部都能建置」
#       （正式的建置與測試走 CMake）。
#
# 精靈原本產生的 main.cpp、mainwindow.h、mainwindow.cpp、mainwindow.ui 現在沒有被使用（入口是 ../src/app/main.cpp）。

QT += core gui widgets

TEMPLATE = app
TARGET = AiInspTool
CONFIG += c++20

INCLUDEPATH += $$PWD/../src

# S03：src/ 多了 application、infrastructure（只有不依賴視覺函式庫的 backend_fake、perf、threading；S06 加 image_qt、storage）與組裝根。
# backend_opencv、backend_halcon（S07、S08）需要各自的函式庫，這個 qmake 專案不建置它們；到時候要用的話再加。
SOURCES += $$PWD/../src/app/main.cpp
SOURCES += $$PWD/../src/app/CompositionRoot.cpp
SOURCES += $$files($$PWD/../src/domain/*.cpp, true)
SOURCES += $$files($$PWD/../src/application/*.cpp, true)
SOURCES += $$files($$PWD/../src/infrastructure/backend_fake/*.cpp, true)
SOURCES += $$files($$PWD/../src/infrastructure/image_qt/*.cpp, true)
SOURCES += $$files($$PWD/../src/infrastructure/perf/*.cpp, true)
SOURCES += $$files($$PWD/../src/infrastructure/storage/*.cpp, true)
SOURCES += $$files($$PWD/../src/infrastructure/threading/*.cpp, true)
SOURCES += $$files($$PWD/../src/presentation/*.cpp, true)

# Q_OBJECT 的標頭一定要列進 HEADERS，qmake 才會對它們執行 moc。
HEADERS += $$files($$PWD/../src/domain/*.h, true)
HEADERS += $$files($$PWD/../src/application/*.h, true)
HEADERS += $$files($$PWD/../src/infrastructure/backend_fake/*.h, true)
HEADERS += $$files($$PWD/../src/infrastructure/image_qt/*.h, true)
HEADERS += $$files($$PWD/../src/infrastructure/perf/*.h, true)
HEADERS += $$files($$PWD/../src/infrastructure/storage/*.h, true)
HEADERS += $$files($$PWD/../src/infrastructure/threading/*.h, true)
HEADERS += $$PWD/../src/app/CompositionRoot.h
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
