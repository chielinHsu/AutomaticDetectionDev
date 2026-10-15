# OpenCV 的 adapter。S06：影像轉換（cv::Mat ⇄ ImageView）與 IImageSource；S07：範本建立器、搜尋器、樣本庫比對器。
# ADR-026：比對區（遮罩的 NCC）與邊緣模式（OpenCvFeatures）。
# 只在 AOI_WITH_OPENCV=ON 時建置（src/src.pro）。OpenCV 只能出現在這個資料夾（check-deps）；位置見 qmake/aoi_opencv.pri。

AOI_MODULE = backend_opencv
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/infrastructure/backend_opencv
SOURCES += \
    $$DIR/OpenCvBackend.cpp \
    $$DIR/OpenCvCorrelation.cpp \
    $$DIR/OpenCvFeatures.cpp \
    $$DIR/OpenCvImageSource.cpp \
    $$DIR/OpenCvImages.cpp \
    $$DIR/OpenCvLibraryMatcher.cpp \
    $$DIR/OpenCvPatternSearcher.cpp \
    $$DIR/OpenCvPyramid.cpp \
    $$DIR/OpenCvTemplate.cpp \
    $$DIR/OpenCvTemplateBuilder.cpp

HEADERS += \
    $$DIR/OpenCvBackend.h \
    $$DIR/OpenCvCorrelation.h \
    $$DIR/OpenCvFeatures.h \
    $$DIR/OpenCvImageSource.h \
    $$DIR/OpenCvImages.h \
    $$DIR/OpenCvLibraryMatcher.h \
    $$DIR/OpenCvPatternSearcher.h \
    $$DIR/OpenCvPyramid.h \
    $$DIR/OpenCvSearchOptions.h \
    $$DIR/OpenCvTemplate.h \
    $$DIR/OpenCvTemplateBuilder.h

# OpenCV 的 DLL 複製到 <建置>/bin（AiInspTool.exe 與測試的旁邊；Debug 用 d 版）。只複製 OpenCV 的：
# Qt 的 DLL 放過去，Qt 會到執行檔旁邊找外掛而找不到。放在這裡（不放在每個程式）：所有用到 OpenCV 的程式都在它之後建置，
# 而且只有一個 Makefile 寫這個檔，jom 並行建置時不會互相踩到。
COPIES += aoi_opencv_dll
aoi_opencv_dll.files = $$AOI_OPENCV_DLL
aoi_opencv_dll.path = $$AOI_BIN_DIR
