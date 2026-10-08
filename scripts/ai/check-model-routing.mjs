#!/usr/bin/env node
// 模型路由的設定檢查（MODEL_ROUTING.md 第 8 節）。結束碼 0 ＝ 全部通過，1 ＝ 有失敗。
//
// 用法：node scripts/ai/check-model-routing.mjs
//
// 檢查：
//  1. 三個 subagent（.claude/agents/）：name 與檔名一致、有 description、model 是別名（haiku／sonnet／opus）、
//     唯讀的兩個沒有寫入的工具、三個都不能再委派（沒有 Agent 工具）。
//  2. .claude/settings.json：合法 JSON、UserPromptSubmit hook 已登錄並指向存在的檔案、
//     和 git HEAD 的版本相比沒有少任何權限、hook 或其他設定、沒有自動批准危險操作的權限模式。
//  3. hook 本身：範例需求的分類與輸出格式、失敗時不擋（結束碼 0、不輸出）；
//     再用 Git Bash 跑一次 settings.json 裡的指令字串（Claude Code 在 Windows 用 Git Bash 執行 hook）。
//  4. CLAUDE.md 匯入 MODEL_ROUTING.md。
//
// 驗證不了（要在 Claude Code 裡看）：Claude Code 是否真的載入了 subagent 與 hook、委派之後的回報。
import { existsSync, readFileSync } from 'node:fs';
import { spawnSync } from 'node:child_process';
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
const AGENTS = [
  { name: 'fast-worker', model: 'haiku', readOnly: true },
  { name: 'developer', model: 'sonnet', readOnly: false },
  { name: 'architect', model: 'opus', readOnly: true },
];
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
  check(fields.name === agent.name, `subagent ${agent.name}：name 與檔名一致（${fields.name}）`);
  check(Boolean(fields.description), `subagent ${agent.name}：有 description`);
  check(fields.model === agent.model, `subagent ${agent.name}：model 是別名 ${agent.model}（實際：${fields.model ?? '沒有寫'}）`);

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
function runHook(stdin, extraEnv = {}) {
  const started = process.hrtime.bigint();
  const run = spawnSync(process.execPath, [HOOK], {
    input: stdin,
    encoding: 'utf8',
    env: { ...process.env, AOI_MODEL_ROUTER: '', ...extraEnv },
    timeout: 10000,
  });
  const ms = Number(process.hrtime.bigint() - started) / 1e6;
  return { status: run.status, stdout: run.stdout ?? '', stderr: run.stderr ?? '', error: run.error, ms };
}

function contextOf(stdout) {
  try {
    const output = JSON.parse(stdout);
    const specific = output?.hookSpecificOutput;
    if (specific?.hookEventName !== 'UserPromptSubmit') return null;
    return typeof specific.additionalContext === 'string' ? specific.additionalContext : null;
  } catch {
    return null;
  }
}

const payload = (prompt) => JSON.stringify({ session_id: 'check', hook_event_name: 'UserPromptSubmit', prompt });

// [需求, 預期的 subagent]。前三個是使用者給的範例（MODEL_ROUTING.md 的來源 prompt）。
const CASES = [
  ['找出專案中使用 OpenCV 的檔案', 'fast-worker'],
  ['修改 Qt 主視窗排版', 'developer'],
  ['分析 CUDA 記憶體競態問題', 'architect'],
  ['整理 build/qmake-release/test-logs 裡失敗的測試', 'fast-worker'],
  ['列出所有用到 JobRunner 的檔案', 'fast-worker'],
  ['Find all files that include opencv2', 'fast-worker'],
  ['幫參數面板新增一個下拉選單，並補上單元測試', 'developer'],
  ['Fix the typo in the About dialog', 'developer'],
  ['修復 JobRunner 取消時的 race condition', 'architect'],
  ['設計 AES 加密的金鑰管理流程', 'architect'],
  ['Refactor the presenter layer across modules', 'architect'],
  ['HALCON 偶爾拿不到授權，幫我查原因', 'architect'],
];

let slowest = 0;
if (existsSync(HOOK)) {
  for (const [prompt, expected] of CASES) {
    const run = runHook(payload(prompt));
    slowest = Math.max(slowest, run.ms);
    const context = contextOf(run.stdout);
    const actual = context?.match(/subagent：([a-z-]+)/)?.[1] ?? '沒有輸出';
    check(run.status === 0 && run.stderr === '' && actual === expected,
      `hook：「${prompt}」→ ${expected}（實際：${actual}，結束碼 ${run.status}）`);
    if (context && !context.includes('主 session 的模型沒有切換')) {
      check(false, `hook：「${prompt}」的提示沒有寫明主 session 的模型沒有切換`);
    }
  }

  const long = runHook(payload(`修改 Qt 主視窗排版。${'補充說明。'.repeat(400)}`));
  check(contextOf(long.stdout)?.includes('可能不準') === true, 'hook：很長的需求會註明「關鍵字比對可能不準」');

  const QUIET = [
    ['空的輸入', '', {}],
    ['不是 JSON', 'not json', {}],
    ['prompt 不是字串', '{"prompt":123}', {}],
    ['太短的回覆', payload('好'), {}],
    ['斜線指令', payload('/ai-handoff 收工'), {}],
    ['AOI_MODEL_ROUTER=off', payload('分析 CUDA 記憶體競態問題'), { AOI_MODEL_ROUTER: 'off' }],
  ];
  for (const [label, stdin, env] of QUIET) {
    const run = runHook(stdin, env);
    slowest = Math.max(slowest, run.ms);
    check(run.status === 0 && run.stdout === '' && run.stderr === '',
      `hook：${label} → 結束碼 0、不輸出（實際：結束碼 ${run.status}，stdout ${run.stdout.length} 字元，stderr ${run.stderr.length} 字元）`);
  }
  check(slowest < 5000, `hook：每次執行在 5 秒內（最慢 ${Math.round(slowest)} ms，含 Node 啟動）`);
}

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
    const run = spawnSync(bash, ['-c', routerHook.command], {
      input: payload('分析 CUDA 記憶體競態問題'),
      encoding: 'utf8',
      env: { ...process.env, AOI_MODEL_ROUTER: '', CLAUDE_PROJECT_DIR: REPO_ROOT },
      timeout: 15000,
    });
    const actual = contextOf(run.stdout ?? '')?.match(/subagent：([a-z-]+)/)?.[1] ?? '沒有輸出';
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
