# 開發環境：把 MSVC 2022 (vcvars64)、Qt（qmake）與 jom 放進目前這個 PowerShell session。
#
# 用法（要用 dot-source，環境變數才會留在目前的 session）：
#   . .\scripts\dev\env.ps1
#
# 路徑預設是本機實測的位置（見 AI_CONTEXT.md 的環境表）。換機器時用環境變數覆寫，不要改這個檔：
#   AOI_QT_ROOT     Qt 的 msvc2022_64 資料夾   （預設 D:\Qt\6.12.0\msvc2022_64；S06 起。S02-S05 用 6.8.0）
#   AOI_QT_TOOLS    Qt Tools 資料夾            （預設 D:\Qt\Tools；jom 在 QtCreator\bin\jom）
# VS 2022 用 vswhere 找，不寫死路徑。建置只用 qmake（ADR-018）；CMake 與 Ninja 不再需要。

$aoiPrevEap = $ErrorActionPreference   # dot-source 會改到呼叫端的設定，結尾還原
$ErrorActionPreference = 'Stop'

$qtRoot = if ($env:AOI_QT_ROOT) { $env:AOI_QT_ROOT } else { 'D:\Qt\6.12.0\msvc2022_64' }
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
    (Join-Path $qtRoot 'bin'),
    (Join-Path $qtTools 'QtCreator\bin\jom')
)
foreach ($p in $extra) {
    if (-not (Test-Path $p)) { throw "找不到路徑：$p" }
}
$env:PATH = ($extra -join ';') + ';' + $env:PATH
$env:AOI_QT_ROOT = $qtRoot

# Rust（ADR-031）：rustup 預設裝在 %USERPROFILE%\.cargo\bin（設了 CARGO_HOME 就是 %CARGO_HOME%\bin）。
# 安裝之前就開著的程式（Claude 桌面 app、Qt Creator）的 PATH 沒有它；這裡補上（已經在 PATH 上就不重複）。
# 沒裝 Rust 不算錯：AOI_DOMAIN_IMPL=cpp 的建置用不到 cargo；預設（rust，R05.5 起）找不到 cargo 時 qmake 會報錯（aoi_rust.pri）。
$cargoBin = if ($env:CARGO_HOME) { Join-Path $env:CARGO_HOME 'bin' } else { Join-Path $env:USERPROFILE '.cargo\bin' }
$hasCargo = Test-Path (Join-Path $cargoBin 'cargo.exe')
if ($hasCargo -and -not (($env:PATH -split ';') -contains $cargoBin)) { $env:PATH = $cargoBin + ';' + $env:PATH }

# 把主控台字碼頁固定成系統的 OEM 字碼頁（這台是繁中 Windows，cl.exe 的訊息是中文）。
# CMake＋Ninja 時代這一步是必要的（Ninja 靠 /showIncludes 的中文前綴記錄標頭相依）；qmake 的 Makefile 自己掃 #include，
# 不再依賴它，保留是為了讓 cl 的中文訊息在不同的啟動方式（例如從 Git Bash 啟動 powershell.exe）下一致。
try {
    $oemCp = [Globalization.CultureInfo]::CurrentCulture.TextInfo.OEMCodePage
    if ([Console]::OutputEncoding.CodePage -ne $oemCp) { & chcp.com $oemCp | Out-Null }
} catch { }

Write-Host "MSVC  : $(cmd.exe /c "cl.exe 2>&1" | Select-Object -First 1)"
Write-Host "qmake : $((& qmake.exe -query QT_VERSION))（$qtRoot）"
Write-Host "jom   : $((& jom.exe /VERSION | Select-Object -First 1))"
Write-Host "cargo : $(if ($hasCargo) { (& (Join-Path $cargoBin 'cargo.exe') --version) } else { "沒有（$cargoBin）：預設的建置（rust）會失敗，只能建 AOI_DOMAIN_IMPL=cpp" })"

$ErrorActionPreference = $aoiPrevEap
Remove-Variable aoiPrevEap
