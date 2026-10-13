#!/usr/bin/env node
// 效能回歸閘門（S10，ADR-023）：把一次 test.ps1 -Perf 的所有結果檔彙整成一份執行紀錄，和基準線與預算比，給出結束碼。
//
// 用法：
//   node tools/perf-compare.mjs collect --since <ISO 時間> --out <run.json> [--results perf/results] [--work <建置>/test-work/perf-results]
//                                       [--logs <建置>/test-logs] [--env <env.json>]
//       讀 since 之後寫出的結果檔（perf_core … perf_ab、perf_matrix、perf_e2e、ui-*），正規化成同一種指標列，加上測試執行檔的狀態
//       （log 的 Totals、SKIP、XFAIL），寫成 perf/results/<日期>-<commit>-<機器>.json（scripts/dev/perf.ps1 會呼叫）。
//   node tools/perf-compare.mjs compare <run.json> [--baseline perf/baseline/<機器>-harness.json] [--budgets perf/budgets.json]
//                                       [--limit 15] [--json <out.json>] [--html <out.html>]
//       閘門。結束碼：0 通過；1 失敗（超出預算而且不是已知的，或比基準線慢超過 limit%）；2 輸入有問題；3 無法判定（整台機器變慢）。
//   node tools/perf-compare.mjs baseline <run.json>... --out <baseline.json> [--force]
//       從幾次執行建基準線：每個指標記中位數與 p95 的範圍。拒絕 injectedDelayMs > 0 的結果；機器或 build type 不同時要 --force。
//   node tools/perf-compare.mjs noise <run.json> <run.json>...
//       量測雜訊：同一份程式幾次執行之間，每個指標中位數的最大／最小比；列出超過 limit% 的。
//   node tools/perf-compare.mjs html <run.json> [--baseline <file>] [--budgets <file>] [--template docs/steps/S10-perf-harness/explainer.html] --out <file>
//       把執行紀錄與比較結果嵌進互動式報表（explainer.html 的 aoi-perf-run 區塊）。
//   node tools/perf-compare.mjs --self-test
//       用程式裡造的資料驗證閘門的規則（抓得到變慢、已知的不擋、跳過不是通過、整台變慢不下結論…）。
//
// 規則（docs/steps/S10-perf-harness/spec.md、ADR-023）：
//  - 預算：p95（越大越好的指標看最小值）超過 limit ＝ 超標。已知的不當失敗，但一定列出：指標自己標的 known、perf/budgets.json 裡
//    status 是 measured-over-limit／calibrated-at-limit 的 id、budgets.json 的 knownIssues。
//  - 基準線：中位數比基準線「幾次執行的最大中位數」還慢超過 limit%（預設 15，budgets.json 的 regressionLimitPercent）＝ 回歸；
//    單位是 ms 的另外要慢超過 0.05 ms（微秒等級的指標不看百分比）。越大越好的指標反過來。只看 ms、MB、px、percent 的指標。
//  - 整台機器變慢：回歸的指標佔比較過的一半以上時，不下「回歸」的結論（結束碼 3），因為同一份程式在這台機器兩次就可能差 2–3 倍。
//  - 跳過不是通過：log 有 SKIP 的執行檔列成「未驗證」；XFAIL 列成「已知、待修」。兩者都不改結束碼。
//  - 沒有基準線的指標只看預算；基準線有、這次沒有的指標列成「缺少」（例如 HALCON 讀不到授權）。
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs';
import { basename, dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { hostname } from 'node:os';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const DEFAULT_LIMIT = 15;
const GATED_UNITS = new Set(['ms', 'MB', 'px', 'percent']);
// 絕對下限：差不到這麼多不算回歸（微秒等級的時間、1 ms 取樣的峰值記憶體在幾 MB 內是雜訊：S10 實測）。
const MIN_ABS = { ms: 0.05, MB: 32, px: 0.01, percent: 0.1 };

// ---------------------------------------------------------------------------------------------
// 參數
// ---------------------------------------------------------------------------------------------
function parseArgs(argv) {
  const out = { _: [], flags: {} };
  for (let i = 0; i < argv.length; ++i) {
    const a = argv[i];
    if (a.startsWith('--')) {
      const name = a.slice(2);
      const next = argv[i + 1];
      if (next !== undefined && !next.startsWith('--')) {
        out.flags[name] = next;
        ++i;
      } else out.flags[name] = true;
    } else out._.push(a);
  }
  return out;
}

function readJson(path) {
  // PowerShell 的 Out-File -Encoding utf8 會加 BOM（perf.ps1 的環境快照）：JSON.parse 不吃，先拿掉。
  return JSON.parse(readFileSync(path, 'utf8').replace(/^﻿/, ''));
}

function round3(v) {
  return Math.round(v * 1000) / 1000;
}

// ---------------------------------------------------------------------------------------------
// 結果檔 → 統一的指標列
// ---------------------------------------------------------------------------------------------
const PREFIX_SOURCES = [
  ['core_', 'core'], ['lattice_', 'lattice'], ['storage_', 'storage'], ['opencv_', 'opencv'], ['halcon_', 'halcon'],
  ['ab_', 'ab'], ['matrix_', 'matrix'], ['e2e_', 'e2e'], ['ui-baseline-', 'ui-baseline'], ['ui-s04-', 'ui-s04'], ['ui-compare-', 'ui-compare'],
];

export function sourceOf(fileName, root) {
  if (root && root.kind === 'perf_ab') return 'ab';
  if (root && root.kind === 'perf_matrix') return 'matrix';
  if (root && root.kind === 'perf_e2e') return 'e2e';
  for (const [prefix, source] of PREFIX_SOURCES) if (fileName.startsWith(prefix)) return source;
  return null;
}

function metricFromRow(source, m, budgetIds) {
  const unit = m.unit ?? 'ms';
  // 越大越好的指標：結果檔標 direction／higherIsBetter，或只有 p05 沒有 p95（perf_opencv、perf_halcon 的 T6a 那類）。
  const direction = m.direction === 'min' || m.higherIsBetter || (m.p05 !== undefined && m.p95 === undefined) ? 'min' : 'max';
  const budget = typeof m.budget === 'number' ? m.budget : null;
  let budgetId = m.budgetId ?? null;
  if (!budgetId && budget !== null && budgetIds.has(m.id)) budgetId = m.id;
  const row = {
    key: `${source}/${m.id}`, source, id: m.id, label: m.label ?? m.id, unit, n: m.n ?? null,
    median: m.median ?? null, p95: m.p95 ?? m.median ?? null, min: m.min ?? null, max: m.max ?? null, p05: m.p05 ?? null,
    budget, budgetId, direction,
  };
  if (m.known) row.known = m.known;
  if (m.note) row.note = m.note;
  return row;
}

// perf_ab 的 runs[]（每個情境 × back end 一列）：時間、召回率、位置誤差、峰值記憶體各一個指標。
function metricsFromAb(root) {
  const out = [];
  for (const r of root.runs ?? []) {
    const where = `${r.label ?? r.scenario}, ${r.backend}`;
    const prefix = `${r.backend}:${r.scenario}:`;
    out.push({ key: `ab/${prefix}total`, source: 'ab', id: `${prefix}total`, label: `use case end to end (A/B board): ${where}`, unit: 'ms', n: r.n ?? null,
      median: r.totalMedianMs, p95: r.totalP95Ms, budget: null, budgetId: null, direction: 'max' });
    out.push({ key: `ab/${prefix}recall`, source: 'ab', id: `${prefix}recall`, label: `recall at 0.70 (A/B board): ${where}`, unit: 'percent', n: 1,
      median: r.recallPercent, p95: r.recallPercent, min: r.recallPercent, budget: null, budgetId: 'T6a', direction: 'min',
      note: `${r.found} of ${r.present}; ${r.falseFound} false` });
    out.push({ key: `ab/${prefix}errorPx`, source: 'ab', id: `${prefix}errorPx`, label: `position error p95 (A/B board): ${where}`, unit: 'px', n: r.found ?? null,
      median: r.errorP95Px, p95: r.errorP95Px, max: r.errorMaxPx, budget: null, budgetId: 'T6b', direction: 'max' });
    out.push({ key: `ab/${prefix}peakMb`, source: 'ab', id: `${prefix}peakMb`, label: `peak private bytes (A/B board): ${where}`, unit: 'MB', n: 1,
      median: r.peakMb, p95: r.peakMb, budget: null, budgetId: null, direction: 'max' });
  }
  return out;
}

export function normalizeResult(fileName, root, budgets) {
  const source = sourceOf(fileName, root);
  if (!source) return null;
  const budgetIds = new Set((budgets?.budgets ?? []).map((b) => b.id));
  const metrics = source === 'ab' ? metricsFromAb(root) : (root.metrics ?? []).map((m) => metricFromRow(source, m, budgetIds));
  // 預算的 limit 以 budgets.json 為準（結果檔裡的是量測當時讀到的值）。
  for (const m of metrics) {
    if (m.budgetId) {
      const b = (budgets?.budgets ?? []).find((x) => x.id === m.budgetId);
      if (b && typeof b.limit === 'number' && m.budget === null) m.budget = b.limit;
      if (b && b.direction === 'min') m.direction = 'min';
    }
  }
  return {
    source, file: fileName, kind: root.kind ?? null, step: root.step ?? null, measuredAt: root.measuredAt ?? null,
    machine: root.machine ?? null, injectedDelayMs: root.injectedDelayMs ?? 0, metrics,
    extra: pickExtra(source, root),
  };
}

// 報表要用的附加資料（矩陣的格子、T5 的階段、A/B 的比較）。
function pickExtra(source, root) {
  if (source === 'matrix') return { cells: root.cells ?? [], profile: root.profile ?? null, environment: root.environment ?? null };
  if (source === 'e2e') return { phases: root.phases ?? {}, environment: root.environment ?? null };
  if (source === 'ab') return { comparisons: root.comparisons ?? [] };
  if (source === 'ui-s04') return { scalingWithObjectCount: root.scalingWithObjectCount ?? [] };
  return null;
}

// ---------------------------------------------------------------------------------------------
// Qt Test 的 log → 執行檔的狀態（跳過不是通過；XFAIL 是已知、待修）
// ---------------------------------------------------------------------------------------------
export function parseQtTestLog(name, text) {
  const t = { name, passed: null, failed: null, skipped: null, skips: [], xfails: [], fails: [], status: 'unknown' };
  const totals = /Totals: (\d+) passed, (\d+) failed, (\d+) skipped/.exec(text);
  if (totals) {
    t.passed = Number(totals[1]);
    t.failed = Number(totals[2]);
    t.skipped = Number(totals[3]);
  }
  for (const line of text.split(/\r?\n/)) {
    // 「Class::func(row) 訊息」：函式名稱是 \w+，資料列在括號裡（列名可以有空白、| 等）。
    let m = /^SKIP\s*:\s*\S+?::(\w+(?:\([^)]*\))?)\s*(.*)$/.exec(line);
    if (m) t.skips.push({ where: m[1], reason: m[2].trim() });
    m = /^XFAIL\s*:\s*\S+?::(\w+(?:\([^)]*\))?)\s*(.*)$/.exec(line);
    if (m) t.xfails.push({ where: m[1], message: m[2].trim() });
    m = /^(?:FAIL!|XPASS)\s*:\s*\S+?::(\w+(?:\([^)]*\))?)\s*(.*)$/.exec(line);
    if (m) t.fails.push({ where: m[1], message: m[2].trim() });
  }
  if (!totals) t.status = 'incomplete';
  else if (t.failed > 0 || t.fails.length > 0) t.status = 'fail';
  else if (t.skipped > 0) t.status = 'pass-with-skips';
  else t.status = 'pass';
  return t;
}

