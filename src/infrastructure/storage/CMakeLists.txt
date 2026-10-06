# 儲存（S06）：配方與樣本庫的 JSON 檔。只用 Qt Core（QJsonDocument、QSaveFile），不用 Qt Gui／Widgets、不用視覺函式庫。
add_library(aoi_storage STATIC JsonCodec.cpp JsonFile.cpp JsonLibraryStore.cpp JsonRecipeStore.cpp LayeredLibraryStore.cpp)
target_link_libraries(aoi_storage PUBLIC aoi_application Qt6::Core)
aoi_set_warnings(aoi_storage)
