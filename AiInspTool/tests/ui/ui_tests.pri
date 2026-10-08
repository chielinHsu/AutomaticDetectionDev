# tests/ui：畫面的測試（S02、S04）。Presenter 的測試走產品的接線（core → MainWindow → AppPresenters），所以也連組裝根與假 back end。
AOI_LINK = presentation presentation_dev app_core
include($$AOI_QMAKE/aoi_test.pri)
HEADERS += \
    $$AOI_ROOT/tests/ui/MeasurementHygiene.h \
    $$AOI_ROOT/tests/ui/PerfSupport.h \
    $$AOI_ROOT/tests/ui/PresenterRig.h \
    $$AOI_ROOT/tests/ui/TestSupport.h
