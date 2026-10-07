# 說明頁（docs/steps/S05-lattice/explainer.html）的資料由 C++ 算：檢查頁面裡的資料仍是目前的程式算出來的。
# 更新頁面：設環境變數 AOI_EXPORT_EXPLAINER=1 再執行 tst_lattice_explainer。

TARGET = tst_lattice_explainer
include(domain_tests.pri)
SOURCES += $$AOI_ROOT/tests/domain/tst_lattice_explainer.cpp
