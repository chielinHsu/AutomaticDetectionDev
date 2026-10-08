#!/usr/bin/env node
// 模型與推理強度路由的實測（MODEL_ROUTING.md 第 8 節）。會呼叫 Claude、花額度：沒有 --yes 只印計畫。
//
// 用法：
//   node scripts/ai/bench-model-routing.mjs verify [--yes]
//     開一個 headless session（主 session：haiku、effort low），同時委派五個 subagent 各回一句話，
//     從 subagent 的 session 紀錄讀出實際的 model 與 effort，和 frontmatter 比對；
//     不同就報「降級」，結束碼 1。
//   node scripts/ai/bench-model-routing.mjs bench [--yes] [--agents a,b] [--tasks t1,t2] [--repeat N]
//     同一組任務讓每個 agent 各跑一次：claude -p --agent <agent> --effort <frontmatter 的 effort>
//     （實測 --agent 只套用 agent 的 model，不套用 effort，所以明確傳 --effort，再從紀錄確認）。
//     記錄正確與否、時間、tokens、Claude Code 回報的費用、實際的 model 與 effort、是否建議升級。
//
// 結果寫到 perf/results/model-routing-<時間>.json（不入版控），表格印在 stdout。
// 數字只取 Claude Code 回報的值（-p 的 result、stream-json、session 紀錄）；拿不到的寫 null，不估算。
// 跑的時候關掉 hook（AOI_MODEL_ROUTER=off），避免路由提示混進被量測的 session。
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { homedir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const AGENT_NAMES = ['fast-worker', 'developer', 'developer-deep', 'architect', 'architect-deep'];
const EFFORT_ORDER = ['low', 'medium', 'high', 'xhigh', 'max'];
const CLAUDE = process.env.AOI_CLAUDE_BIN || 'claude';
const RUN_TIMEOUT_MS = 15 * 60 * 1000;

// 同一組任務：答案固定，可以自動判斷對錯。
const TASKS = {
  lookup: {
    label: '查詢（容易）',
    prompt: '本專案的座標與角度慣例定義在哪一個原始碼檔案？只回答一個相對路徑，不要其他文字。',
    correct: (text) => /src[\\/]domain[\\/]geometry\.h/.test(text),
  },
  code: {
    label: '讀程式推理（中等）',
    prompt: '讀 .claude/hooks/model_router.mjs，回答：需求「列出所有用到 JobRunner 的檔案」會被建議給哪一個 agent？只回答 agent 名稱，不要其他文字。',
    correct: (text) => /\bfast-worker\b/.test(text) && !/\b(developer|architect)/.test(text),
  },
  math: {
    label: '純推理（困難）',
    prompt: '不要使用任何工具。求所有使 n² + 85n + 2017 成為完全平方數的正整數 n 的總和。只回答一個整數，不要其他文字。',
    correct: (text) => /^\D*195\D*$/.test(text.trim()),
  },
};

function parseArgs(argv) {
  const args = { command: argv[0], yes: false, agents: AGENT_NAMES, tasks: Object.keys(TASKS), repeat: 1 };
  for (let i = 1; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === '--yes') args.yes = true;
    else if (arg === '--agents') args.agents = argv[++i].split(',').map((s) => s.trim()).filter(Boolean);
    else if (arg === '--tasks') args.tasks = argv[++i].split(',').map((s) => s.trim()).filter(Boolean);
    else if (arg === '--repeat') args.repeat = Math.max(1, Number.parseInt(argv[++i], 10) || 1);
    else throw new Error(`不認得的參數：${arg}`);
  }
  return args;
}

function frontmatter(path) {
  const block = readFileSync(path, 'utf8').replace(/^﻿/, '').match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n/);
  const fields = {};
  for (const line of block?.[1].split(/\r?\n/) ?? []) {
    const pair = line.match(/^([A-Za-z]+):\s*(.*)$/);
    if (pair) fields[pair[1]] = pair[2].trim();
  }
  return fields;
}

function agentConfigs(names) {
  return names.map((name) => {
    const path = join(REPO_ROOT, '.claude', 'agents', `${name}.md`);
    if (!existsSync(path)) throw new Error(`找不到 .claude/agents/${name}.md`);
    const fields = frontmatter(path);
    return { name, model: fields.model, effort: fields.effort };
  });
}

