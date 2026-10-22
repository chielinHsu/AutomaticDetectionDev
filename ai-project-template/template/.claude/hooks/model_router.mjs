#!/usr/bin/env node
// 模型與推理強度路由的 UserPromptSubmit hook（範本 2.0.0 的通用版；設計依據見 docs/adr/000-ai-agent-structure.md）：用關鍵字替需求打分——
// 複雜度、風險、推理深度、不確定性（1–5）、範圍、驗證成本——建議 agent、model 與 effort，
// 經 additionalContext 交給 Claude。規則見 MODEL_ROUTING.md。
//
// 這只是建議：不切換主 session 的模型與 effort；實際的模型與 effort 由所委派 subagent 的設定決定。
// 不呼叫外部 API。任何失敗都以結束碼 0、不輸出收場（結束碼 2 會擋住需求，其他非 0 會顯示 hook error）。
// 不處理：斜線指令（/…）、agent 與背景工作的通知；太短的文字只更新失敗與成功的狀態。
// AI_MODEL_ROUTER=off 時整個關掉。
//
// 狀態：同一個 session 裡使用者回報失敗的次數、升級的次數與升級的下限，存在
// %TEMP%/claude-model-router/<session>.json（AI_MODEL_ROUTER_STATE_DIR 可改；寫不進去就當沒有狀態），
// 7 天沒更新的檔案會被刪掉。
//
// 檢查：node scripts/ai/check-model-routing.mjs
import { mkdirSync, readFileSync, readdirSync, statSync, unlinkSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

const MIN_LENGTH = 6;              // 「好」「繼續」「還是不行」這類短回覆不重新分級
const LONG_PROMPT = 1500;          // 超過就註明「可能不準」：貼上的長文常提到和任務無關的字眼
const SCAN_LIMIT = 20000;
const MAX_REASONS = 3;
const FAILURES_TO_ESCALATE = 2;    // 使用者回報兩次失敗就升一級（MODEL_ROUTING.md 第 4 節）
const STATE_TTL_MS = 7 * 24 * 60 * 60 * 1000;

// 由低到高，升級就是往後一格。model 與 effort 要和 .claude/agents/ 的 frontmatter 一致（check-model-routing.mjs 會比對）。
const LADDER = [
  { agent: 'fast-worker', model: 'haiku', effort: 'low' },
  { agent: 'developer', model: 'sonnet', effort: 'medium' },
  { agent: 'developer-deep', model: 'sonnet', effort: 'high' },
  { agent: 'architect', model: 'opus', effort: 'high' },
  { agent: 'architect-deep', model: 'opus', effort: 'xhigh' },
];
const FAST = 0;
const DEV = 1;
const DEV_DEEP = 2;
const ARCH = 3;
const ARCH_DEEP = 4;

// 主題：命中就把各分數提高到至少這個值（取最大，不相加）。verification 是這類問題驗證一次的成本。
// 對應 MODEL_ROUTING.md 第 2 節的評分表。
const TOPICS = [
  { reason: '系統架構', risk: 4, complexity: 4, reasoning: 4, verification: 'medium', crossModule: true, pattern: /系統架構|架構(設計|調整|變更|規劃|重整)|(改|調整|新增)(分層|模組)|分層(規則|架構)|依賴(方向|規則|表)|port\s*(契約|介面)|\bADR\b|architecture|dependency\s+rule/i },
  { reason: '並行與生命週期', risk: 4, complexity: 3, reasoning: 4, verification: 'high', pattern: /執行緒|競態|競爭條件|死結|race\s*condition|data\s*race|deadlock|mutex|thread|可重入|reentran|生命週期|lifetime|所有權|ownership|use-after-free|dangling|記憶體(洩漏|損毀|錯誤|問題)|memory\s*(leak|corruption)/i },
  { reason: '安全、金鑰與授權', risk: 5, complexity: 3, reasoning: 4, verification: 'high', pattern: /加密|解密|\bAES\b|金鑰|密鑰|key\s*management|crypto|密碼|憑證|credential|secret|授權(檔|提醒|判斷|錯誤)|license|dongle/i },
  { reason: '不可逆的資料操作', risk: 5, complexity: 3, reasoning: 3, verification: 'high', pattern: /資料遷移|遷移(資料|格式)|schema|migration|不可逆|force[\s-]*push|reset\s+--hard|刪除.{0,8}(資料|資料庫|使用者|備份)/i },
  { reason: '跨模組重構', risk: 4, complexity: 5, reasoning: 4, verification: 'medium', crossModule: true, pattern: /跨(模組|層|檔案|執行緒)|大(規模|範圍)重構|重構.{0,10}(架構|模組|分層)|refactor.{0,30}(modules?|architecture|layers?)|across\s+(modules|layers)/i },
  { reason: '效能', risk: 3, complexity: 4, reasoning: 4, verification: 'high', pattern: /CUDA|\bGPU\b|瓶頸|bottleneck|profiler|profiling|(效能|性能)(分析|優化|最佳化|問題|瓶頸|退化|下降)|performance\s+(analysis|issue|regression|bottleneck)/i },
  { reason: '根因不明', risk: 3, reasoning: 4, uncertainty: 4, verification: 'high', pattern: /無法重現|不能重現|重現不了|偶爾|間歇|時好時壞|intermittent|flaky|heisenbug|根因不明|原因不明|找不到原因|查不出(原因|來)/i },
  { reason: '建置系統', risk: 3, complexity: 3, reasoning: 3, verification: 'medium', pattern: /CMake|Makefile|MSBuild|qmake|\bjom\b|gradle|webpack|\.csproj|\.vcxproj|\.pro\b|\.pri\b|建置(系統|設定|選項)|編譯選項|連結錯誤|link(er)?\s+error|\bLNK\d{4}\b/i },
];
const VERIFICATION_LEVELS = ['low', 'medium', 'high'];

// 動作：查詢類（Haiku）要「有查詢的字眼，而且沒有修改、分析、設計的字眼」。
const LOOKUP = /找出|找到|搜尋|查詢|查一下|列出|哪(些|個|裡|一個)|在哪|grep|\bsearch\b|\bfind\b|where\s+is|\blist\b|整理.{0,40}(log|日誌|紀錄|輸出|結果|失敗)|摘要|總結|summar|統計|彙整|有沒有|有多少|幾個/i;
const EDIT = /修改|修正|修復|修好|修掉|修一下|改成|改為|改掉|改一下|改用|實作|實現|新增|加入|加上|增加|刪除|移除|重構|重寫|撰寫|建立|產生|寫(一|個|出|入|進|成)|套用|commit|\b(implement|fix|add|change|update|rename|refactor|create|write|remove|delete|replace)\b/i;
const ANALYSIS = /分析|診斷|除錯|調查|debug|為什麼|為何|原因|根因|評估|審查|review|比較|建議|辦法|怎麼(做|改|辦|處理|修)|如何|\b(explain|why|analy[sz]e|investigate)\b/i;
const DESIGN = /設計|規劃|取捨|trade-?off|\bdesign\b/i;
const BUG = /錯誤|失敗|\bbug\b|crash|當機|當掉|閃退|崩潰|例外|exception|異常|不正確|不對|壞掉|\berror\b/i;
const WHY = /為什麼|為何|原因|怎麼會|\bwhy\b/i;
const FEATURE = /新功能|新增.{0,8}(功能|模組|頁面|面板|工具|選單|按鈕|欄位|對話框)|實作.{0,8}(功能|模組|流程)|\bimplement\b|\bfeature\b/i;
const MULTI = /多個(檔案|模組|類別)|整個|全部的|所有的|每個/i;
// 驗證成本：要硬體、授權、真實影像、效能量測或人工確認的，驗證一次很貴（high）。
const VERIFY_HIGH = /dongle|授權|license|真實(影像|資料)|real\s+(image|data)|效能量測|量測|\bperf_|benchmark|基準線|手動(測試|驗證|確認)|硬體|相機|camera|截圖|screenshot/i;
const DOCS = /文件|說明|README|\.md\b|註解|文案|typo|錯字|翻譯/i;
const CODE = /\.(cpp|h|hpp|cs|pro|pri|mjs|js|ts|py|ps1)\b|程式|函式|類別|\bclass\b|\bfunction\b|編譯|建置|\bbuild\b|測試|\btest/i;
const MODULE_WORDS = /面板|panel|模組|module|back\s*end|front\s*end|storage|畫布|canvas|hook|subagent|use\s*case|\bport\b/i;
const FILE = /[\w./\\-]*\w\.(?:cpp|cc|h|hpp|mjs|js|ts|py|pro|pri|json|md|ps1|ui|qrc|html)\b/gi;
// 兩個以上不同的 src/<資料夾> ＝ 跨模組（資料夾名稱不限）。
const LAYER = /(?:^|[\\/])src[\\/]([A-Za-z0-9_-]+)[\\/]/i;
// 狀態：使用者回報失敗或成功。
const FAILURE = /還是(不行|失敗|錯|有問題|一樣|沒用)|又(失敗|錯了|壞了|當了)|仍然(失敗|不行|有問題|錯)|沒(有)?(修好|解決)|still\s+(fail|broken|not\s+work)|(doesn't|does\s+not|didn't|did\s+not)\s+work|same\s+error|一樣的錯/i;
const SUCCESS = /好了|解決了|修好了|通過了|成功了|可以了|沒問題了|\b(works\s+now|fixed|resolved|all\s+(tests\s+)?pass(ed)?)\b/i;
// agent 與背景工作的通知也會觸發 UserPromptSubmit，不是使用者的需求。
const INJECTED = /^\s*<(task-notification|agent-message|cross-session-message|system-reminder)\b|\[Subagent hand-back\]|\[SYSTEM NOTIFICATION/;

function assess(text) {
  const result = { lookup: false, complexity: 1, risk: 1, reasoning: 1, uncertainty: 1, scope: 'unknown', verification: 'low', topics: [] };
  const raise = (key, value) => {
    if (value !== undefined && value > result[key]) result[key] = value;
  };

  for (const topic of TOPICS) {
    const match = text.match(topic.pattern);
    if (match) result.topics.push({ reason: topic.reason, hit: match[0].trim(), topic });
  }

  const files = [...new Set((text.match(FILE) ?? []).map((file) => file.replace(/\\/g, '/')))];
  const layers = new Set(files.map((file) => file.match(LAYER)?.[1]).filter(Boolean));
  if (layers.size >= 2 || result.topics.some(({ topic }) => topic.crossModule)) result.scope = 'cross-module';
  else if (files.length >= 2 || MODULE_WORDS.test(text)) result.scope = 'module';
  else if (files.length === 1) result.scope = 'single-file';

  const lookup = text.match(LOOKUP);
  const edit = text.match(EDIT);
  const analysis = ANALYSIS.test(text);
  const design = DESIGN.test(text);
  const bug = BUG.test(text);
  if (lookup && !edit && !analysis && !design) {
    // 查詢不改任何東西：風險、推理都是 1，主題只用來提醒。
    result.lookup = true;
    result.verb = lookup[0];
    return result;
  }

  if (edit) {
    raise('complexity', 2);
    raise('risk', 2);
    raise('reasoning', 2);
  }
  if (analysis || bug) raise('reasoning', 3);
  if (bug) raise('uncertainty', WHY.test(text) ? 3 : 2);
  if (design) raise('reasoning', 4);
  if (FEATURE.test(text) || (edit && MULTI.test(text)) || files.length >= 3) raise('complexity', 3);
  if (result.scope === 'cross-module') raise('complexity', 4);
  for (const { topic } of result.topics) {
    raise('risk', topic.risk);
    raise('complexity', topic.complexity);
    raise('reasoning', topic.reasoning);
    raise('uncertainty', topic.uncertainty);
  }
  if (result.topics.filter(({ topic }) => topic.risk >= 4).length >= 2) {
    // 兩個以上的高風險主題同時出現：推理與複雜度再高一級
    raise('reasoning', 5);
    raise('complexity', Math.min(5, result.complexity + 1));
  }

  const docsOnly = DOCS.test(text) && !CODE.test(text);
  const levels = [(edit || bug || analysis) && !docsOnly ? 'medium' : 'low', ...result.topics.map(({ topic }) => topic.verification)];
  if (VERIFY_HIGH.test(text)) levels.push('high');
  result.verification = VERIFICATION_LEVELS[Math.max(...levels.map((level) => VERIFICATION_LEVELS.indexOf(level)))];
  return result;
}

// 第一個符合的就停（MODEL_ROUTING.md 第 3 節）。
function decide(a) {
  if (a.lookup) return { index: FAST, rule: '查詢類的需求，沒有修改、分析或設計的字眼' };
  if (a.risk >= 4) {
    if (a.reasoning >= 5 || a.uncertainty >= 4) {
      const why = a.reasoning >= 5 ? `推理 ${a.reasoning} ≥ 5` : `不確定性 ${a.uncertainty} ≥ 4`;
      return { index: ARCH_DEEP, rule: `風險 ${a.risk} ≥ 4，而且${why}：高階模型加最高的推理強度` };
    }
    return { index: ARCH, rule: `風險 ${a.risk} ≥ 4（架構、安全或高風險修改）：直接用高階模型` };
  }
  const deep = [
    a.complexity >= 4 && `複雜度 ${a.complexity}`,
    a.reasoning >= 4 && `推理 ${a.reasoning}`,
    a.uncertainty >= 4 && `不確定性 ${a.uncertainty}`,
    a.verification === 'high' && '驗證成本高',
  ].filter(Boolean);
  if (deep.length > 0) return { index: DEV_DEEP, rule: `${deep.join('、')}，風險未達 4：先提高推理強度，不換模型` };
  return { index: DEV, rule: '一般開發' };
}

function stateLocation(sessionId) {
  if (typeof sessionId !== 'string' || sessionId === '') return null;
  const dir = process.env.AI_MODEL_ROUTER_STATE_DIR || join(tmpdir(), 'claude-model-router');
  return { dir, file: join(dir, `${sessionId.replace(/[^A-Za-z0-9_-]/g, '_')}.json`) };
}

function loadState(where) {
  const empty = { prompts: 0, failures: 0, escalations: 0, floor: -1, last: null };
  if (!where) return empty;
  try {
    return { ...empty, ...JSON.parse(readFileSync(where.file, 'utf8')) };
  } catch {
    return empty;
  }
}

function saveState(where, state) {
  if (!where) return;
  try {
    mkdirSync(where.dir, { recursive: true });
    writeFileSync(where.file, JSON.stringify(state));
    const now = Date.now();
    for (const name of readdirSync(where.dir)) {
      const path = join(where.dir, name);
      try {
        if (now - statSync(path).mtimeMs > STATE_TTL_MS) unlinkSync(path);
      } catch {
        // 別的 session 可能同時在清
      }
    }
  } catch {
    // 寫不進去就當沒有狀態
  }
}

function buildContext(prompt, fields, notes) {
  const lines = [
    '[模型路由提示] 關鍵字的初步評分，不是判斷；規則見 MODEL_ROUTING.md。',
    JSON.stringify(fields),
    ...notes,
  ];
  if (prompt.length > LONG_PROMPT) lines.push(`這段需求很長（${prompt.length} 字），關鍵字比對可能不準。`);
  lines.push('請依實際任務重新評估：只在值得時用 Agent 工具委派（subagent_type 填 recommended_agent），小任務或主 session 的模型與 effort 已經足夠時自己做。hook 只提供建議：主 session 的模型與 effort 沒有切換，實際的模型與 effort 由所委派 subagent 的設定決定。');
  return lines.join('\n');
}

function main() {
  if ((process.env.AI_MODEL_ROUTER || '').toLowerCase() === 'off') return;

  let input;
  try {
    input = JSON.parse(readFileSync(0, 'utf8').replace(/^﻿/, ''));
  } catch {
    return;
  }
  const prompt = typeof input?.prompt === 'string' ? input.prompt.trim() : '';
  if (prompt === '' || prompt.startsWith('/') || INJECTED.test(prompt)) return;

  const where = stateLocation(input.session_id);
  const state = loadState(where);
  const text = prompt.slice(0, SCAN_LIMIT);
  if (FAILURE.test(text)) {
    state.failures += 1;
  } else if (SUCCESS.test(text)) {
    // 問題解決：拿掉升級的下限，之後明確的小任務照常分級（降級）。
    state.failures = 0;
    state.floor = -1;
  }

  const short = prompt.length < MIN_LENGTH;
  let assessment;
  let decision;
  if (short) {
    if (!state.last) {
      state.prompts += 1;
      saveState(where, state);
      return;
    }
    assessment = state.last.assessment;
    decision = { index: state.last.index, rule: '沿用上一個需求的評分' };
  } else {
    assessment = assess(text);
    decision = decide(assessment);
  }

  let index = decision.index;
  const notes = [];
  let escalated = false;
  let atTop = false;
  const reportedFailures = state.failures;
  if (!assessment.lookup) {
    if (state.failures >= FAILURES_TO_ESCALATE) {
      const from = Math.max(index, state.floor);
      if (from >= ARCH_DEEP) {
        atTop = true;
        notes.push(`使用者回報失敗 ${state.failures} 次，已經是最高一級（${LADDER[ARCH_DEEP].agent}）：不要再重試同一個做法，整理已知的證據，請使用者決定下一步。`);
      } else {
        escalated = true;
        state.escalations += 1;
        state.floor = from + 1;
        notes.push(`使用者回報失敗 ${state.failures} 次：由 ${LADDER[from].agent} 升級到 ${LADDER[from + 1].agent}；先重新分析根因，不要重複同一個做法。`);
      }
      state.failures = 0;
    }
    if (state.floor > index) index = state.floor;
  }
  state.prompts += 1;
  state.last = {
    index,
    assessment: {
      lookup: assessment.lookup,
      complexity: assessment.complexity,
      risk: assessment.risk,
      reasoning: assessment.reasoning,
      uncertainty: assessment.uncertainty,
      scope: assessment.scope,
      verification: assessment.verification,
      topics: assessment.topics.map(({ reason, hit }) => ({ reason, hit })),
    },
  };
  saveState(where, state);
  if (short && !escalated && !atTop) return;

  const tier = LADDER[index];
  const hits = assessment.topics.slice(0, MAX_REASONS).map(({ reason, hit }) => `${reason}（${hit}）`);
  let reason = decision.rule;
  if (!assessment.lookup && hits.length > 0) reason += `；命中：${hits.join('、')}`;
  if (index !== decision.index) reason += `；升級的下限是 ${tier.agent}`;
  if (assessment.lookup && hits.length > 0) {
    notes.push(`注意：查詢的主題涉及 ${hits.join('、')}；要分析或修改時會重新評分。`);
  }
  if (index >= ARCH) notes.push('architect 系列只分析與規劃，實作交給 developer 系列或主 session。');

  const fields = {
    recommended_agent: tier.agent,
    recommended_model: tier.model,
    recommended_effort: tier.effort,
    complexity_score: assessment.complexity,
    risk_score: assessment.risk,
    reasoning_score: assessment.reasoning,
    uncertainty_score: assessment.uncertainty,
    scope: assessment.scope,
    verification_cost: assessment.verification,
    routing_reason: reason,
    failure_count: reportedFailures,
    escalation_count: state.escalations,
    escalated,
  };
  if (process.env.CLAUDE_EFFORT) fields.main_session_effort = process.env.CLAUDE_EFFORT;

  const output = {
    hookSpecificOutput: {
      hookEventName: 'UserPromptSubmit',
      additionalContext: buildContext(prompt, fields, notes),
    },
  };
  process.stdout.write(JSON.stringify(output));
}

process.stdout.on('error', () => {});
try {
  main();
} catch {
  // 失敗不擋使用者的需求：不輸出，結束碼 0。
}
process.exitCode = 0;