function collectLogs(logsDir, sinceMs) {
  if (!logsDir || !existsSync(logsDir)) return [];
  const out = [];
  for (const f of readdirSync(logsDir)) {
    if (!f.endsWith('.txt')) continue;
    const path = join(logsDir, f);
    const st = statSync(path);
    if (sinceMs && st.mtimeMs < sinceMs) continue;
    out.push(parseQtTestLog(f.replace(/\.txt$/, ''), readFileSync(path, 'utf8')));
  }
  return out;
}

// ---------------------------------------------------------------------------------------------
// collect
// ---------------------------------------------------------------------------------------------
function listResultFiles(dir, sinceMs) {
  if (!dir || !existsSync(dir)) return [];
  return readdirSync(dir)
    .filter((f) => f.endsWith('.json') && !/^\d{8}-\d{6}-/.test(f))  // 不讀彙整過的執行紀錄本身
    .map((f) => ({ f, path: join(dir, f), mtimeMs: statSync(join(dir, f)).mtimeMs }))
    .filter((e) => !sinceMs || e.mtimeMs >= sinceMs)
    .sort((a, b) => a.mtimeMs - b.mtimeMs);
}

export function buildRun({ files, logs, budgets, env, since }) {
  const sources = [];
  const metrics = [];
  const seen = new Map();
  for (const { f, root } of files) {
    const norm = normalizeResult(f, root, budgets);
    if (!norm) continue;
    // 同一種來源有兩個檔（例如跑了兩次）：留最後的那一個。
    if (seen.has(norm.source)) {
      const index = seen.get(norm.source);
      sources[index] = norm;
    } else {
      seen.set(norm.source, sources.length);
      sources.push(norm);
    }
  }
  for (const s of sources) for (const m of s.metrics) metrics.push(m);
  const first = sources.find((s) => s.machine) ?? null;
  const machine = first ? { ...first.machine } : { machineName: hostname() };
  const injected = sources.reduce((acc, s) => Math.max(acc, s.injectedDelayMs ?? 0), 0);
  return {
    schema: 'aoi-perf-run/1',
    collectedAt: new Date().toISOString(),
    since: since ?? null,
    machine,
    environment: env ?? null,
    injectedDelayMs: injected,
    sources: sources.map((s) => ({ source: s.source, file: s.file, kind: s.kind, step: s.step, measuredAt: s.measuredAt, metrics: s.metrics.length, injectedDelayMs: s.injectedDelayMs })),
    tests: logs,
    metrics,
    extra: Object.fromEntries(sources.filter((s) => s.extra).map((s) => [s.source, s.extra])),
  };
}

