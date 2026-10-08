#!/usr/bin/env node
// 模型與推理強度路由的設定檢查（MODEL_ROUTING.md 第 8 節）。結束碼 0 ＝ 全部通過，1 ＝ 有失敗。
// 不呼叫 Claude、不花額度；Claude Code 實際套用的模型與 effort 用 scripts/ai/bench-model-routing.mjs verify 驗證。
//
// 用法：node scripts/ai/check-model-routing.mjs
//
// 檢查：
//  1. 五個 subagent（.claude/agents/）：name 與檔名一致、有 description、model 是別名、effort 是有效的等級
//     而且等於設計值、唯讀的三個沒有寫入的工具、五個都不能再委派（沒有 Agent 工具）。
//  2. .claude/settings.json：合法 JSON、UserPromptSubmit hook 已登錄並指向存在的檔案、
//     和 git HEAD 的版本相比沒有少任何權限、hook 或其他設定、沒有自動批准危險操作的權限模式。
//  3. hook 本身：範例需求分到預期的 agent、輸出的欄位與分數範圍、建議的 model 與 effort 等於該 agent 的
//     frontmatter、使用者回報兩次失敗會升級、成功後降級、最高一級不再升、agent 的通知不處理、
//     失敗時不擋（結束碼 0、不輸出）；再用 Git Bash 跑一次 settings.json 裡的指令字串。
//  4. CLAUDE.md 匯入 MODEL_ROUTING.md。
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
const HOOK = join(REPO_ROOT, '.claude', 'hooks', 'model_router.mjs');
const SETTINGS = join(REPO_ROOT, '.claude', 'settings.json');

const failures = [];
const warnings = [];
let passed = 0;

function check(ok, message) {
  if (ok) {
    passed++;
    console.log(`OK   ${message}`);
  } else {
    failures.push(message);
    console.log(`FAIL ${message}`);
  }
}

function warn(message) {
  warnings.push(message);
  console.log(`WARN ${message}`);
}

function readText(path) {
  return readFileSync(path, 'utf8').replace(/^\uFEFF/, '');
}

// ---- 1. subagent ---------------------------------------------------------------------------
// MODEL_ROUTING.md 第 3 節的分派表。effort 的有效值見官方文件（sub-agents 的 frontmatter）。
const AGENTS = [
  { name: 'fast-worker', model: 'haiku', effort: 'low', readOnly: true },
  { name: 'developer', model: 'sonnet', effort: 'medium', readOnly: false },
  { name: 'developer-deep', model: 'sonnet', effort: 'high', readOnly: false },
  { name: 'architect', model: 'opus', effort: 'high', readOnly: true },
  { name: 'architect-deep', model: 'opus', effort: 'xhigh', readOnly: true },
];
const EFFORT_LEVELS = ['low', 'medium', 'high', 'xhigh', 'max'];
const MODEL_ALIASES = ['haiku', 'sonnet', 'opus', 'fable'];
const WRITE_TOOLS = ['Edit', 'Write', 'NotebookEdit'];

