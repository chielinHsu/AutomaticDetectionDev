param(
    [string]$BuildDir = 'build/qmake-socket-mvp',
    [ValidateSet('Release','Debug')][string]$Config = 'Release',
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1" | Out-Null
$socketRoot = (Resolve-Path "$PSScriptRoot\..\..").Path
$socketBuild = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $socketRoot $BuildDir }
New-Item -ItemType Directory -Force -Path $socketBuild | Out-Null
$socketArgs = @((Join-Path $socketRoot 'AiInspTool\AiInspTool.pro'), '-spec', 'win32-msvc', 'AOI_BUILD_TESTS=OFF')
if ($Config -eq 'Debug') { $socketArgs += 'CONFIG+=debug' }
Push-Location -LiteralPath $socketBuild
try {
    & qmake.exe @socketArgs
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    & jom.exe qmake_all
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    & jom.exe /J $Jobs
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    Write-Host "Socket executable: $(Join-Path $socketBuild 'bin\AiInspTool.exe') --socket"
} finally { Pop-Location }