function cmdCollect(args) {
  const sinceRaw = args.flags.since;
  const sinceMs = sinceRaw ? (Number.isFinite(Number(sinceRaw)) ? Number(sinceRaw) : Date.parse(sinceRaw)) : 0;
  if (sinceRaw && !Number.isFinite(sinceMs)) return fail(2, `--since is not a time: ${sinceRaw}`);
  const out = args.flags.out;
  if (!out) return fail(2, 'collect needs --out <run.json>');
  const budgets = readJson(resolve(REPO_ROOT, args.flags.budgets ?? 'perf/budgets.json'));
  const dirs = [args.flags.results ?? join(REPO_ROOT, 'perf', 'results'), args.flags.work].filter(Boolean).map((d) => resolve(d));
  const files = [];
  for (const dir of dirs) for (const e of listResultFiles(dir, sinceMs)) files.push({ f: e.f, root: readJson(e.path) });
  const logs = collectLogs(args.flags.logs ? resolve(args.flags.logs) : null, sinceMs);
  const env = args.flags.env && existsSync(args.flags.env) ? readJson(args.flags.env) : null;
  const run = buildRun({ files, logs, budgets, env, since: sinceRaw ?? null });
  if (run.metrics.length === 0) {
    console.error(`collect: no result file newer than ${sinceRaw ?? 'the beginning'} in ${dirs.join(', ')}`);
    return 2;
  }
  mkdirSync(dirname(resolve(out)), { recursive: true });
  writeFileSync(resolve(out), JSON.stringify(run, null, 1));
  console.log(`collected ${run.metrics.length} metrics from ${run.sources.length} result files and ${run.tests.length} test logs → ${out}`);
  for (const s of run.sources) console.log(`  ${s.source.padEnd(12)} ${String(s.metrics).padStart(4)} metrics  ${s.file}${s.injectedDelayMs ? `  (injected delay ${s.injectedDelayMs} ms)` : ''}`);
  return 0;
}

// ---------------------------------------------------------------------------------------------
// compare（閘門的規則）
// ---------------------------------------------------------------------------------------------
function gateValue(m) {
  if (m.direction === 'min') return m.p05 ?? m.min ?? m.median;
  return m.p95 ?? m.median;
}

function knownReason(m, budgets) {
  if (m.known) return m.known;
  const entry = m.budgetId ? (budgets?.budgets ?? []).find((b) => b.id === m.budgetId) : null;
  if (entry && (entry.status === 'measured-over-limit' || entry.status === 'calibrated-at-limit')) return `${entry.id} is ${entry.status} in perf/budgets.json`;
  for (const k of budgets?.knownIssues ?? []) {
    if (k.source && k.source !== m.source) continue;
    if (k.idPattern && !new RegExp(k.idPattern).test(m.id)) continue;
    if (!k.source && !k.idPattern) continue;
    return k.reason ?? 'knownIssues in perf/budgets.json';
  }
  return null;
}

