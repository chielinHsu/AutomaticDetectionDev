# 產品：五層的程式庫（依賴表在 qmake/aoi_deps.pri）與 AiInspTool.exe。
# 每個模組的 .depends 直接取自依賴表，所以建置順序就是依賴方向：
#   app ──► presentation ──► application ──► domain
#   app ──► infrastructure ──► application ──► domain
# 違規的 #include 由 tools/check-deps.mjs 擋下（scripts/dev/test.ps1 會跑）。

TEMPLATE = subdirs
include($$AOI_QMAKE/aoi_options.pri)

for(module, AOI_MODULES) {
    SUBDIRS += $$module
    $${module}.file = $$relative_path($$clean_path($$AOI_QMAKE/../$$eval(AOI_PRO_$${module})), $$PWD)
    $${module}.depends = $$eval(AOI_DEPS_$${module})
}

SUBDIRS += AiInspTool
AiInspTool.file = app/app.pro
AiInspTool.depends = presentation app_core
equals(AOI_DEMO_SCENE, ON): AiInspTool.depends += presentation_dev
