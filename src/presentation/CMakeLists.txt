# presentation 層：Qt Widgets 的畫布、工具、主題、i18n 與主視窗。
# 可以依賴 domain 與（S03 起）application；不可以依賴 OpenCV、HALCON。
qt_add_library(aoi_presentation STATIC
    canvas/BoxFrames.cpp
    canvas/BoxFrames.h
    canvas/ComponentSnapshot.h
    canvas/ICanvasController.h
    canvas/ICanvasView.h
    canvas/ImageCanvas.cpp
    canvas/ImageCanvas.h
    canvas/ImageItem.cpp
    canvas/ImageItem.h
    canvas/OverlayStyle.cpp
    canvas/OverlayStyle.h
    canvas/OverlayTypes.h
    canvas/RoiGraphicsItem.cpp
    canvas/RoiGraphicsItem.h
    canvas/RoiLayerItem.cpp
    canvas/RoiLayerItem.h
    i18n/Translations.cpp
    i18n/Translations.h
    shell/EmptyCard.cpp
    shell/EmptyCard.h
    shell/ImageLoader.cpp
    shell/ImageLoader.h
    shell/MainWindow.cpp
    shell/MainWindow.h
    shell/MessageBar.cpp
    shell/MessageBar.h
    shell/ShellController.cpp
    shell/ShellController.h
    theme/ThemeManager.cpp
    theme/ThemeManager.h
    theme/ThemeTokens.cpp
    theme/ThemeTokens.h
    tools/ITool.h
    tools/ToolId.h
    tools/ToolRegistry.cpp
    tools/ToolRegistry.h
    tools/Tools.cpp
    tools/Tools.h
)
target_include_directories(aoi_presentation PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(aoi_presentation PUBLIC Qt6::Widgets aoi_domain)
aoi_set_warnings(aoi_presentation)

# tokens.json 與 copy.json 是唯一的來源，內嵌進執行檔，程式裡不再寫一份。
qt_add_resources(aoi_presentation aoi_ux_resources
    PREFIX "/aoi/ux"
    BASE "${PROJECT_SOURCE_DIR}/docs/ux"
    FILES
        "${PROJECT_SOURCE_DIR}/docs/ux/tokens.json"
        "${PROJECT_SOURCE_DIR}/docs/ux/copy.json"
)

# 合成板子與框，供測試與效能量測用，也給 app 的 --demo 選項用。
qt_add_library(aoi_presentation_dev STATIC dev/FakeScene.cpp dev/FakeScene.h)
target_link_libraries(aoi_presentation_dev PUBLIC aoi_presentation)
aoi_set_warnings(aoi_presentation_dev)
