# 建置（qmake ＋ jom）並執行測試，失敗時印出 Qt Test 的 log。建置只用 qmake（ADR-018）。
#
# 用法（在專案根目錄）：
#   .\scripts\dev\test.ps1                          # Release，OpenCV ＋ HALCON（預設全開）
#   .\scripts\dev\test.ps1 -Config Debug            # Debug（Profile 也可以：Release ＋ 除錯資訊，同 Qt Creator）
#   .\scripts\dev\test.ps1 -NoOpenCv -NoHalcon      # 關掉其中一個或兩個；每種組合有自己的建置資料夾
#   .\scripts\dev\test.ps1 -Perf                    # 只跑效能量測（perf_*；要 Release）
#   .\scripts\dev\test.ps1 -Filter 'tst_lattice*'   # 只跑名稱符合的項目（萬用字元）
#   .\scripts\dev\test.ps1 -NoBuild                 # 不建置，直接跑上次建好的
#   .\scripts\dev\test.ps1 -BuildDir AiInspTool\build\Desktop_Qt_6_12_0_MSVC2022_64bit_Debug -Config Debug -NoBuild
#                                                   # 跑 Qt Creator 建好的（-Config 與選項要和那個建置相同）
#
# 建置資料夾：build\qmake-<config>[-noopencv][-nohalcon]（執行檔在 bin\，log 在 test-logs\，測試的工作資料夾 test-work\）。
# 跑哪些測試：qmake 寫的 aoi-tests.txt（-Perf 時 aoi-perf-tests.txt），清單本身在 AiInspTool/tests/tests.pro；
# 不是 -Perf 時再加 5 個 node 的檢查（依賴方向 3 個、S00 示範頁的 fixture 1 個、S10 效能閘門的自我測試 1 個）。
# 一鍵的效能測試（建置 → -Perf → 彙整 → 和基準線與預算比 → 報表）是 scripts/dev/perf.ps1（S10）。
# 環境（CMake 時代由 CTest 設定）：QT_QPA_PLATFORM=offscreen、QT_QPA_FONTDIR、PATH（Qt、HALCON 的 bin；OpenCV 的 DLL 已複製到 bin\）、
# 每個項目逾時 -TimeoutSec 秒、參數 -o <log>,txt（這台機器的 Qt Test 純文字輸出不寫到被接走的 stdout）。
param(
    [ValidateSet('Release', 'Debug', 'Profile')][string]$Config = 'Release',
    [switch]$NoOpenCv,
    [switch]$NoHalcon,
    [switch]$Perf,
    [string]$Filter = '*',
    [switch]$NoBuild,
    [string]$BuildDir = '',
    [int]$TimeoutSec = 300,
    [int]$Jobs = 0
)

. "$PSScriptRoot\env.ps1" | Out-Null

