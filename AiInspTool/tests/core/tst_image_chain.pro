# S06：影像來源的組合（ImageSourceChain），application 層、用假的 port。

TARGET = tst_image_chain
include(core_tests.pri)
SOURCES += $$AOI_ROOT/tests/core/tst_image_chain.cpp