function frontmatter(text) {
  const block = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n/);
  if (!block) return null;
  const fields = {};
  for (const line of block[1].split(/\r?\n/)) {
    const pair = line.match(/^([A-Za-z]+):\s*(.*)$/);
    if (pair) fields[pair[1]] = pair[2].trim().replace(/^(["'])(.*)\1$/, '$2');
  }
  return fields;
}

function toolList(value) {
  return (value ?? '').split(',').map((tool) => tool.trim()).filter(Boolean);
}

const agentConfig = new Map();
for (const agent of AGENTS) {
  const path = join(REPO_ROOT, '.claude', 'agents', `${agent.name}.md`);
  if (!existsSync(path)) {
    check(false, `subagent ${agent.name}：找不到 .claude/agents/${agent.name}.md`);
    continue;
  }
  const fields = frontmatter(readText(path));
  if (!fields) {
    check(false, `subagent ${agent.name}：沒有 frontmatter（--- … ---）`);
    continue;
  }
  agentConfig.set(agent.name, fields);
  check(fields.name === agent.name, `subagent ${agent.name}：name 與檔名一致（${fields.name}）`);
  check(Boolean(fields.description), `subagent ${agent.name}：有 description`);
  check(MODEL_ALIASES.includes(fields.model) && fields.model === agent.model,
    `subagent ${agent.name}：model 是別名 ${agent.model}（實際：${fields.model ?? '沒有寫'}）`);
  check(EFFORT_LEVELS.includes(fields.effort) && fields.effort === agent.effort,
    `subagent ${agent.name}：effort 是有效的等級 ${agent.effort}（實際：${fields.effort ?? '沒有寫'}）`);

  const tools = toolList(fields.tools);
  const disallowed = toolList(fields.disallowedTools);
  if (agent.readOnly) {
    const writers = tools.filter((tool) => WRITE_TOOLS.includes(tool));
    check(tools.length > 0 && writers.length === 0,
      `subagent ${agent.name}：唯讀，tools 有明確列出且沒有寫入的工具（${tools.join(', ') || '沒有列出，會繼承全部工具'}）`);
  }
  const canDelegate = tools.length > 0 ? tools.includes('Agent') : !disallowed.includes('Agent');
  check(!canDelegate, `subagent ${agent.name}：不能再委派（沒有 Agent 工具）`);
}
if (process.env.CLAUDE_CODE_EFFORT_LEVEL) {
  warn(`環境變數 CLAUDE_CODE_EFFORT_LEVEL=${process.env.CLAUDE_CODE_EFFORT_LEVEL}：它優先於 subagent 的 effort，frontmatter 的 effort 不會生效`);
}

// ---- 2. settings.json ----------------------------------------------------------------------
let settings = null;
try {
  settings = JSON.parse(readText(SETTINGS));
  check(true, '.claude/settings.json 是合法的 JSON');
} catch (error) {
  check(false, `.claude/settings.json 不是合法的 JSON：${error.message}`);
}

function commandsOf(hooks, event) {
  const commands = [];
  for (const entry of hooks?.[event] ?? []) {
    for (const hook of entry?.hooks ?? []) {
      if (hook?.type === 'command' && typeof hook.command === 'string') commands.push(hook);
    }
  }
  return commands;
}

let routerHook = null;
if (settings) {
  const routers = commandsOf(settings.hooks, 'UserPromptSubmit').filter((hook) => hook.command.includes('model_router.mjs'));
  check(routers.length === 1, `settings.json 的 UserPromptSubmit 有一個 model_router.mjs 的 hook（實際：${routers.length} 個）`);
  routerHook = routers[0] ?? null;
  check(existsSync(HOOK), 'hook 指向的 .claude/hooks/model_router.mjs 存在');
  if (routerHook) {
    const timeout = routerHook.timeout;
    check(timeout === undefined || (Number.isFinite(timeout) && timeout > 0 && timeout <= 30),
      `hook 的 timeout 在 30 秒內（UserPromptSubmit 的預設上限）：${timeout ?? '沒有寫，用預設'}`);
  }

  const mode = settings.permissions?.defaultMode;
  check(!['bypassPermissions', 'acceptEdits'].includes(mode), `沒有自動批准的權限模式（defaultMode：${mode ?? '沒有設定'}）`);
  const blanket = (settings.permissions?.allow ?? []).filter((rule) => /^(Bash|PowerShell)(\(\*\))?$/.test(rule));
  check(blanket.length === 0, `allow 沒有整個放行 shell 的規則${blanket.length ? `：${blanket.join(', ')}` : ''}`);

  const head = spawnSync('git', ['show', 'HEAD:.claude/settings.json'], { cwd: REPO_ROOT, encoding: 'utf8' });
  if (head.status !== 0) {
    check(false, `讀不到 git HEAD 的 .claude/settings.json：${(head.stderr || head.error?.message || '').trim()}`);
  } else {
    const before = JSON.parse(head.stdout.replace(/^\uFEFF/, ''));
    const missing = [];
    for (const key of Object.keys(before)) {
      if (!(key in settings)) missing.push(`頂層的 ${key}`);
    }
    for (const kind of ['allow', 'ask', 'deny']) {
      for (const rule of before.permissions?.[kind] ?? []) {
        if (!(settings.permissions?.[kind] ?? []).includes(rule)) missing.push(`permissions.${kind}：${rule}`);
      }
    }
    for (const event of Object.keys(before.hooks ?? {})) {
      const now = commandsOf(settings.hooks, event).map((hook) => hook.command);
      for (const hook of commandsOf(before.hooks, event)) {
        if (!now.includes(hook.command)) missing.push(`hooks.${event}：${hook.command}`);
      }
    }
    check(missing.length === 0, `和 git HEAD 相比沒有少任何設定${missing.length ? `，少了：${missing.join('；')}` : ''}`);
  }
}

// ---- 3. hook 的行為 --------------------------------------------------------------------------
const stateDir = mkdtempSync(join(tmpdir(), 'check-model-routing-'));

function runHook(stdin, extraEnv = {}) {
  const started = process.hrtime.bigint();
  const run = spawnSync(process.execPath, [HOOK], {
    input: stdin,
    encoding: 'utf8',
    env: { ...process.env, AOI_MODEL_ROUTER: '', AOI_MODEL_ROUTER_STATE_DIR: stateDir, ...extraEnv },
    timeout: 10000,
  });
  const ms = Number(process.hrtime.bigint() - started) / 1e6;
  return { status: run.status, stdout: run.stdout ?? '', stderr: run.stderr ?? '', error: run.error, ms };
}

// 回傳 { context, fields }；格式不對就回 null。
function parseOutput(stdout) {
  try {
    const specific = JSON.parse(stdout)?.hookSpecificOutput;
    if (specific?.hookEventName !== 'UserPromptSubmit' || typeof specific.additionalContext !== 'string') return null;
    const line = specific.additionalContext.split('\n').find((text) => text.startsWith('{'));
    return { context: specific.additionalContext, fields: line ? JSON.parse(line) : null };
  } catch {
    return null;
  }
}

let sessionCounter = 0;
const payload = (prompt, session = `check-${++sessionCounter}`) =>
  JSON.stringify({ session_id: session, hook_event_name: 'UserPromptSubmit', prompt });

const SCORE_KEYS = ['complexity_score', 'risk_score', 'reasoning_score', 'uncertainty_score'];
function fieldProblems(fields) {
  if (!fields) return ['沒有 JSON 欄位'];
  const problems = [];
  for (const key of ['recommended_agent', 'recommended_model', 'recommended_effort', 'routing_reason']) {
    if (typeof fields[key] !== 'string' || fields[key] === '') problems.push(`${key} 不是字串`);
  }
  for (const key of SCORE_KEYS) {
    if (!Number.isInteger(fields[key]) || fields[key] < 1 || fields[key] > 5) problems.push(`${key} 不在 1–5`);
  }
  if (!['single-file', 'module', 'cross-module', 'unknown'].includes(fields.scope)) problems.push(`scope 不對：${fields.scope}`);
  if (!['low', 'medium', 'high'].includes(fields.verification_cost)) problems.push(`verification_cost 不對：${fields.verification_cost}`);
  const config = agentConfig.get(fields.recommended_agent);
  if (!config) problems.push(`recommended_agent 不是五個 subagent 之一：${fields.recommended_agent}`);
  else if (config.model !== fields.recommended_model || config.effort !== fields.recommended_effort) {
    problems.push(`建議的 ${fields.recommended_model}/${fields.recommended_effort} 和 ${fields.recommended_agent} 的 frontmatter（${config.model}/${config.effort}）不一致`);
  }
  return problems;
}

// [需求, 預期的 subagent]。前三個是使用者最初給的範例。
const CASES = [
  ['找出專案中使用 OpenCV 的檔案', 'fast-worker'],
  ['修改 Qt 主視窗排版', 'developer'],
  ['分析 CUDA 記憶體競態問題', 'architect'],
  ['整理 build/qmake-release/test-logs 裡失敗的測試', 'fast-worker'],
  ['列出所有用到 JobRunner 的檔案', 'fast-worker'],
  ['Find all files that include opencv2', 'fast-worker'],
  ['幫參數面板新增一個下拉選單，並補上單元測試', 'developer'],
  ['Fix the typo in the About dialog', 'developer'],
  ['tst_presenter 偶爾會失敗，幫我找原因並修好', 'developer-deep'],
  ['量測並優化物件表填入 5000 列的效能瓶頸', 'developer-deep'],
  ['修正 perf_halcon 在真實影像上的位置誤差', 'developer-deep'],
  ['修復 JobRunner 取消時的 race condition', 'architect'],
  ['設計 AES 加密的金鑰管理流程', 'architect'],
  ['Refactor the presenter layer across modules', 'architect'],
  ['HALCON 偶爾拿不到授權，幫我查原因', 'architect-deep'],
  ['JobRunner 偶爾死結，無法重現，幫我找根因', 'architect-deep'],
  ['重新設計 JobRunner 的取消機制並調整跨模組的 port 契約', 'architect-deep'],
];

let slowest = 0;
if (existsSync(HOOK)) {
  for (const [prompt, expected] of CASES) {
    const run = runHook(payload(prompt));
    slowest = Math.max(slowest, run.ms);
    const output = parseOutput(run.stdout);
    const fields = output?.fields;
    const actual = fields?.recommended_agent ?? '沒有輸出';
    const problems = fieldProblems(fields);
    check(run.status === 0 && run.stderr === '' && actual === expected && problems.length === 0,
      `hook：「${prompt}」→ ${expected}（實際：${actual}${fields ? ` ${fields.recommended_model}/${fields.recommended_effort}，C${fields.complexity_score} R${fields.risk_score} Re${fields.reasoning_score} U${fields.uncertainty_score}` : ''}${problems.length ? `；${problems.join('；')}` : ''}）`);
    if (output && !output.context.includes('主 session 的模型與 effort 沒有切換')) {
      check(false, `hook：「${prompt}」的提示沒有寫明主 session 的模型與 effort 沒有切換`);
    }
  }

  const long = runHook(payload(`修改 Qt 主視窗排版。${'補充說明。'.repeat(400)}`));
  check(parseOutput(long.stdout)?.context.includes('可能不準') === true, 'hook：很長的需求會註明「關鍵字比對可能不準」');

  // 動態升級與降級：同一個 session 依序送出，[需求, 預期的 agent（null ＝ 不輸出）, 預期 escalated]
  const SEQUENCES = [
    ['兩次失敗升一級，成功後降級', [
      ['修改 Qt 主視窗排版', 'developer', false],
      ['還是不行', null, false],
      ['還是不行', 'developer-deep', true],
      ['好了，解決了', 'developer', false],
      ['列出所有 log 檔', 'fast-worker', false],
    ]],
    ['最高一級不再升，提示停下來', [
      ['JobRunner 偶爾死結，無法重現，幫我找根因', 'architect-deep', false],
      ['還是不行', null, false],
      ['還是不行', 'architect-deep', false],
    ]],
  ];
  for (const [label, steps] of SEQUENCES) {
    const session = `sequence-${++sessionCounter}`;
    const seen = [];
    let ok = true;
    let lastContext = '';
    for (const [prompt, expectedAgent, expectedEscalated] of steps) {
      const run = runHook(payload(prompt, session));
      const output = parseOutput(run.stdout);
      const agent = output?.fields?.recommended_agent ?? null;
      const escalated = output?.fields?.escalated ?? false;
      seen.push(`${agent ?? '不輸出'}${escalated ? '↑' : ''}`);
      if (run.status !== 0 || agent !== expectedAgent || escalated !== expectedEscalated) ok = false;
      if (output) lastContext = output.context;
    }
    if (label.startsWith('最高一級') && !lastContext.includes('最高一級')) ok = false;
    check(ok, `hook：${label}（實際：${seen.join(' → ')}）`);
  }

  const QUIET = [
    ['空的輸入', '', {}],
    ['不是 JSON', 'not json', {}],
    ['prompt 不是字串', '{"prompt":123}', {}],
    ['太短的回覆（沒有前一個需求）', payload('好'), {}],
    ['斜線指令', payload('/ai-handoff 收工'), {}],
    ['背景工作的通知', payload('<task-notification><task-id>x</task-id><summary>JobRunner race</summary></task-notification>'), {}],
    ['subagent 的回報', payload('[Subagent hand-back] The text below is the final report of a subagent: JobRunner thread race'), {}],
    ['AOI_MODEL_ROUTER=off', payload('分析 CUDA 記憶體競態問題'), { AOI_MODEL_ROUTER: 'off' }],
  ];
  for (const [label, stdin, env] of QUIET) {
    const run = runHook(stdin, env);
    slowest = Math.max(slowest, run.ms);
    check(run.status === 0 && run.stdout === '' && run.stderr === '',
      `hook：${label} → 結束碼 0、不輸出（實際：結束碼 ${run.status}，stdout ${run.stdout.length} 字元，stderr ${run.stderr.length} 字元）`);
  }

  // 狀態資料夾寫不進去（指到一個檔案）：照樣輸出、不報錯。
  const noState = runHook(payload('修改 Qt 主視窗排版'), { AOI_MODEL_ROUTER_STATE_DIR: HOOK });
  check(noState.status === 0 && noState.stderr === '' && parseOutput(noState.stdout)?.fields?.recommended_agent === 'developer',
    'hook：狀態資料夾寫不進去時照樣輸出、不報錯');
  check(slowest < 5000, `hook：每次執行在 5 秒內（最慢 ${Math.round(slowest)} ms，含 Node 啟動）`);
}
rmSync(stateDir, { recursive: true, force: true });

// 用 Git Bash 跑 settings.json 裡的指令字串，和 Claude Code 在 Windows 的做法一樣（shell form）。
function findGitBash() {
  const candidates = [];
  if (process.env.CLAUDE_CODE_GIT_BASH_PATH) candidates.push(process.env.CLAUDE_CODE_GIT_BASH_PATH);
  const execPath = spawnSync('git', ['--exec-path'], { encoding: 'utf8' });
  if (execPath.status === 0) candidates.push(resolve(execPath.stdout.trim(), '..', '..', '..', 'bin', 'bash.exe'));
  candidates.push('C:\\Program Files\\Git\\bin\\bash.exe');
  return candidates.find((path) => existsSync(path)) ?? null;
}

if (routerHook && process.platform === 'win32') {
  const bash = findGitBash();
  if (!bash) {
    warn('找不到 Git Bash：沒有驗證 settings.json 的指令字串（沒有 Git Bash 時 Claude Code 改用 PowerShell 執行 hook）');
  } else {
    const bashState = mkdtempSync(join(tmpdir(), 'check-model-routing-bash-'));
    const run = spawnSync(bash, ['-c', routerHook.command], {
      input: payload('分析 CUDA 記憶體競態問題'),
      encoding: 'utf8',
      env: { ...process.env, AOI_MODEL_ROUTER: '', AOI_MODEL_ROUTER_STATE_DIR: bashState, CLAUDE_PROJECT_DIR: REPO_ROOT },
      timeout: 15000,
    });
    rmSync(bashState, { recursive: true, force: true });
    const actual = parseOutput(run.stdout ?? '')?.fields?.recommended_agent ?? '沒有輸出';
    check(run.status === 0 && actual === 'architect',
      `Git Bash（${bash}）執行 settings.json 的指令字串 → architect（實際：${actual}，結束碼 ${run.status}${run.stderr ? `，stderr：${run.stderr.trim()}` : ''}）`);
  }
}

// ---- 4. CLAUDE.md ----------------------------------------------------------------------------
const claude = readText(join(REPO_ROOT, 'CLAUDE.md'));
check(/^@MODEL_ROUTING\.md\s*$/m.test(claude), 'CLAUDE.md 匯入 @MODEL_ROUTING.md');
check(existsSync(join(REPO_ROOT, 'MODEL_ROUTING.md')), 'MODEL_ROUTING.md 存在');

// ---- 結果 ------------------------------------------------------------------------------------
if (failures.length > 0) {
  console.log(`RESULT: FAILED (${failures.length} failure(s), ${passed} passed, ${warnings.length} warning(s))`);
  process.exitCode = 1;
} else {
  console.log(`RESULT: OK (${passed} passed, ${warnings.length} warning(s))`);
}