function runClaude(args) {
  const env = { ...process.env, AOI_MODEL_ROUTER: 'off' };
  delete env.CLAUDECODE;
  const started = process.hrtime.bigint();
  const run = spawnSync(CLAUDE, args, {
    cwd: REPO_ROOT,
    env,
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
    maxBuffer: 256 * 1024 * 1024,
    timeout: RUN_TIMEOUT_MS,
  });
  const wallMs = Math.round(Number(process.hrtime.bigint() - started) / 1e6);
  return { status: run.status, stdout: run.stdout ?? '', stderr: run.stderr ?? '', error: run.error, wallMs };
}

function jsonLines(text) {
  return text.replace(/^﻿/, '').split(/\r?\n/).filter(Boolean).map((line) => {
    try {
      return JSON.parse(line);
    } catch {
      return null;
    }
  }).filter(Boolean);
}

// session 紀錄：~/.claude/projects/<cwd 換成 - 的名稱>/<session>.jsonl；找不到就掃全部的專案資料夾。
function sessionDir(sessionId) {
  const base = join(homedir(), '.claude', 'projects');
  const direct = join(base, REPO_ROOT.replace(/[^A-Za-z0-9]/g, '-'));
  if (existsSync(join(direct, `${sessionId}.jsonl`))) return direct;
  if (!existsSync(base)) return null;
  for (const name of readdirSync(base)) {
    if (existsSync(join(base, name, `${sessionId}.jsonl`))) return join(base, name);
  }
  return null;
}

// 每一筆 assistant 紀錄有實際送出的 model 與 effort（模型不支援 effort 時沒有這個欄位）。
function effective(file) {
  if (!file || !existsSync(file)) return { models: [], efforts: [], found: false };
  const rows = jsonLines(readFileSync(file, 'utf8')).filter((row) => row.type === 'assistant' && row.message?.model && row.message.model !== '<synthetic>');
  return {
    models: [...new Set(rows.map((row) => row.message.model))],
    efforts: [...new Set(rows.map((row) => row.effort ?? null))],
    found: true,
  };
}

function judge(config, actual) {
  if (!actual.found || actual.models.length === 0) return { ok: false, verdict: '沒有找到執行紀錄' };
  const family = config.model;
  if (!actual.models.every((model) => model.includes(family))) {
    return { ok: false, verdict: `模型不符：設定 ${config.model}，實際 ${actual.models.join('、')}` };
  }
  if (actual.efforts.length === 1 && actual.efforts[0] === config.effort) return { ok: true, verdict: '符合' };
  if (actual.efforts.every((effort) => effort === null)) {
    return { ok: false, verdict: `降級：設定 effort ${config.effort}，但 ${actual.models.join('、')} 不支援 effort，沒有送出` };
  }
  const shown = actual.efforts.map((effort) => effort ?? '沒有送出').join('、');
  const lower = actual.efforts.some((effort) => EFFORT_ORDER.indexOf(effort) < EFFORT_ORDER.indexOf(config.effort));
  return { ok: false, verdict: `${lower ? '降級' : '不符'}：設定 effort ${config.effort}，實際 ${shown}` };
}

