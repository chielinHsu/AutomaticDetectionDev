# 產品的靜態程式庫。用法（.pro 的開頭）：
#   AOI_MODULE = storage
#   include($$AOI_QMAKE/aoi_lib.pri)
#   SOURCES += ...
# 依賴、Qt 模組與外部函式庫都查 aoi_deps.pri 的表，.pro 只列檔案。

isEmpty(AOI_MODULE): error("$$_PRO_FILE_: set AOI_MODULE before including aoi_lib.pri.")
TEMPLATE = lib
CONFIG += staticlib
TARGET = aoi_$$AOI_MODULE
include($$PWD/aoi_options.pri)
AOI_CLOSURE = $$aoiClosure($$AOI_MODULE)
include($$PWD/aoi_target.pri)
