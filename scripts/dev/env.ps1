# 開發環境：把 MSVC 2022 (vcvars64)、Qt、CMake、Ninja 放進目前這個 PowerShell session。
#
# 用法（要用 dot-source，環境變數才會留在目前的 session）：
#   . .\scripts\dev\env.ps1
#
# 路徑預設是本機實測的位置（見 AI_CONTEXT.md 的環境表）。換機器時用環境變數覆寫，不要改這個檔：
#   AOI_QT_ROOT     Qt 的 msvc2022_64 資料夾   （預設 D:\Qt\6.8.0\msvc2022_64）
#   AOI_QT_TOOLS    Qt Tools 資料夾            （預設 D:\Qt\Tools）
# VS 2022 用 vswhere 找，不寫死路徑。

$aoiPrevEap = $ErrorActionPreference   # dot-source 會改到呼叫端的設定，結尾還原
$ErrorActionPreference = 'Stop'

$qtRoot = if ($env:AOI_QT_ROOT) { $env:AOI_QT_ROOT } else { 'D:\Qt\6.8.0\msvc2022_64' }
$qtTools = if ($env:AOI_QT_TOOLS) { $env:AOI_QT_TOOLS } else { 'D:\Qt\Tools' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "找不到 vswhere：$vswhere" }

$vsPath = & $vswhere -latest -version '[17.0,18.0)' -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw '找不到含 C++ x64 工具的 Visual Studio 2022' }

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { throw "找不到 vcvars64.bat：$vcvars" }

# 在 cmd 裡跑 vcvars64.bat，再把結果的環境變數搬回目前的 session。
$dump = & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set"
foreach ($line in $dump) {
    $i = $line.IndexOf('=')
    if ($i -gt 0) { Set-Item -Path ("Env:" + $line.Substring(0, $i)) -Value $line.Substring($i + 1) }
}

$extra = @(
    (Join-Path $qtTools 'CMake_64\bin'),
    (Join-Path $qtTools 'Ninja'),
    (Join-Path $qtRoot 'bin')
)
foreach ($p in $extra) {
    if (-not (Test-Path $p)) { throw "找不到路徑：$p" }
}
$env:PATH = ($extra -join ';') + ';' + $env:PATH
$env:AOI_QT_ROOT = $qtRoot

# cl.exe 的 /showIncludes 輸出是中文（這個 VS 只裝了 1028 繁中資源；實測 VSLANG=1033 沒有效果），
# 它的編碼取決於主控台字碼頁。CMake 在 configure 時把當時的前綴（「注意: 包含檔案:」）寫進 build.ninja，
# 之後 build 時字碼頁若不同（例如從 Git Bash 啟動 powershell.exe 就會），Ninja 認不得前綴，
# 標頭檔的相依就沒有記錄：只改標頭檔不會重新編譯，輸出還會被亂碼洪水淹沒（實測發生過）。
# 所以進入開發環境時，把主控台字碼頁固定成系統的 OEM 字碼頁；configure 與 build 都要在這個環境裡做。
try {
    $oemCp = [Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
    if ([Console]::OutputEncoding.CodePage -ne $oemCp) { & chcp.com $oemCp | Out-Null }
} catch { }

Write-Host "MSVC  : $(cmd.exe /c "cl.exe 2>&1" | Select-Object -First 1)"
Write-Host "CMake : $((& cmake.exe --version | Select-Object -First 1))"
Write-Host "Ninja : $(& ninja.exe --version)"
Write-Host "Qt    : $qtRoot"

$ErrorActionPreference = $aoiPrevEap
Remove-Variable aoiPrevEap