export function compareRun(run, baseline, budgets, limitPercent) {
  const limit = (limitPercent ?? budgets?.method?.regressionLimitPercent ?? DEFAULT_LIMIT) / 100;
  const report = {
    schema: 'aoi-perf-compare/1', comparedAt: new Date().toISOString(), limitPercent: limit * 100,
    baseline: baseline ? { file: baseline.file ?? null, runs: baseline.runs ?? [], createdAt: baseline.createdAt ?? null, machineName: baseline.machineName ?? null } : null,
    overBudget: [], knownOverBudget: [], withinBudget: [], regressions: [], improvements: [], knownRegressions: [], unchanged: [], missing: [], newMetrics: [],
    notVerified: [], xfails: [], failedTests: [], warnings: [], verdict: 'PASS', exitCode: 0,
  };
  if (run.injectedDelayMs > 0) report.warnings.push(`this run has an injected delay of ${run.injectedDelayMs} ms (harness self-test): do not use it as a baseline`);
  const env = run.environment?.start ?? run.environment ?? null;
  if (env && typeof env.cpuLoadPercent === 'number' && env.cpuLoadPercent >= 50) report.warnings.push(`system CPU load was ${env.cpuLoadPercent}% when the run started`);
  if (env && typeof env.cpuPerformancePercent === 'number' && env.cpuPerformancePercent < 90)
    report.warnings.push(`the processor ran at ${env.cpuPerformancePercent}% of its nominal speed when the run started (power plan, thermal or other programs): times are not comparable with a run at 100%`);
  if (baseline && run.machine?.machineName && baseline.machineName && run.machine.machineName.toLowerCase() !== baseline.machineName.toLowerCase())
    report.warnings.push(`baseline is from ${baseline.machineName}, this run is from ${run.machine.machineName}: numbers compare only on the same machine`);
  if (baseline && run.machine?.qt && baseline.qt && run.machine.qt !== baseline.qt) report.warnings.push(`baseline Qt ${baseline.qt}, this run Qt ${run.machine.qt}`);

  const byKey = new Map(run.metrics.map((m) => [m.key, m]));
  for (const m of run.metrics) {
    const value = gateValue(m);
    const item = { key: m.key, label: m.label, unit: m.unit, value: round3(value), median: m.median, p95: m.p95, budget: m.budget, budgetId: m.budgetId, direction: m.direction };
    if (m.budget !== null && m.budget !== undefined && value !== null) {
      const over = m.direction === 'min' ? value < m.budget : value > m.budget;
      if (!over) report.withinBudget.push(item);
      else {
        const known = knownReason(m, budgets);
        if (known) report.knownOverBudget.push({ ...item, known });
        else report.overBudget.push(item);
      }
    }
    if (baseline?.metrics) {
      const b = baseline.metrics[m.key];
      if (!b) {
        report.newMetrics.push({ key: m.key });
      } else if (GATED_UNITS.has(m.unit) && typeof m.median === 'number') {
        const [lo, hi] = b.medianRange;
        const minAbs = MIN_ABS[m.unit] ?? 0;
        const slower = m.direction === 'min' ? m.median < lo * (1 - limit) && lo - m.median > minAbs : m.median > hi * (1 + limit) && m.median - hi > minAbs;
        const faster = m.direction === 'min' ? m.median > hi * (1 + limit) && m.median - hi > minAbs : m.median < lo * (1 - limit) && lo - m.median > minAbs;
        const change = m.direction === 'min' ? (m.median - lo) / Math.max(lo, 1e-9) : (m.median - hi) / Math.max(hi, 1e-9);
        const row = { ...item, baselineMedianRange: b.medianRange, baselineP95Range: b.p95Range, changePercent: round3(change * 100) };
        if (slower) {
          const known = knownReason(m, budgets);
          if (known) report.knownRegressions.push({ ...row, known });
          else report.regressions.push(row);
        } else if (faster) report.improvements.push(row);
        else report.unchanged.push(row);
      }
    }
  }
  if (baseline?.metrics) {
    for (const key of Object.keys(baseline.metrics)) if (!byKey.has(key)) report.missing.push({ key, label: baseline.metrics[key].label ?? key });
  }
  for (const t of run.tests ?? []) {
    if (t.status === 'fail') report.failedTests.push({ name: t.name, fails: t.fails });
    if (t.skipped > 0 || t.status === 'incomplete') report.notVerified.push({ name: t.name, skipped: t.skipped, status: t.status, skips: t.skips });
    for (const x of t.xfails) report.xfails.push({ name: t.name, where: x.where, message: x.message });
  }

  const compared = report.regressions.length + report.knownRegressions.length + report.improvements.length + report.unchanged.length;
  const regressed = report.regressions.length + report.knownRegressions.length;
  report.comparedMetrics = compared;
  report.regressedShare = compared ? round3(regressed / compared) : 0;
  if (compared > 0 && regressed / compared >= 0.5) {
    report.verdict = 'INCONCLUSIVE';
    report.exitCode = 3;
    report.warnings.push(`${regressed} of ${compared} compared metrics are slower than the baseline: the whole machine was probably slower (power, load, core); measure again before calling it a regression`);
  } else if (report.overBudget.length || report.regressions.length || report.failedTests.length) {
    report.verdict = 'FAIL';
    report.exitCode = 1;
  }
  return report;
}

function fmt(v, unit) {
  if (v === null || v === undefined) return '—';
  return unit === 'percent' ? `${v}%` : `${v} ${unit}`;
}

