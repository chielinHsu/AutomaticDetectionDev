@echo off
cd /d "%~dp0"
echo 1. Socket Region CAD
echo 2. Component Deep Learning - OBB
echo 3. Component Deep Learning - Instance Segmentation
choice /c 123 /n /m "Select mode [1-3]: "
if errorlevel 3 goto segment
if errorlevel 2 goto obb
if not exist "build\Socket-RegionCAD\AiInspTool.exe" goto missing
start "" "build\Socket-RegionCAD\AiInspTool.exe" --socket
exit /b
:obb
if not exist "build\pcb-dl-env\Scripts\python.exe" goto missing
"build\pcb-dl-env\Scripts\python.exe" -m tools.pcb_dl.cli gui --task obb
pause
exit /b
:segment
if not exist "build\pcb-dl-env\Scripts\python.exe" goto missing
"build\pcb-dl-env\Scripts\python.exe" -m tools.pcb_dl.cli gui --task segment
pause
exit /b
:missing
echo Build Socket or run scripts\dev\setup-pcb-dl.ps1 first. See tools\pcb_dl\README.md.
pause
exit /b 2