function claudeVersion() {
  const run = spawnSync(CLAUDE, ['--version'], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
  return (run.stdout ?? '').trim() || null;
}

function saveResults(kind, data) {
  const dir = join(REPO_ROOT, 'perf', 'results');
  mkdirSync(dir, { recursive: true });
  const stamp = new Date().toISOString().replace(/[:.]/g, '-');
  const file = join(dir, `model-routing-${kind}-${stamp}.json`);
  writeFileSync(file, `${JSON.stringify(data, null, 2)}\n`);
  return file;
}

const fmt = (value, digits = 0) => (value === null || value === undefined ? 'n/a' : Number(value).toFixed(digits));

// ---- verify --------------------------------------------------------------------------------
function verify(args) {
  const configs = agentConfigs(args.agents);
  const prompt = `請在同一則回覆裡用 Agent 工具同時委派 ${configs.length} 次（不要傳 model 或 effort 參數），subagent_type 依序是：${configs.map((c) => c.name).join('、')}；prompt 都是「不要使用任何工具。只回一行：你的 subagent 名稱 OK」。全部完成後，逐一列出每個 subagent 的回覆原文。`;
  const claudeArgs = ['-p', prompt, '--model', 'haiku', '--effort', 'low', '--max-turns', '8', '--output-format', 'stream-json', '--verbose'];
  console.log(`verify：委派 ${configs.length} 個 subagent（${configs.map((c) => `${c.name}=${c.model}/${c.effort}`).join('、')}）。`);
  if (!args.yes) {
    console.log('這會呼叫 Claude、花額度（每個 subagent 啟動時載入專案文件，約數萬 tokens）。確定要跑就加 --yes。');
    return 0;
  }
  if (process.env.CLAUDE_CODE_EFFORT_LEVEL) console.log(`注意：CLAUDE_CODE_EFFORT_LEVEL=${process.env.CLAUDE_CODE_EFFORT_LEVEL} 優先於 subagent 的 effort。`);

  const run = runClaude(claudeArgs);
  const events = jsonLines(run.stdout);
  const sessionId = events.find((event) => event.session_id)?.session_id ?? null;
  const tasks = new Map();
  for (const event of events) {
    if (event.type === 'system' && event.subtype === 'task_started') tasks.set(event.task_id, { agent: event.subagent_type });
    if (event.type === 'system' && event.subtype === 'task_notification' && tasks.has(event.task_id)) {
      Object.assign(tasks.get(event.task_id), { status: event.status, usage: event.usage ?? null });
    }
  }
  const result = events.filter((event) => event.type === 'result').at(-1) ?? null;
  const dir = sessionId ? sessionDir(sessionId) : null;

  const rows = configs.map((config) => {
    const [taskId, task] = [...tasks.entries()].find(([, value]) => value.agent === config.name) ?? [null, null];
    const actual = effective(dir && taskId ? join(dir, sessionId, 'subagents', `agent-${taskId}.jsonl`) : null);
    const { ok, verdict } = judge(config, actual);
    return {
      agent: config.name,
      configured: { model: config.model, effort: config.effort },
      actual: { models: actual.models, efforts: actual.efforts },
      status: task?.status ?? null,
      total_tokens: task?.usage?.total_tokens ?? null,
      duration_ms: task?.usage?.duration_ms ?? null,
      ok,
      verdict,
    };
  });

  console.log('\n| agent | 設定 | 實際 model | 實際 effort | tokens | 時間 (ms) | 結果 |');
  console.log('|---|---|---|---|---|---|---|');
  for (const row of rows) {
    console.log(`| ${row.agent} | ${row.configured.model}/${row.configured.effort} | ${row.actual.models.join(', ') || 'n/a'} | ${row.actual.efforts.map((e) => e ?? '沒有送出').join(', ') || 'n/a'} | ${fmt(row.total_tokens)} | ${fmt(row.duration_ms)} | ${row.verdict} |`);
  }
  const file = saveResults('verify', {
    kind: 'verify',
    at: new Date().toISOString(),
    claude_code_version: claudeVersion(),
    session_id: sessionId,
    exit_status: run.status,
    total_cost_usd: result?.total_cost_usd ?? null,
    duration_ms: result?.duration_ms ?? null,
    rows,
  });
  console.log(`\n整個 session：Claude Code 回報 total_cost_usd ${fmt(result?.total_cost_usd, 4)}，${fmt(result?.duration_ms)} ms。結果：${file}`);
  const bad = rows.filter((row) => !row.ok);
  console.log(bad.length === 0 ? 'RESULT: OK（五個 subagent 的 model 與 effort 都照設定套用）' : `RESULT: FAILED（${bad.length} 個不符或降級）`);
  return bad.length === 0 ? 0 : 1;
}

// ---- bench ---------------------------------------------------------------------------------
function bench(args) {
  const configs = agentConfigs(args.agents);
  const taskIds = args.tasks;
  for (const id of taskIds) if (!TASKS[id]) throw new Error(`沒有這個任務：${id}（可用：${Object.keys(TASKS).join('、')}）`);
  const total = configs.length * taskIds.length * args.repeat;
  console.log(`bench：${taskIds.length} 個任務 × ${configs.length} 個 agent × ${args.repeat} 次 ＝ ${total} 個 headless session。`);
  if (!args.yes) {
    console.log('這會呼叫 Claude、花額度（每個 session 載入專案文件；Opus 的 xhigh 會用較多思考 tokens）。確定要跑就加 --yes。');
    return 0;
  }

  const runs = [];
  for (const id of taskIds) {
    const task = TASKS[id];
    for (const config of configs) {
      for (let attempt = 1; attempt <= args.repeat; attempt++) {
        const run = runClaude(['-p', task.prompt, '--agent', config.name, '--effort', config.effort, '--max-turns', '8', '--output-format', 'json']);
        let result = null;
        try {
          result = JSON.parse(run.stdout.replace(/^﻿/, ''));
        } catch {
          result = null;
        }
        const sessionId = result?.session_id ?? null;
        const dir = sessionId ? sessionDir(sessionId) : null;
        const actual = effective(dir ? join(dir, `${sessionId}.jsonl`) : null);
        const text = typeof result?.result === 'string' ? result.result : '';
        const usage = result?.usage ?? {};
        const record = {
          task: id,
          agent: config.name,
          attempt,
          configured: { model: config.model, effort: config.effort },
          actual: { models: actual.models, efforts: actual.efforts },
          effort_check: judge(config, actual).verdict,
          correct: result && !result.is_error ? task.correct(text) : false,
          answer: text.slice(0, 200),
          escalation_requested: /建議升級/.test(text),
          is_error: result?.is_error ?? null,
          exit_status: run.status,
          wall_ms: run.wallMs,
          duration_ms: result?.duration_ms ?? null,
          duration_api_ms: result?.duration_api_ms ?? null,
          num_turns: result?.num_turns ?? null,
          input_tokens: usage.input_tokens ?? null,
          output_tokens: usage.output_tokens ?? null,
          thinking_tokens: usage.output_tokens_details?.thinking_tokens ?? null,
          cache_creation_input_tokens: usage.cache_creation_input_tokens ?? null,
          cache_read_input_tokens: usage.cache_read_input_tokens ?? null,
          total_cost_usd: result?.total_cost_usd ?? null,
        };
        runs.push(record);
        console.log(`${id} × ${config.name}（${attempt}/${args.repeat}）：${record.correct ? '對' : '錯'}，${fmt(record.duration_ms)} ms，output ${fmt(record.output_tokens)}（思考 ${fmt(record.thinking_tokens)}），US$${fmt(record.total_cost_usd, 4)}，${record.effort_check}`);
      }
    }
  }

  console.log('\n| 任務 | agent | model/effort（實際） | 正確 | 時間 (ms) | output tokens | 思考 tokens | 費用 (US$) | 建議升級 |');
  console.log('|---|---|---|---|---|---|---|---|---|');
  for (const r of runs) {
    console.log(`| ${TASKS[r.task].label} | ${r.agent} | ${r.actual.models.join(', ') || 'n/a'} / ${r.actual.efforts.map((e) => e ?? '沒有送出').join(', ') || 'n/a'} | ${r.correct ? '對' : '錯'} | ${fmt(r.duration_ms)} | ${fmt(r.output_tokens)} | ${fmt(r.thinking_tokens)} | ${fmt(r.total_cost_usd, 4)} | ${r.escalation_requested ? '是' : '否'} |`);
  }
  console.log('\n| agent | 正確 | 時間合計 (ms) | output tokens 合計 | 費用合計 (US$) |');
  console.log('|---|---|---|---|---|');
  for (const config of configs) {
    const mine = runs.filter((r) => r.agent === config.name);
    const sum = (key) => (mine.every((r) => r[key] === null) ? null : mine.reduce((acc, r) => acc + (r[key] ?? 0), 0));
    console.log(`| ${config.name} | ${mine.filter((r) => r.correct).length}/${mine.length} | ${fmt(sum('duration_ms'))} | ${fmt(sum('output_tokens'))} | ${fmt(sum('total_cost_usd'), 4)} |`);
  }
  const file = saveResults('bench', { kind: 'bench', at: new Date().toISOString(), claude_code_version: claudeVersion(), repeat: args.repeat, runs });
  console.log(`\n結果：${file}`);
  const mismatched = runs.filter((r) => r.effort_check !== '符合');
  if (mismatched.length > 0) console.log(`注意：${mismatched.length} 次的實際 model 或 effort 和設定不符（見 effort_check）。`);
  return 0;
}

try {
  const args = parseArgs(process.argv.slice(2));
  if (args.command === 'verify') process.exitCode = verify(args);
  else if (args.command === 'bench') process.exitCode = bench(args);
  else {
    console.log('用法：node scripts/ai/bench-model-routing.mjs verify|bench [--yes] [--agents a,b] [--tasks t1,t2] [--repeat N]');
    process.exitCode = 2;
  }
} catch (error) {
  console.error(`錯誤：${error.message}`);
  process.exitCode = 2;
}