function printReport(report, run) {
  const line = (s) => console.log(s);
  line(`== perf gate: ${report.verdict} (exit ${report.exitCode}) — ${run.machine?.machineName ?? '?'} ${run.machine?.buildType ?? ''} commit ${run.machine?.commit ?? '?'}, limit ${report.limitPercent}%`);
  line(`   metrics ${run.metrics.length}, within budget ${report.withinBudget.length}, over budget ${report.overBudget.length} (known ${report.knownOverBudget.length}), ` +
       `baseline: regressions ${report.regressions.length} (known ${report.knownRegressions.length}), improvements ${report.improvements.length}, unchanged ${report.unchanged.length}, missing ${report.missing.length}, new ${report.newMetrics.length}`);
  for (const w of report.warnings) line(`   WARNING: ${w}`);
  const section = (title, rows, f) => {
    if (!rows.length) return;
    line(`-- ${title} (${rows.length})`);
    for (const r of rows) line(`   ${f(r)}`);
  };
  section('OVER BUDGET', report.overBudget, (r) => `${r.key}: ${fmt(r.value, r.unit)} vs ${fmt(r.budget, r.unit)} (${r.budgetId ?? ''})`);
  section('over budget, known (listed, not failing)', report.knownOverBudget, (r) => `${r.key}: ${fmt(r.value, r.unit)} vs ${fmt(r.budget, r.unit)} — ${r.known}`);
  section('REGRESSIONS vs baseline', report.regressions, (r) => `${r.key}: median ${fmt(r.median, r.unit)} vs baseline [${r.baselineMedianRange.join(', ')}] (${r.changePercent > 0 ? '+' : ''}${r.changePercent}%)`);
  section('regressions, known (listed, not failing)', report.knownRegressions, (r) => `${r.key}: median ${fmt(r.median, r.unit)} vs [${r.baselineMedianRange.join(', ')}] — ${r.known}`);
  section('improvements vs baseline', report.improvements, (r) => `${r.key}: median ${fmt(r.median, r.unit)} vs [${r.baselineMedianRange.join(', ')}] (${r.changePercent}%)`);
  section('missing (in the baseline, not in this run: skipped back end? renamed?)', report.missing, (r) => r.key);
  section('FAILED TESTS', report.failedTests, (r) => `${r.name}: ${r.fails.map((f) => f.where).join(', ')}`);
  section('NOT VERIFIED (skipped items: a skip is not a pass)', report.notVerified, (r) => `${r.name}: ${r.status}, ${r.skipped ?? '?'} skipped${r.skips.length ? ' — ' + r.skips.map((s) => `${s.where} ${s.reason}`).join('; ') : ''}`);
  section('XFAIL (known, to be fixed; XPASS would be a failure)', report.xfails, (r) => `${r.name}: ${r.where} ${r.message}`);
}

function loadBaseline(path) {
  if (!path || !existsSync(path)) return null;
  const b = readJson(path);
  b.file = path;
  return b;
}

function defaultBaselinePath(run) {
  const name = (run.machine?.machineName ?? hostname()).toLowerCase();
  return join(REPO_ROOT, 'perf', 'baseline', `${name}-harness.json`);
}

function cmdCompare(args) {
  const runPath = args._[1];
  if (!runPath || !existsSync(runPath)) return fail(2, 'compare needs <run.json>');
  const run = readJson(runPath);
  const budgets = readJson(resolve(REPO_ROOT, args.flags.budgets ?? 'perf/budgets.json'));
  const baselinePath = args.flags.baseline ?? defaultBaselinePath(run);
  const baseline = loadBaseline(baselinePath);
  if (!baseline) console.log(`(no baseline at ${baselinePath}: only the budgets are checked; make one with 'baseline')`);
  const report = compareRun(run, baseline, budgets, args.flags.limit ? Number(args.flags.limit) : null);
  printReport(report, run);
  if (args.flags.json) writeFileSync(resolve(args.flags.json), JSON.stringify(report, null, 1));
  if (args.flags.html) writeHtml(run, report, baseline, args.flags.template, args.flags.html);
  return report.exitCode;
}

// ---------------------------------------------------------------------------------------------
// baseline、noise
// ---------------------------------------------------------------------------------------------
export function buildBaseline(runs, { force = false, limitPercent = DEFAULT_LIMIT } = {}) {
  if (!runs.length) throw new Error('baseline needs at least one run');
  for (const r of runs) if ((r.injectedDelayMs ?? 0) > 0) throw new Error(`run ${r.file ?? ''} has an injected delay (${r.injectedDelayMs} ms): it is a harness self-test, not a baseline`);
  const first = runs[0];
  for (const r of runs.slice(1)) {
    const same = (a, b) => (a ?? '').toString().toLowerCase() === (b ?? '').toString().toLowerCase();
    if (!force && (!same(r.machine?.machineName, first.machine?.machineName) || !same(r.machine?.buildType, first.machine?.buildType)))
      throw new Error(`runs come from different machines or build types (${r.machine?.machineName} ${r.machine?.buildType} vs ${first.machine?.machineName} ${first.machine?.buildType}); use --force to merge them anyway`);
  }
  const metrics = {};
  for (const r of runs) {
    for (const m of r.metrics) {
      if (typeof m.median !== 'number') continue;
      const e = metrics[m.key] ?? (metrics[m.key] = { label: m.label, unit: m.unit, direction: m.direction, medians: [], p95s: [] });
      e.medians.push(m.median);
      e.p95s.push(m.p95 ?? m.median);
    }
  }
  for (const e of Object.values(metrics)) {
    e.n = e.medians.length;
    e.medianRange = [round3(Math.min(...e.medians)), round3(Math.max(...e.medians))];
    e.p95Range = [round3(Math.min(...e.p95s)), round3(Math.max(...e.p95s))];
    e.medians = e.medians.map(round3);
    e.p95s = e.p95s.map(round3);
  }
  return {
    schema: 'aoi-perf-baseline/1', createdAt: new Date().toISOString(), limitPercent,
    machineName: first.machine?.machineName ?? null, buildType: first.machine?.buildType ?? null, qt: first.machine?.qt ?? null,
    commit: first.machine?.commit ?? null, machine: first.machine ?? null,
    runs: runs.map((r) => r.file ?? r.collectedAt ?? null),
    note: `Regression gate baseline (S10, ADR-023). Numbers compare only on this machine. medianRange and p95Range are the spread over ${runs.length} run(s) of the same build; the gate flags a metric whose median is more than ${limitPercent}% above the top of medianRange.`,
    metrics,
  };
}

