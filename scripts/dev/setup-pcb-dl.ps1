param([Parameter(Mandatory=$true)][string]$PythonExe)
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
Push-Location $projectRoot
try {
    & $PythonExe -c "import sys; assert sys.version_info[:2] == (3,12), 'Python 3.12 x64 required'; assert sys.maxsize > 2**32"
    if ($LASTEXITCODE -ne 0) { throw 'Python version check failed' }
    if (-not (Test-Path 'build/pcb-dl-env/Scripts/python.exe')) {
        & $PythonExe -m venv build/pcb-dl-env
        if ($LASTEXITCODE -ne 0) { throw 'venv creation failed' }
    }
    & build/pcb-dl-env/Scripts/python.exe -m pip install --disable-pip-version-check -r tools/pcb_dl/requirements-lock.txt
    if ($LASTEXITCODE -ne 0) { throw 'dependency installation failed' }
    & build/pcb-dl-env/Scripts/python.exe -m pip check
    if ($LASTEXITCODE -ne 0) { throw 'dependency check failed' }
    Write-Host 'Ready: StartCadTools.cmd (2=OBB, 3=segmentation)'
} finally { Pop-Location }