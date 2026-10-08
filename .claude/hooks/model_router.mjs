#!/usr/bin/env node
// 模型路由的 UserPromptSubmit hook：用關鍵字把使用者的需求初步分成 Haiku／Sonnet／Opus，
// 經 additionalContext 交給 Claude。規則見 MODEL_ROUTING.md。
//
// 這只是提示：不切換主 session 的模型，Claude 要依實際任務重新評估。
// 不呼叫外部 API。任何失敗都以結束碼 0、不輸出收場（結束碼 2 會擋住需求，其他非 0 會顯示 hook error）。
// 不處理：太短的文字、斜線指令（/…）；環境變數 AOI_MODEL_ROUTER=off 時整個關掉。
//
// 檢查：node scripts/ai/check-model-routing.mjs
import { readFileSync } from 'node:fs';

const MIN_LENGTH = 6;       // 「好」「繼續」這類回覆不分類
const LONG_PROMPT = 1500;   // 超過就註明「可能不準」：貼上的長文常提到和任務無關的字眼
const SCAN_LIMIT = 20000;
const MAX_REASONS = 3;

const TIERS = {
  haiku: { label: 'Haiku', agent: 'fast-worker', model: 'haiku' },
  sonnet: { label: 'Sonnet', agent: 'developer', model: 'sonnet' },
  opus: { label: 'Opus', agent: 'architect', model: 'opus' },
};

// Opus：任一條命中，而且不是單純查詢（見 classify）。對應 MODEL_ROUTING.md 第 3 節。
const OPUS_RULES = [
  { reason: '系統架構', pattern: /系統架構|架構(設計|調整|變更|規劃|重整)|(改|調整|新增)(分層|模組)|分層(規則|架構)|依賴(方向|規則|表)|port\s*(契約|介面)|\bADR\b|architecture|dependency\s+rule/i },
  { reason: '並行與生命週期', pattern: /執行緒|競態|競爭條件|死結|race\s*condition|data\s*race|deadlock|mutex|thread|可重入|reentran|latest-wins|JobRunner|CancelToken|生命週期|lifetime|所有權|ownership|use-after-free|dangling|記憶體(洩漏|損毀|錯誤|問題)|memory\s*(leak|corruption)/i },
  { reason: '座標與搜尋演算法', pattern: /座標(系|慣例|轉換)|角度(慣例|正負)|位姿|晶格|lattice|shape\s*model|由粗到細|coarse-to-fine|\bNCC\b/i },
  { reason: '加密、金鑰與授權', pattern: /加密|解密|\bAES\b|金鑰|密鑰|key\s*management|crypto|(HALCON|runtime|SDK|RTL).{0,6}授權|授權(檔|提醒|判斷|錯誤)|license|dongle/i },
  { reason: '不可逆的資料操作', pattern: /資料遷移|遷移(資料|格式)|schema|migration|不可逆|force[\s-]*push|reset\s+--hard|刪除.{0,8}(資料|配方|樣本庫|使用者)/i },
  { reason: '效能與 CUDA', pattern: /CUDA|\bGPU\b|瓶頸|bottleneck|profiler|profiling|(效能|性能)(分析|優化|最佳化|問題|瓶頸|退化|下降)|performance\s+(analysis|issue|regression|bottleneck)/i },
  { reason: '跨模組重構', pattern: /跨(模組|層|檔案|執行緒)|大(規模|範圍)重構|重構.{0,10}(架構|模組|分層)|refactor.{0,30}(modules?|architecture|layers?)|across\s+(modules|layers)/i },
  { reason: '根因不明', pattern: /無法重現|不能重現|重現不了|偶爾|間歇|時好時壞|intermittent|flaky|heisenbug|根因不明|原因不明|找不到原因|查不出(原因|來)/i },
];

// Haiku：查詢、整理、摘要類的字眼，而且沒有修改或分析的字眼。
const LOOKUP = /找出|找到|搜尋|查詢|查一下|列出|哪(些|個|裡|一個)|在哪|grep|\bsearch\b|\bfind\b|where\s+is|\blist\b|整理.{0,40}(log|日誌|紀錄|輸出|結果|失敗)|摘要|總結|summar|統計|彙整|有沒有|有多少|幾個/i;
const EDIT = /修改|修正|修復|修好|修掉|修一下|改成|改為|改掉|改一下|改用|實作|實現|新增|加入|加上|增加|刪除|移除|重構|重寫|撰寫|建立|產生|寫(一|個|出|入|進|成)|套用|commit|\b(implement|fix|add|change|update|rename|refactor|create|write|remove|delete|replace)\b/i;
const ANALYSIS = /分析|診斷|除錯|調查|debug|為什麼|為何|原因|根因|設計|規劃|評估|審查|review|比較|建議|辦法|怎麼(做|改|辦|處理|修)|如何|\b(explain|why|analy[sz]e|investigate|design)\b/i;

function classify(text) {
  const risky = [];
  for (const rule of OPUS_RULES) {
    const match = text.match(rule.pattern);
    if (match) risky.push(`${rule.reason}（${match[0].trim()}）`);
  }

  const lookup = text.match(LOOKUP);
  const edit = text.match(EDIT);
  if (lookup && !edit && !ANALYSIS.test(text)) {
    return { tier: TIERS.haiku, reasons: [`查詢類的需求（${lookup[0]}），沒有修改或分析的字眼`], risky };
  }
  if (risky.length > 0) {
    return { tier: TIERS.opus, reasons: risky, risky: [] };
  }
  const why = edit ? `一般開發（${edit[0]}）` : '沒有命中查詢或高風險的關鍵字，用預設';
  return { tier: TIERS.sonnet, reasons: [why], risky: [] };
}

function buildContext(prompt, result) {
  const { tier, reasons, risky } = result;
  const lines = [
    '[模型路由提示] 關鍵字的初步分類，不是判斷；規則見 MODEL_ROUTING.md。',
    `建議：${tier.label}（subagent：${tier.agent}，model：${tier.model}）。原因：${reasons.slice(0, MAX_REASONS).join('；')}。`,
  ];
  if (risky.length > 0) {
    lines.push(`注意：主題涉及 ${risky.slice(0, MAX_REASONS).join('；')}；要分析或修改時改用 architect。`);
  }
  if (tier === TIERS.opus) {
    lines.push('architect 只分析與規劃，實作交給 developer 或主 session。');
  }
  if (prompt.length > LONG_PROMPT) {
    lines.push(`這段需求很長（${prompt.length} 字），關鍵字比對可能不準。`);
  }
  lines.push('請依實際任務重新評估：只在值得時用 Agent 工具委派，小任務或主 session 已是建議的模型時自己做。主 session 的模型沒有切換。');
  return lines.join('\n');
}

function main() {
  if ((process.env.AOI_MODEL_ROUTER || '').toLowerCase() === 'off') return;

  let input;
  try {
    input = JSON.parse(readFileSync(0, 'utf8').replace(/^﻿/, ''));
  } catch {
    return;
  }
  const prompt = typeof input?.prompt === 'string' ? input.prompt.trim() : '';
  if (prompt.length < MIN_LENGTH || prompt.startsWith('/')) return;

  const result = classify(prompt.slice(0, SCAN_LIMIT));
  const output = {
    hookSpecificOutput: {
      hookEventName: 'UserPromptSubmit',
      additionalContext: buildContext(prompt, result),
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