function cmdBaseline(args) {
  const paths = args._.slice(1);
  const out = args.flags.out;
  if (!paths.length || !out) return fail(2, 'baseline needs <run.json>... --out <baseline.json>');
  const budgets = readJson(resolve(REPO_ROOT, args.flags.budgets ?? 'perf/budgets.json'));
  const runs = paths.map((p) => ({ ...readJson(p), file: basename(p) }));
  let baseline;
  try {
    baseline = buildBaseline(runs, { force: Boolean(args.flags.force), limitPercent: budgets?.method?.regressionLimitPercent ?? DEFAULT_LIMIT });
  } catch (e) {
    return fail(2, e.message);
  }
  mkdirSync(dirname(resolve(out)), { recursive: true });
  writeFileSync(resolve(out), JSON.stringify(baseline, null, 1));
  console.log(`baseline with ${Object.keys(baseline.metrics).length} metrics from ${runs.length} run(s) → ${out}`);
  return 0;
}

export function noiseBetween(runs, limitPercent = DEFAULT_LIMIT) {
  const byKey = new Map();
  for (const r of runs) for (const m of r.metrics) {
    if (!GATED_UNITS.has(m.unit) || typeof m.median !== 'number') continue;
    const e = byKey.get(m.key) ?? { key: m.key, unit: m.unit, medians: [] };
    e.medians.push(m.median);
    byKey.set(m.key, e);
  }
  const rows = [];
  for (const e of byKey.values()) {
    if (e.medians.length < 2) continue;
    const lo = Math.min(...e.medians);
    const hi = Math.max(...e.medians);
    const minAbs = MIN_ABS[e.unit] ?? 0;
    rows.push({ key: e.key, unit: e.unit, medians: e.medians.map(round3), ratio: round3(hi / Math.max(lo, 1e-9)), spreadPercent: round3((hi - lo) / Math.max(lo, 1e-9) * 100), small: hi - lo <= minAbs });
  }
  rows.sort((a, b) => b.ratio - a.ratio);
  const spreads = rows.filter((r) => !r.small).map((r) => r.spreadPercent).sort((a, b) => a - b);
  const pct = (p) => (spreads.length ? spreads[Math.min(spreads.length - 1, Math.max(0, Math.ceil(p * spreads.length) - 1))] : 0);
  return {
    runs: runs.length, metrics: rows.length, spreadMedianPercent: pct(0.5), spreadP95Percent: pct(0.95),
    overLimit: rows.filter((r) => !r.small && r.spreadPercent > limitPercent), rows,
  };
}

function cmdNoise(args) {
  const paths = args._.slice(1);
  if (paths.length < 2) return fail(2, 'noise needs at least two <run.json>');
  const budgets = readJson(resolve(REPO_ROOT, args.flags.budgets ?? 'perf/budgets.json'));
  const limit = budgets?.method?.regressionLimitPercent ?? DEFAULT_LIMIT;
  const n = noiseBetween(paths.map((p) => readJson(p)), limit);
  console.log(`== noise over ${n.runs} runs, ${n.metrics} metrics: spread of medians median ${n.spreadMedianPercent}%, p95 ${n.spreadP95Percent}%; ${n.overLimit.length} metrics spread more than ${limit}%`);
  for (const r of n.overLimit.slice(0, 40)) console.log(`   ${r.key}: medians [${r.medians.join(', ')}] ${r.unit}  spread ${r.spreadPercent}%`);
  if (n.overLimit.length > 40) console.log(`   … ${n.overLimit.length - 40} more`);
  if (args.flags.json) writeFileSync(resolve(args.flags.json), JSON.stringify(n, null, 1));
  return 0;
}

