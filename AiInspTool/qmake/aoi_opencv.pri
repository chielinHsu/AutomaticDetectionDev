# OpenCV（AOI_WITH_OPENCV=ON 時由 aoi_options.pri include）。
#
# AOI_OPENCV_DIR 是 OpenCV Windows 套件的 build 資料夾（含 include/ 與 x64/vc*/lib/opencv_world*.lib）。
# 預設看 qmake 變數，再看同名的環境變數，最後是這台機器的位置（AI_CONTEXT.md 的環境表）。
# 這台機器的 OpenCV 4.9.0 只有 opencv_world（一個 DLL），vc16（VS 2019）建置、由 VS 2022 的工具鏈連結。
# 結果：AOI_OPENCV_INCLUDE、AOI_OPENCV_LIB（依 Debug／Release 選 d 版）、AOI_OPENCV_DLL（建置時複製到 <建置>/bin）。

isEmpty(AOI_OPENCV_DIR): AOI_OPENCV_DIR = $$(AOI_OPENCV_DIR)
isEmpty(AOI_OPENCV_DIR): AOI_OPENCV_DIR = D:/opencv/opencv/build
AOI_OPENCV_DIR = $$clean_path($$AOI_OPENCV_DIR)

AOI_OPENCV_INCLUDE = $$AOI_OPENCV_DIR/include
AOI_OPENCV_LIBDIR =
for(vc, $$list(vc17 vc16 vc15)) {
    aoi_cv_found = $$files($$AOI_OPENCV_DIR/x64/$$vc/lib/opencv_world*.lib)
    isEmpty(AOI_OPENCV_LIBDIR):!isEmpty(aoi_cv_found): AOI_OPENCV_LIBDIR = $$AOI_OPENCV_DIR/x64/$$vc/lib
}

# opencv_world<版本>.lib 是 Release、opencv_world<版本>d.lib 是 Debug。
AOI_OPENCV_NAME =
aoi_cv_libs = $$files($$AOI_OPENCV_LIBDIR/opencv_world*.lib)
for(lib, aoi_cv_libs) {
    name = $$basename(lib)
    name = $$replace(name, \\.lib$, )
    !contains(name, .*d$): AOI_OPENCV_NAME = $$name
}

!exists($$AOI_OPENCV_INCLUDE/opencv2/core.hpp)|isEmpty(AOI_OPENCV_NAME) {
    error("AOI_WITH_OPENCV=ON, but OpenCV was not found in '$$AOI_OPENCV_DIR' \
(looked for include/opencv2/core.hpp and x64/vc*/lib/opencv_world*.lib). \
Point to the OpenCV build folder: qmake AiInspTool.pro AOI_OPENCV_DIR=D:/opencv/opencv/build \
(or the environment variable AOI_OPENCV_DIR), or build without OpenCV: qmake AiInspTool.pro AOI_WITH_OPENCV=OFF \
(then TIFF images cannot be opened and the OpenCV back end is unavailable).")
}

CONFIG(debug, debug|release): AOI_OPENCV_NAME = $${AOI_OPENCV_NAME}d
AOI_OPENCV_LIB = $$AOI_OPENCV_LIBDIR/$${AOI_OPENCV_NAME}.lib
AOI_OPENCV_DLL = $$clean_path($$AOI_OPENCV_LIBDIR/../bin/$${AOI_OPENCV_NAME}.dll)
!exists($$AOI_OPENCV_LIB): error("AOI_WITH_OPENCV=ON, but '$$AOI_OPENCV_LIB' does not exist (this OpenCV has no $$AOI_BUILD_TYPE library).")
!exists($$AOI_OPENCV_DLL): error("AOI_WITH_OPENCV=ON, but '$$AOI_OPENCV_DLL' does not exist: the program could not start.")
