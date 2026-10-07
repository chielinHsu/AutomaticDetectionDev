# 建置並執行單元測試（不含 perf），失敗時印出 Qt Test 的 log。
#
# 用法（在專案根目錄）：
#   .\scripts\dev\test.ps1                 # release
#   .\scripts\dev\test.ps1 -Preset debug
#   .\scripts\dev\test.ps1 -Perf           # 只跑 label=perf（效能基準線，要 release）
#   .\scripts\dev\test.ps1 -Preset release-opencv   # with AOI_WITH_OPENCV=ON (S06); -Perf works with it too
#   .\scripts\dev\test.ps1 -Preset release-halcon   # OpenCV + HALCON (S08); -Perf works with it too
param(
    [ValidateSet('debug', 'release', 'relwithdebinfo', 'debug-opencv', 'release-opencv', 'debug-halcon', 'release-halcon')][string]$Preset = 'release',
    [switch]$Perf
)

. "$PSScriptRoot\env.ps1" | Out-Null

$root = Resolve-Path "$PSScriptRoot\..\.."
Push-Location $root
try {
    if (-not (Test-Path "build\$Preset\build.ninja")) { cmake --preset $Preset; if ($LASTEXITCODE) { exit $LASTEXITCODE } }
    cmake --build "build\$Preset"
    if ($LASTEXITCODE) { exit $LASTEXITCODE }

    $testPreset = if ($Perf) {
        switch ($Preset) { 'release-opencv' { 'perf-opencv' } 'release-halcon' { 'perf-halcon' } default { 'perf' } }
    } else { $Preset }
    ctest --preset $testPreset
    $code = $LASTEXITCODE
    if ($code) {
        Write-Host ''
        Write-Host '--- 失敗測試的 log（build\<preset>\test-logs）---'
        Get-ChildItem "build\$Preset\test-logs\*.txt" | ForEach-Object {
            $text = Get-Content $_.FullName -Encoding UTF8
            if ($text -match '^FAIL') { Write-Host "== $($_.Name)"; $text | Where-Object { $_ -match '^FAIL|Loc:|Actual|Expected|Totals' } }
        }
    }
    exit $code
} finally {
    Pop-Location
}