// ---------------------------------------------------------------------------------------------
// html：把資料嵌進 explainer.html
// ---------------------------------------------------------------------------------------------
export function injectHtml(template, payload) {
  const begin = template.indexOf('<script type="application/json" id="aoi-perf-run">');
  const end = template.indexOf('</script>', begin);
  if (begin < 0 || end < 0) throw new Error('the template has no <script type="application/json" id="aoi-perf-run"> block');
  const open = template.indexOf('>', begin) + 1;
  const json = JSON.stringify(payload).replace(/<\//g, '<\\/');
  return template.slice(0, open) + '\n' + json + '\n' + template.slice(end);
}

function writeHtml(run, report, baseline, templatePath, outPath) {
  const template = readFileSync(resolve(REPO_ROOT, templatePath ?? 'docs/steps/S10-perf-harness/explainer.html'), 'utf8');
  const payload = { run, report, baseline: baseline ? { metrics: baseline.metrics, runs: baseline.runs, createdAt: baseline.createdAt, machineName: baseline.machineName, commit: baseline.commit } : null };
  mkdirSync(dirname(resolve(outPath)), { recursive: true });
  writeFileSync(resolve(outPath), injectHtml(template, payload));
  console.log(`report → ${outPath}`);
}

function cmdHtml(args) {
  const runPath = args._[1];
  const out = args.flags.out;
  if (!runPath || !existsSync(runPath) || !out) return fail(2, 'html needs <run.json> --out <file.html>');
  const run = readJson(runPath);
  const budgets = readJson(resolve(REPO_ROOT, args.flags.budgets ?? 'perf/budgets.json'));
  const baseline = loadBaseline(args.flags.baseline ?? defaultBaselinePath(run));
  const report = compareRun(run, baseline, budgets, args.flags.limit ? Number(args.flags.limit) : null);
  writeHtml(run, report, baseline, args.flags.template, out);
  return 0;
}

// ---------------------------------------------------------------------------------------------
// self-test：閘門的規則用造的資料驗證（不碰 perf/results）
// ---------------------------------------------------------------------------------------------
function selfTest() {
  let checks = 0;
  const assert = (cond, what) => {
    ++checks;
    if (!cond) throw new Error(`self-test failed: ${what}`);
  };
  const budgets = {
    method: { regressionLimitPercent: 15 },
    budgets: [
      { id: 'T5', limit: 300, status: 'proposed' },
      { id: 'U3', limit: 16, status: 'measured-over-limit' },
      { id: 'T6a', limit: 99, direction: 'min', status: 'calibrated' },
    ],
    knownIssues: [{ source: 'ab', idPattern: '^halcon:lowContrast:', reason: 'ADR-004 §5 B1' }],
  };
  const metric = (key, median, extra = {}) => {
    const [source, id] = key.split('/');
    return { key, source, id, label: id, unit: 'ms', n: 10, median, p95: median * 1.1, budget: null, budgetId: null, direction: 'max', ...extra };
  };
  const machine = { machineName: 'TestBox', buildType: 'Release', qt: '6.12.0', commit: 'abc' };
  const runA = { machine, injectedDelayMs: 0, tests: [], metrics: [metric('e2e/T5:opencv', 100, { budget: 300, budgetId: 'T5' }), metric('opencv/T2a', 60), metric('lattice/T3', 0.8), metric('ui-s04/U3', 40, { budget: 16, budgetId: 'U3' })] };
  const runB = { machine, injectedDelayMs: 0, tests: [], metrics: [metric('e2e/T5:opencv', 110, { budget: 300, budgetId: 'T5' }), metric('opencv/T2a', 64), metric('lattice/T3', 0.9), metric('ui-s04/U3', 44, { budget: 16, budgetId: 'U3' })] };
  const baseline = buildBaseline([runA, runB]);
  assert(baseline.metrics['opencv/T2a'].medianRange[0] === 60 && baseline.metrics['opencv/T2a'].medianRange[1] === 64, 'baseline keeps the range of medians');

  // 1. 和基準線一樣：通過；U3 超標但已知 → 列出、不擋。
  let r = compareRun(runB, baseline, budgets);
  assert(r.verdict === 'PASS' && r.exitCode === 0, `identical run passes (got ${r.verdict})`);
  assert(r.knownOverBudget.length === 1 && r.knownOverBudget[0].key === 'ui-s04/U3' && r.overBudget.length === 0, 'U3 is listed as known over budget, not failing');

  // 2. 一個指標慢 50%：失敗，列出那一個。
  const slow = { ...runB, metrics: runB.metrics.map((m) => (m.key === 'opencv/T2a' ? { ...m, median: 96, p95: 105 } : m)) };
  r = compareRun(slow, baseline, budgets);
  assert(r.verdict === 'FAIL' && r.regressions.length === 1 && r.regressions[0].key === 'opencv/T2a', 'a 50% slower metric is a regression');

  // 3. 慢 10%：在 15% 以內，通過。
  const slight = { ...runB, metrics: runB.metrics.map((m) => (m.key === 'opencv/T2a' ? { ...m, median: 70 } : m)) };
  r = compareRun(slight, baseline, budgets);
  assert(r.verdict === 'PASS' && r.regressions.length === 0, '10% above the top of the range is inside the limit');

  // 4. 微秒等級的指標：相對差很大、絕對差很小 → 不算回歸。
  const tiny = { ...runB, metrics: runB.metrics.map((m) => (m.key === 'lattice/T3' ? { ...m, median: 0.93 } : m)) };
  r = compareRun(tiny, baseline, budgets);
  assert(r.regressions.length === 0, 'a 0.03 ms change is below the absolute floor');

  // 5. 超出預算而且不是已知的：失敗。
  const over = { ...runB, metrics: runB.metrics.map((m) => (m.key === 'e2e/T5:opencv' ? { ...m, median: 310, p95: 340 } : m)) };
  r = compareRun(over, baseline, budgets);
  assert(r.verdict === 'FAIL' && r.overBudget.length === 1 && r.overBudget[0].key === 'e2e/T5:opencv', 'T5 over 300 ms fails');

  // 6. 一半以上都變慢：無法判定（整台機器變慢），結束碼 3。
  const all = { ...runB, metrics: runB.metrics.map((m) => ({ ...m, median: m.median * 2, p95: m.p95 * 2 })) };
  r = compareRun(all, baseline, budgets);
  assert(r.verdict === 'INCONCLUSIVE' && r.exitCode === 3, `everything slower is inconclusive (got ${r.verdict})`);

  // 7. 越大越好的指標（召回率）：變小才是回歸；knownIssues 的樣式讓 HALCON 低對比不擋。
  const recall = (key, v, extra = {}) => ({ ...metric(key, v, { unit: 'percent', direction: 'min', budget: 99, budgetId: 'T6a', min: v, p95: v }), ...extra });
  const base2 = buildBaseline([{ machine, injectedDelayMs: 0, tests: [], metrics: [recall('ab/opencv:clean:recall', 100), recall('ab/halcon:lowContrast:recall', 0)] }]);
  r = compareRun({ machine, injectedDelayMs: 0, tests: [], metrics: [recall('ab/opencv:clean:recall', 100), recall('ab/halcon:lowContrast:recall', 0)] }, base2, budgets);
  assert(r.overBudget.length === 0 && r.knownOverBudget.length === 1 && /B1/.test(r.knownOverBudget[0].known), 'HALCON low contrast recall is a known issue');
  r = compareRun({ machine, injectedDelayMs: 0, tests: [], metrics: [recall('ab/opencv:clean:recall', 80), recall('ab/halcon:lowContrast:recall', 0)] }, base2, budgets);
  assert(r.overBudget.length === 1 && r.regressions.length === 1, 'OpenCV recall dropping to 80% is over budget and a regression');

  // 8. log：跳過不是通過；XFAIL 列出來；XPASS 是失敗。
  const log = parseQtTestLog('tst_x', [
    '********* Start testing of X *********',
    'PASS   : X::initTestCase()',
    'SKIP   : X::f(halcon) HALCON is not usable here: no license',
    'SKIP   : X::initTestCase() AOI_REAL_IMAGE_DIR is not set',
    'XFAIL  : X::g(B1 low contrast|halcon) all parts found',
    'PASS   : X::g(B1 low contrast|halcon)',
    'Totals: 2 passed, 0 failed, 2 skipped, 0 blacklisted, 10ms',
  ].join('\n'));
  assert(log.status === 'pass-with-skips' && log.skips.length === 2 && log.skips[0].where === 'f(halcon)' && log.skips[1].where === 'initTestCase()' && log.skips[1].reason.startsWith('AOI_REAL'), 'a skip is parsed and is not a pass');
  assert(log.xfails.length === 1 && log.xfails[0].where === 'g(B1 low contrast|halcon)', 'an XFAIL is parsed');
  const xpass = parseQtTestLog('tst_y', 'XPASS  : Y::g(B3|halcon) fixed\nTotals: 1 passed, 1 failed, 0 skipped, 0 blacklisted, 1ms');
  assert(xpass.status === 'fail' && xpass.fails.length === 1, 'an XPASS is a failure');
  r = compareRun({ ...runB, tests: [log, xpass] }, baseline, budgets);
  assert(r.notVerified.length === 1 && r.xfails.length === 1 && r.failedTests.length === 1 && r.verdict === 'FAIL', 'skips are listed as not verified, XFAIL as known, a failed test fails the gate');

  // 9. 注入延遲的結果不能當基準線；缺少的指標列出來。
  let threw = false;
  try {
    buildBaseline([{ ...runB, injectedDelayMs: 50 }]);
  } catch {
    threw = true;
  }
  assert(threw, 'a run with an injected delay is refused as a baseline');
  r = compareRun({ ...runB, metrics: runB.metrics.filter((m) => m.key !== 'opencv/T2a') }, baseline, budgets);
  assert(r.missing.length === 1 && r.missing[0].key === 'opencv/T2a', 'a metric in the baseline but not in the run is listed as missing');

  // 10. perf_ab 的 runs[] 正規化；噪音的計算；HTML 的嵌入。
  const ab = normalizeResult('ab_20261013-120000.json', { kind: 'perf_ab', runs: [{ scenario: 'clean', label: 'clean', backend: 'opencv', n: 5, totalMedianMs: 70, totalP95Ms: 75, recallPercent: 100, found: 10, present: 10, falseFound: 0, errorP95Px: 0.1, errorMaxPx: 0.3, peakMb: 50 }] }, budgets);
  assert(ab.source === 'ab' && ab.metrics.length === 4 && ab.metrics[1].budget === 99 && ab.metrics[1].direction === 'min', 'perf_ab rows become four metrics with the T6a budget');
  const noise = noiseBetween([runA, runB], 15);
  assert(noise.metrics === 4 && noise.overLimit.length === 0 && noise.spreadP95Percent <= 15, 'noise between two close runs is inside the limit');
  const cvT6a = normalizeResult('halcon_20261013-120000.json', { metrics: [{ id: 'CV-T6a', label: 'recall', unit: 'percent', n: 1, median: 100, p05: 100, budget: 99, pass: true }] }, budgets);
  assert(cvT6a.metrics[0].direction === 'min' && compareRun({ machine, injectedDelayMs: 0, tests: [], metrics: cvT6a.metrics }, null, budgets).overBudget.length === 0, 'a p05-only metric is higher-is-better and 100% is not over a 99% budget');
  const html = injectHtml('<html><script type="application/json" id="aoi-perf-run">{}</script></html>', { a: '</script>' });
  assert(html.includes('<\\/script>') && html.includes('"a":'), 'the payload is embedded with </ escaped');

  console.log(`perf-compare self-test: ${checks} checks passed`);
  return 0;
}

// ---------------------------------------------------------------------------------------------
function fail(code, message) {
  console.error(message);
  return code;
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  if (args.flags['self-test']) return selfTest();
  const cmd = args._[0];
  if (cmd === 'collect') return cmdCollect(args);
  if (cmd === 'compare') return cmdCompare(args);
  if (cmd === 'baseline') return cmdBaseline(args);
  if (cmd === 'noise') return cmdNoise(args);
  if (cmd === 'html') return cmdHtml(args);
  console.error('usage: perf-compare.mjs collect|compare|baseline|noise|html … | --self-test (see the header of this file)');
  return 2;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    process.exitCode = main();
  } catch (e) {
    console.error(e.stack ?? String(e));
    process.exitCode = 2;
  }
}
