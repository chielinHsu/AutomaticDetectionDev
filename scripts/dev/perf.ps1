# 效能測試一鍵執行（S10）：建置 → 跑所有效能量測（test.ps1 -Perf）→ 彙整成一份執行紀錄 → 和基準線與預算比（閘門）→ 互動式報表。
#
# 用法（在專案根目錄）：
#   .\scripts\dev\perf.ps1                         # Release，建置＋全部效能量測，和 perf/baseline/<機器>-harness.json 比
#   .\scripts\dev\perf.ps1 -NoBuild                # 不建置
#   .\scripts\dev\perf.ps1 -Runs 3                 # 連跑 3 次（每次一份執行紀錄），最後算幾次之間的雜訊
#   .\scripts\dev\perf.ps1 -Runs 3 -WriteBaseline  # 連跑 3 次之後，用這 3 次建新的基準線（覆寫 perf/baseline/<機器>-harness.json）
#   .\scripts\dev\perf.ps1 -Filter 'perf_matrix'   # 只跑名稱符合的量測（閘門只比這些指標；基準線裡其他的指標列成「缺少」）
#   .\scripts\dev\perf.ps1 -Window                 # 真機視窗模式（QT_QPA_PLATFORM=windows）：量 GPU 與實際顯示相關的指標，結果另外標示
#   .\scripts\dev\perf.ps1 -Profile full           # perf_matrix 跑整個外積（540 格；逾時自動放大到 3600 秒）
#   .\scripts\dev\perf.ps1 -InjectDelayMs 60       # harness 的自我測試：每次量測多 60 ms，閘門應該失敗（結果不能當基準線）
#
# 輸出（perf/results/，不入版控）：<yyyyMMdd-HHmmss>-<commit>-<機器>.json（執行紀錄）、同名的 .html（報表）、-compare.json（閘門的結果）。
# 結束碼 ＝ 閘門的結束碼（tools/perf-compare.mjs）：0 通過、1 失敗、2 輸入有問題、3 無法判定（整台機器變慢）；-Runs 多次時取最差的。
# 量測前的建議（docs/steps/S10-perf-harness/spec.md「穩定性」）：接電源、電源計畫用高效能或平衡、關掉瀏覽器與其他會吃 CPU 的程式、
# 不要同時建置；每次執行紀錄都有量測當下的 CPU 負載與時脈，報表會標出負載高的那一次。
param(
    [ValidateSet('Release', 'Profile')][string]$Config = 'Release',
    [switch]$NoOpenCv,
    [switch]$NoHalcon,
    [switch]$NoBuild,
    [string]$Filter = '*',
    [int]$Runs = 1,
    [switch]$WriteBaseline,
    [string]$Baseline = '',
    [switch]$Window,
    [ValidateSet('quick', 'full')][string]$Profile = 'quick',
    [double]$InjectDelayMs = 0,
    [int]$TimeoutSec = 600
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$name = 'qmake-' + $Config.ToLower()
if ($NoOpenCv) { $name += '-noopencv' }
if ($NoHalcon) { $name += '-nohalcon' }
$build = Join-Path $root "build\$name"
$resultsDir = Join-Path $root 'perf\results'
New-Item -ItemType Directory -Force $resultsDir | Out-Null

$node = Get-Command node.exe -ErrorAction SilentlyContinue
if (-not $node) { Write-Host '找不到 node（Node.js 要在 PATH 上）'; exit 2 }
$machine = ($env:COMPUTERNAME).ToLower()
if (-not $Baseline) { $Baseline = Join-Path $root "perf\baseline\$machine-harness.json" }
if ($Profile -eq 'full' -and $TimeoutSec -lt 3600) { $TimeoutSec = 3600 }

if ($Window) { $env:QT_QPA_PLATFORM = 'windows' }
$env:AOI_PERF_PROFILE = $Profile
if ($InjectDelayMs -gt 0) { $env:AOI_PERF_INJECT_DELAY_MS = "$InjectDelayMs" } else { Remove-Item Env:AOI_PERF_INJECT_DELAY_MS -ErrorAction SilentlyContinue }

# 量測當下的環境（任務 8）：電源計畫、是否接電源、CPU 負載與時脈（WMI 的瞬間值；每個量測執行檔自己也會再取樣一次寫進結果）。
function Get-Environment {
    $e = [ordered]@{ sampledAt = (Get-Date).ToString('o') }
    # 電源計畫：用 WMI（powercfg 的輸出是 OEM 字碼頁，中文的名稱寫進 JSON 會壞掉）。
    try {
        $plan = Get-CimInstance -Namespace root\cimv2\power -ClassName Win32_PowerPlan -ErrorAction Stop | Where-Object { $_.IsActive } | Select-Object -First 1
        if ($plan) { $e.powerScheme = $plan.ElementName }
    } catch { }
    try {
        $cpu = Get-CimInstance Win32_Processor | Select-Object -First 1
        $e.cpuLoadPercent = [int]$cpu.LoadPercentage
        $e.cpuMhz = [int]$cpu.CurrentClockSpeed
        $e.cpuMaxMhz = [int]$cpu.MaxClockSpeed
    } catch { }
    try {
        $battery = Get-CimInstance Win32_Battery -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($battery) { $e.acPower = ($battery.BatteryStatus -ne 1); $e.batteryPercent = [int]$battery.EstimatedChargeRemaining }
    } catch { }
    try { $e.processCount = (Get-Process).Count } catch { }
    # 處理器實際跑的速度對額定時脈的百分比（Windows 的效能計數器；1 秒的取樣）。S10 實測：同一台機器跑在 67% 時，T3、U4、I3 的
    # 數字是 S04–S06 的 1.5–2 倍，閘門會把預算判成失敗——報表要讓人看到這個數字，不要去猜程式為什麼慢。
    try {
        $perf = Get-Counter '\Processor Information(_Total)\% Processor Performance' -ErrorAction Stop
        $e.cpuPerformancePercent = [math]::Round($perf.CounterSamples[0].CookedValue, 1)
    } catch { }
    try { $e.topProcesses = @(Get-Process | Sort-Object CPU -Descending | Select-Object -First 5 | ForEach-Object { $_.Name }) } catch { }
    $e.qpaPlatform = $(if ($Window) { 'windows' } else { 'offscreen' })
    $e.profile = $Profile
    $e.injectedDelayMs = $InjectDelayMs
    return $e
}

$commit = (& git -C $root rev-parse --short HEAD 2>$null)
if (-not $commit) { $commit = 'unknown' }
$dirty = (& git -C $root status --porcelain 2>$null)
if ($dirty) { $commit += '+uncommitted' }

$runFiles = @()
$worst = 0
for ($i = 1; $i -le $Runs; $i++) {
    $stamp = (Get-Date).ToString('yyyyMMdd-HHmmss')
    $runBase = Join-Path $resultsDir "$stamp-$commit-$machine"
    $envFile = "$runBase-env.json"
    (Get-Environment | ConvertTo-Json -Depth 4) | Out-File -Encoding utf8 $envFile
    $since = (Get-Date).AddSeconds(-1).ToString('o')
    Write-Host ''
    Write-Host "== perf run $i／$Runs（$name，profile $Profile$(if ($Window) { '，windows 平台' })$(if ($InjectDelayMs -gt 0) { "，注入延遲 $InjectDelayMs ms" })）"

    $testArgs = @('-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'test.ps1'), '-Perf', '-Config', $Config, '-Filter', $Filter, '-TimeoutSec', "$TimeoutSec")
    if ($NoOpenCv) { $testArgs += '-NoOpenCv' }
    if ($NoHalcon) { $testArgs += '-NoHalcon' }
    if ($NoBuild -or $i -gt 1) { $testArgs += '-NoBuild' }
    & powershell.exe @testArgs
    $testCode = $LASTEXITCODE
    if ($testCode -ne 0) { Write-Host "test.ps1 -Perf 的結束碼 $testCode（有量測執行檔失敗或逾時；閘門照樣比有結果的部分）" }

    $runFile = "$runBase.json"
    & $node.Source (Join-Path $root 'tools\perf-compare.mjs') collect --since $since --results $resultsDir --work (Join-Path $build 'test-work\perf-results') `
        --logs (Join-Path $build 'test-logs') --env $envFile --out $runFile
    if ($LASTEXITCODE -ne 0) { Write-Host '彙整失敗（沒有結果檔？）'; exit 2 }
    $runFiles += $runFile

    & $node.Source (Join-Path $root 'tools\perf-compare.mjs') compare $runFile --baseline $Baseline --json "$runBase-compare.json" --html "$runBase.html"
    $gate = $LASTEXITCODE
    if ($testCode -ne 0 -and $gate -eq 0) { $gate = 1 }
    if ($gate -gt $worst) { $worst = $gate }
    Write-Host "報表：$runBase.html"
}

if ($Runs -gt 1) {
    Write-Host ''
    & $node.Source (Join-Path $root 'tools\perf-compare.mjs') noise @runFiles
}
if ($WriteBaseline) {
    if ($InjectDelayMs -gt 0) { Write-Host '注入延遲的結果不能當基準線（-InjectDelayMs 與 -WriteBaseline 不能一起用）'; exit 2 }
    Write-Host ''
    & $node.Source (Join-Path $root 'tools\perf-compare.mjs') baseline @runFiles --out $Baseline
    if ($LASTEXITCODE -ne 0) { exit 2 }
}
exit $worst