$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$name = 'qmake-' + $Config.ToLower()
if ($NoOpenCv) { $name += '-noopencv' }
if ($NoHalcon) { $name += '-nohalcon' }
$build = Join-Path $root "build\$name"
if ($BuildDir) {
    $build = if ([IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }
    $name = Split-Path $build -Leaf
}
$bin = Join-Path $build 'bin'
$logs = Join-Path $build 'test-logs'
$work = Join-Path $build 'test-work'

if ($Perf -and $Config -eq 'Debug') { Write-Warning 'Debug 的效能數字沒有意義（沒最佳化），預算也不當成失敗條件。' }

# ---- 建置：和 Qt Creator 一樣，qmake → jom qmake_all → jom ----
if (-not $NoBuild) {
    $qmakeArgs = @((Join-Path $root 'AiInspTool\AiInspTool.pro'), '-spec', 'win32-msvc')
    switch ($Config) {
        'Debug' { $qmakeArgs += @('CONFIG+=debug', 'CONFIG+=qml_debug') }
        'Profile' { $qmakeArgs += @('CONFIG+=qml_debug', 'CONFIG+=force_debug_info', 'CONFIG+=separate_debug_info') }
    }
    if ($NoOpenCv) { $qmakeArgs += 'AOI_WITH_OPENCV=OFF' }
    if ($NoHalcon) { $qmakeArgs += 'AOI_WITH_HALCON=OFF' }

    New-Item -ItemType Directory -Force $build | Out-Null
    Push-Location $build
    try {
        & qmake.exe @qmakeArgs
        if ($LASTEXITCODE) { Write-Host "qmake 失敗（結束碼 $LASTEXITCODE）"; exit $LASTEXITCODE }
        & jom.exe qmake_all
        if ($LASTEXITCODE) { Write-Host "jom qmake_all 失敗（結束碼 $LASTEXITCODE）"; exit $LASTEXITCODE }
        $jomArgs = if ($Jobs -gt 0) { @('/J', "$Jobs") } else { @() }
        & jom.exe @jomArgs
        if ($LASTEXITCODE) { Write-Host "建置失敗（jom 結束碼 $LASTEXITCODE）"; exit $LASTEXITCODE }
    } finally {
        Pop-Location
    }
}

# ---- 要跑的項目 ----
$manifest = Join-Path $build $(if ($Perf) { 'aoi-perf-tests.txt' } else { 'aoi-tests.txt' })
if (-not (Test-Path $manifest)) { Write-Host "找不到 $manifest：先建置（不要加 -NoBuild）。"; exit 1 }
$tests = @(Get-Content $manifest | Where-Object { $_ -and ($_ -like $Filter) })

# node 的檢查：名稱、參數、要成功（0）還是要失敗（非 0）。
$nodeChecks = @()
if (-not $Perf) {
    $nodeChecks = @(
        @{ Name = 'check_deps'; Args = @('tools/check-deps.mjs', '--root', $root); ExpectFail = $false },
        @{ Name = 'check_deps_self_test'; Args = @('tools/check-deps.mjs', '--self-test'); ExpectFail = $false },
        @{ Name = 'check_deps_catches_a_violation'; Args = @('tools/check-deps.mjs', '--root', 'tests/core/fixtures/deps/domain-qt'); ExpectFail = $true },
        @{ Name = 's00_demo_fixture_is_current'; Args = @('tools/s00-demo-fixture.mjs', '--check'); ExpectFail = $false },
        @{ Name = 'perf_compare_self_test'; Args = @('tools/perf-compare.mjs', '--self-test'); ExpectFail = $false }
    ) | Where-Object { $_.Name -like $Filter }
}

New-Item -ItemType Directory -Force $logs, $work | Out-Null

# 測試的環境：已經有設定的不覆寫（例如要在真的視窗上量測時先設 QT_QPA_PLATFORM=windows）。
if (-not $env:QT_QPA_PLATFORM) { $env:QT_QPA_PLATFORM = 'offscreen' }
if (-not $env:QT_QPA_FONTDIR) { $env:QT_QPA_FONTDIR = (Join-Path $env:WINDIR 'Fonts') }
$pathParts = @((Join-Path $env:AOI_QT_ROOT 'bin'), $bin)
$halconBin = if ($env:HALCONROOT) { Join-Path $env:HALCONROOT 'bin\x64-win64' } else { '' }
$env:PATH = ($pathParts -join ';') + ';' + $env:PATH
if ($halconBin -and (Test-Path $halconBin)) { $env:PATH = $env:PATH + ';' + $halconBin }

$results = New-Object System.Collections.Generic.List[object]

function Invoke-AoiItem([string]$display, [string]$file, [string[]]$arguments, [string]$log, [bool]$expectFail, [bool]$captureOutput) {
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $file
    $psi.Arguments = ($arguments | ForEach-Object { if ($_ -match '\s') { '"' + $_ + '"' } else { $_ } }) -join ' '
    $psi.WorkingDirectory = $(if ($captureOutput) { $root } else { $work })
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $captureOutput
    $psi.RedirectStandardError = $captureOutput
    $watch = [Diagnostics.Stopwatch]::StartNew()
    $process = [Diagnostics.Process]::Start($psi)
    $outTask = $null; $errTask = $null
    if ($captureOutput) { $outTask = $process.StandardOutput.ReadToEndAsync(); $errTask = $process.StandardError.ReadToEndAsync() }
    $finished = $process.WaitForExit($TimeoutSec * 1000)
    if (-not $finished) { & taskkill.exe /T /F /PID $process.Id 2>&1 | Out-Null; $process.WaitForExit() }
    $watch.Stop()
    if ($captureOutput) { ($outTask.Result + $errTask.Result) | Out-File -Encoding utf8 $log }
    $code = $process.ExitCode
    $passed = $finished -and ($(if ($expectFail) { $code -ne 0 } else { $code -eq 0 }))
    $skipped = 0
    $xfails = @()
    if (-not $captureOutput -and (Test-Path $log)) {
        $totals = Select-String -Path $log -Pattern 'Totals: \d+ passed, \d+ failed, (\d+) skipped' -Encoding UTF8 | Select-Object -Last 1
        if ($totals) { $skipped = [int]$totals.Matches[0].Groups[1].Value }
        # 預期失敗（QEXPECT_FAIL）：Qt Test 寫成 XFAIL，算通過；列出「測試函式(資料列)」。修好時會變成 XPASS（失敗）。
        $xfails = @(Select-String -Path $log -Pattern '^XFAIL\s*:\s*[^:]+::(\S+?\([^)]*\))' -Encoding UTF8 | ForEach-Object { $_.Matches[0].Groups[1].Value })
    }
    $status = if (-not $finished) { 'TIMEOUT' } elseif ($passed) { 'PASS' } else { 'FAIL' }
    $notes = @()
    if ($skipped -gt 0) { $notes += "$skipped 個項目跳過" }
    if ($xfails.Count -gt 0) { $notes += "$($xfails.Count) 個預期失敗（XFAIL）" }
    $note = if ($notes.Count) { "（$($notes -join '、')，見 log）" } else { '' }
    Write-Host ('{0,-8} {1,-34} {2,7:N1} s {3}' -f $status, $display, $watch.Elapsed.TotalSeconds, $note)
    $results.Add([pscustomobject]@{ Name = $display; Status = $status; Code = $code; Log = $log; Skipped = $skipped; XFails = $xfails })
}

Write-Host ''
Write-Host "== $name：$($tests.Count) 個測試執行檔$(if ($nodeChecks.Count) { " ＋ $($nodeChecks.Count) 個 node 檢查" })（逾時 $TimeoutSec 秒）"
foreach ($test in $tests) {
    $exe = Join-Path $bin "$test.exe"
    $log = Join-Path $logs "$test.txt"
    if (-not (Test-Path $exe)) {
        Write-Host ('{0,-8} {1,-34} 找不到 {2}' -f 'FAIL', $test, $exe)
        $results.Add([pscustomobject]@{ Name = $test; Status = 'FAIL'; Code = -1; Log = ''; Skipped = 0; XFails = @() })
        continue
    }
    Remove-Item $log -ErrorAction SilentlyContinue
    Invoke-AoiItem $test $exe @('-o', "$log,txt") $log $false $false
}

if ($nodeChecks.Count) {
    $node = Get-Command node.exe -ErrorAction SilentlyContinue
    foreach ($check in $nodeChecks) {
        $log = Join-Path $logs "$($check.Name).txt"
        if (-not $node) {
            Write-Host ('{0,-8} {1,-34} 找不到 node（Node.js 要在 PATH 上）' -f 'FAIL', $check.Name)
            $results.Add([pscustomobject]@{ Name = $check.Name; Status = 'FAIL'; Code = -1; Log = ''; Skipped = 0; XFails = @() })
            continue
        }
        Invoke-AoiItem $check.Name $node.Source $check.Args $log $check.ExpectFail $true
    }
}

# ---- 結果 ----
$failed = @($results | Where-Object { $_.Status -ne 'PASS' })
$withSkips = @($results | Where-Object { $_.Skipped -gt 0 })
$withXFails = @($results | Where-Object { $_.XFails.Count -gt 0 })
Write-Host ''
Write-Host "通過 $($results.Count - $failed.Count)／$($results.Count)$(if ($withSkips.Count) { "；$($withSkips.Count) 個測試有跳過的項目（例如 HALCON 讀不到授權時整組跳過）：$(($withSkips | ForEach-Object Name) -join '、')" })"
# 預期失敗（XFAIL）：已知、待修的問題（例如 ADR-004 §5 的 B1–B3），不是通過也不是失敗；修好時 Qt Test 判成 XPASS（失敗），要拿掉 QEXPECT_FAIL。
if ($withXFails.Count) {
    Write-Host "$($withXFails.Count) 個測試有預期失敗（XFAIL）的項目（已知、待修）："
    foreach ($x in $withXFails) { Write-Host "  $($x.Name)：$($x.XFails -join '、')" }
}
if ($failed.Count) {
    Write-Host ''
    Write-Host "--- 沒有通過的項目（log 在 $logs）---"
    foreach ($f in $failed) {
        Write-Host "== $($f.Name)：$($f.Status)，結束碼 $($f.Code)"
        if ($f.Log -and (Test-Path $f.Log)) {
            Get-Content $f.Log -Encoding UTF8 | Where-Object { $_ -match '^FAIL|^XPASS|Loc:|Actual|Expected|Totals|error|Error' } | Select-Object -First 40
        }
    }
    exit 1
}
exit 0
