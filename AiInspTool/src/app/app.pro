# AiInspTool.exe：組裝（主題、語言、視窗、核心、AppPresenters、命令列 --backend）。入口是 src/app/main.cpp。
#
# 子系統是 GUI（/SUBSYSTEM:WINDOWS）：Qt Creator 對主控台程式預設「在終端機執行」，會多開一個視窗（ADR-018）。
# 啟動失敗的原因與 back end 的提醒仍寫到 stderr：經管線接走時（QProcess、Qt Creator 的〔應用程式輸出〕、tst_cli）拿得到；
# 直接在主控台執行時看不到（GUI 程式沒有主控台），要看就重新導向：AiInspTool.exe --backend halcon 2> err.txt

TEMPLATE = app
TARGET = AiInspTool
include($$AOI_QMAKE/aoi_options.pri)
AOI_LINK = presentation app_core
equals(AOI_DEMO_SCENE, ON): AOI_LINK += presentation_dev
AOI_CLOSURE = $$aoiClosure($$AOI_LINK)
include($$AOI_QMAKE/aoi_target.pri)

CONFIG += windows
CONFIG -= console

# 開啟 --demo（合成板子與框，在 src/presentation/dev）。
equals(AOI_DEMO_SCENE, ON): DEFINES += AOI_DEMO_SCENE=1

SOURCES += $$AOI_ROOT/src/app/main.cpp
