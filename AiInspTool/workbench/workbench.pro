# Native Qt Creator run target, with an offline Python payload build prerequisite.
TEMPLATE = app
TARGET = VisionCadWorkbench
include($$AOI_QMAKE/aoi_options.pri)
AOI_QT_EXTRA = core widgets
include($$AOI_QMAKE/aoi_target.pri)
CONFIG += windows
CONFIG -= console
isEmpty(AOI_PCB_PYTHON): AOI_PCB_PYTHON = $$(AOI_PCB_PYTHON)
isEmpty(AOI_PCB_PYTHON): AOI_PCB_PYTHON = $$AOI_ROOT/build/pcb-dl-env/Scripts/python.exe
!exists($$AOI_PCB_PYTHON): error("Vision CAD needs Python 3.12 and its dependencies. Run scripts/dev/setup-pcb-dl.ps1 or set AOI_PCB_PYTHON=C:/path/to/python.exe. No packages are downloaded during build.")
SOURCES += $$AOI_ROOT/src/app/WorkbenchLauncher.cpp
OTHER_FILES += $$files($$AOI_ROOT/tools/pcb_dl/*.py) $$AOI_ROOT/tools/pcb_dl/requirements-lock.txt
workbench_payload.target = workbench_payload
workbench_payload.depends = FORCE
workbench_payload.commands = $$shell_quote($$system_path($$AOI_PCB_PYTHON)) $$shell_quote($$system_path($$AOI_ROOT/tools/pcb_dl/build_workbench.py)) --source $$shell_quote($$system_path($$AOI_ROOT)) --output $$shell_quote($$system_path($$AOI_BIN_DIR/vision-workbench))
QMAKE_EXTRA_TARGETS += workbench_payload
PRE_TARGETDEPS += workbench_payload
