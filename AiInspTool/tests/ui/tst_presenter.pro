# Presenter 的整合測試：走產品的接線（core → MainWindow → AppPresenters），所以也連組裝根與假 back end。

TARGET = tst_presenter
include(ui_tests.pri)
SOURCES += $$AOI_ROOT/tests/ui/tst_presenter.cpp
