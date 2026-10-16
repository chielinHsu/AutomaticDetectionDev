# presentation 層：Qt Widgets 的畫布、工具、主題、i18n 與主視窗。
# 可以依賴 domain 與 application（S04 起 Presenter 接上 use case）；不可以依賴 infrastructure、OpenCV、HALCON。
# tokens.json 與 copy.json 的資源（aoi_ux.qrc）由用到 presentation 的程式內嵌（aoi_target.pri），不在這個靜態程式庫裡。

AOI_MODULE = presentation
include($$AOI_QMAKE/aoi_lib.pri)

DIR = $$AOI_ROOT/src/presentation
SOURCES += \
    $$DIR/canvas/BoxFrames.cpp \
    $$DIR/canvas/ImageCanvas.cpp \
    $$DIR/canvas/ImageItem.cpp \
    $$DIR/canvas/OverlayStyle.cpp \
    $$DIR/canvas/RoiGraphicsItem.cpp \
    $$DIR/canvas/RoiLayerItem.cpp \
    $$DIR/compare/CompareOverlayItem.cpp \
    $$DIR/compare/ComparePresenter.cpp \
    $$DIR/compare/CompareWindow.cpp \
    $$DIR/guidance/GuidancePanel.cpp \
    $$DIR/i18n/Translations.cpp \
    $$DIR/library/SamplePanel.cpp \
    $$DIR/objects/ObjectsModel.cpp \
    $$DIR/objects/ObjectsPanel.cpp \
    $$DIR/params/ParamsPanel.cpp \
    $$DIR/presenters/AppPresenters.cpp \
    $$DIR/presenters/AutoRoiPresenter.cpp \
    $$DIR/presenters/BackendPresenter.cpp \
    $$DIR/presenters/MessageTexts.cpp \
    $$DIR/presenters/ParamsPresenter.cpp \
    $$DIR/presenters/SampleListBuilder.cpp \
    $$DIR/presenters/SceneBuilder.cpp \
    $$DIR/shell/EmptyCard.cpp \
    $$DIR/shell/MainWindow.cpp \
    $$DIR/shell/MessageBar.cpp \
    $$DIR/theme/ThemeManager.cpp \
    $$DIR/theme/ThemeTokens.cpp \
    $$DIR/tools/ToolRegistry.cpp \
    $$DIR/tools/Tools.cpp

HEADERS += \
    $$DIR/canvas/BoxFrames.h \
    $$DIR/canvas/ComponentSnapshot.h \
    $$DIR/canvas/ICanvasController.h \
    $$DIR/canvas/ICanvasView.h \
    $$DIR/canvas/ImageCanvas.h \
    $$DIR/canvas/ImageItem.h \
    $$DIR/canvas/ImageViewMetaType.h \
    $$DIR/canvas/OverlayStyle.h \
    $$DIR/canvas/OverlayTypes.h \
    $$DIR/canvas/RoiGraphicsItem.h \
    $$DIR/canvas/RoiLayerItem.h \
    $$DIR/compare/CompareInterfaces.h \
    $$DIR/compare/CompareOverlayItem.h \
    $$DIR/compare/ComparePresenter.h \
    $$DIR/compare/CompareWindow.h \
    $$DIR/guidance/GuideDemo.h \
    $$DIR/guidance/GuidancePanel.h \
    $$DIR/i18n/Translations.h \
    $$DIR/library/SamplePanel.h \
    $$DIR/objects/ObjectRows.h \
    $$DIR/objects/ObjectsModel.h \
    $$DIR/objects/ObjectsPanel.h \
    $$DIR/params/ParamsPanel.h \
    $$DIR/presenters/AppPresenters.h \
    $$DIR/presenters/AutoRoiPresenter.h \
    $$DIR/presenters/BackendPresenter.h \
    $$DIR/presenters/ControllerInterfaces.h \
    $$DIR/presenters/MessageTexts.h \
    $$DIR/presenters/ParamsPresenter.h \
    $$DIR/presenters/SampleListBuilder.h \
    $$DIR/presenters/SceneBuilder.h \
    $$DIR/presenters/UiPoster.h \
    $$DIR/presenters/ViewInterfaces.h \
    $$DIR/shell/EmptyCard.h \
    $$DIR/shell/MainWindow.h \
    $$DIR/shell/MessageBar.h \
    $$DIR/theme/ThemeManager.h \
    $$DIR/theme/ThemeTokens.h \
    $$DIR/tools/ITool.h \
    $$DIR/tools/ToolId.h \
    $$DIR/tools/ToolRegistry.h \
    $$DIR/tools/Tools.h

# Qt Designer 的表單（session B，ADR-022）：和類別放在一起。uic 的輸出在建置資料夾（.uic/aoi_presentation），qmake 會加進 include 路徑。
# 表單裡的文字只是佔位（notr，不翻譯）：執行時由各類別的 retranslate 從 copy.json 設定。提升的佔位元件：ImageCanvas、MessageBar、ObjectsPanel、ParamsPanel。
# S09：A/B 比較的視窗 compare/CompareWindow.ui（畫布是提升的 ImageCanvas）。
# 2026-10-14：樣本庫 dock 的面板 library/SamplePanel.ui（主視窗裡是提升元件）；ObjectsPanel.ui 多了選取資訊的表單。
# 2026-10-15：操作步驟 dock 的面板 guidance/GuidancePanel.ui（主視窗裡是提升元件；ADR-027）。
FORMS += \
    $$DIR/compare/CompareWindow.ui \
    $$DIR/guidance/GuidancePanel.ui \
    $$DIR/library/SamplePanel.ui \
    $$DIR/objects/ObjectsPanel.ui \
    $$DIR/params/ParamsPanel.ui \
    $$DIR/shell/EmptyCard.ui \
    $$DIR/shell/MainWindow.ui \
    $$DIR/shell/MessageBar.ui
