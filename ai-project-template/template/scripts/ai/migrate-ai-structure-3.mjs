#!/usr/bin/env node
// AI 結構 2.x → 3.0.0 的機械性遷移（範本管理）。先跑 New-AiProject.ps1 -Refresh 更新範本管理的檔案，再跑本腳本：
//
//   node scripts/ai/migrate-ai-structure-3.mjs --dry-run    只列出會做什麼
//   node scripts/ai/migrate-ai-structure-3.mjs              執行
//
// 會做的事（只做機械性、可還原的部分；判斷交給 agent，見 README「Upgrade 2.x to 3.0.0」）：
//   1. 建立 docs/plans/{active,completed,cancelled}/（空資料夾放 .gitkeep）。
//   2. 舊的全域紀錄搬到 docs/legacy/（git 追蹤的檔案用 git mv，保留歷史）：
//        docs/current-status.md → docs/legacy/current-status.md
//        docs/changelog-ai.md   → docs/legacy/changelog-ai.md
//        docs/plan/             → docs/legacy/plan/（docs/plan/_template.md 刪除，3.0.0 的範本在 docs/plans/_template/）
//   3. 改寫整個 repository 的 Markdown 相對連結：指向搬走的 changelog-ai.md 與 docs/plan/ 的連結、以及搬走的檔案裡的連結，
//      都換成新的相對路徑。docs/current-status.md 這個路徑會放新的專案快照，所以指向它的連結不轉址（新檔再連到封存的舊檔）。
//      反引號裡寫的具體舊計畫檔（例如 `docs/plan/S05-lattice.md`）也換成 docs/legacy/plan/ 的路徑；規則文字裡的
//      `docs/changelog-ai.md`、`docs/plan/<ID>…` 不動（語意要改寫，報告會列出）。範本管理的檔案不改。
//   4. 專案擁有的規則檔（AGENTS.md、CLAUDE.md、AI_CONTEXT.md、project-rules.md、MODEL_ROUTING.md、.claude/agents/、
//      docs/00-master-plan.md）裡，和範本 2.0.0 **逐字相同的整行**換成範本 3.0.0 的寫法（TEMPLATE_LINES，由範本的 diff 產生）；
//      專案改寫過的句子不動，留在報告裡由 agent 改寫。
//   5. 新的 docs/current-status.md（專案層級的快照）與 docs/plans/INDEX.md（下一個可用編號由舊的進度表推算）。
//   6. 報告：舊進度表裡還沒完成的需求（要拆成計畫），以及還在講 2.x 做法的文字（要改寫）。
// 不會 commit；不會改 AGENTS.md、CLAUDE.md 等專案擁有的規則文字（報告列出位置）。
import { execFileSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const args = process.argv.slice(2);
const dryRun = args.includes("--dry-run");
const rootArg = args.find((a) => a.startsWith("--root="));
const root = path.resolve(rootArg ? rootArg.slice(7) : path.join(path.dirname(fileURLToPath(import.meta.url)), "..", ".."));
const today = new Date().toISOString().slice(0, 10);
const log = (...m) => console.log(...m);
const rel = (p) => path.relative(root, p).split(path.sep).join("/");
const abs = (r) => path.join(root, ...r.split("/"));
const exists = (r) => fs.existsSync(abs(r));

const SKIP_DIRS = new Set([".git", "node_modules", "build", "dist", "out", "bin", "obj", "third_party", "vendor", ".venv", "venv", "target"]);
function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, e.name);
    const r = rel(full);
    if (e.isDirectory()) {
      if (SKIP_DIRS.has(e.name) || r === ".claude/worktrees" || r.startsWith("build")) continue;
      walk(full, out);
    } else if (e.name.endsWith(".md")) {
      out.push(r);
    }
  }
  return out;
}

let isGit = false;
try {
  isGit = execFileSync("git", ["-C", root, "rev-parse", "--is-inside-work-tree"], { encoding: "utf8", stdio: ["ignore", "pipe", "ignore"] }).trim() === "true";
} catch { /* 不是 git repository */ }
function tracked(r) {
  if (!isGit) return false;
  try {
    return execFileSync("git", ["-C", root, "ls-files", "--error-unmatch", r], { stdio: ["ignore", "pipe", "ignore"] }).length > 0;
  } catch { return false; }
}

// ---- 0. 前置檢查 ----
const versionFile = ".agents/harness/TEMPLATE_VERSION";
const version = exists(versionFile) ? fs.readFileSync(abs(versionFile), "utf8").trim() : "";
if (!/^3\./.test(version)) {
  console.error(`TEMPLATE_VERSION is '${version}'. Run New-AiProject.ps1 -Path <project> -Refresh from the 3.0.0 template first.`);
  process.exit(2);
}
if (!exists("docs/current-status.md") && !exists("docs/changelog-ai.md") && !exists("docs/plan")) {
  log("Nothing to migrate: docs/current-status.md, docs/changelog-ai.md and docs/plan/ are already gone or never existed.");
}

// ---- 1. 要搬的路徑 ----
const moves = [];
const legacyStatus = exists("docs/legacy/current-status.md");
if (exists("docs/current-status.md") && !legacyStatus) {
  const text = fs.readFileSync(abs("docs/current-status.md"), "utf8");
  if (/^##\s+需求進度表/m.test(text)) moves.push({ from: "docs/current-status.md", to: "docs/legacy/current-status.md", redirect: false });
  else log("SKIP   docs/current-status.md has no 2.x requirements table (already a 3.0.0 snapshot?)");
}
if (exists("docs/changelog-ai.md")) moves.push({ from: "docs/changelog-ai.md", to: "docs/legacy/changelog-ai.md", redirect: true });
const removeFiles = [];
if (exists("docs/plan")) {
  if (exists("docs/plan/_template.md")) removeFiles.push("docs/plan/_template.md");
  const others = fs.readdirSync(abs("docs/plan")).filter((n) => n !== "_template.md");
  if (others.length > 0) moves.push({ from: "docs/plan", to: "docs/legacy/plan", redirect: true });
}

// 檔案自己的新位置（所有搬動）。
const mapPath = (r) => {
  for (const m of moves) {
    if (r === m.from) return m.to;
    if (r.startsWith(m.from + "/")) return m.to + r.slice(m.from.length);
  }
  return r;
};
// 連結的目標要不要轉址：current-status.md 的路徑會放新的快照，不轉址。
const redirectPath = (r) => {
  for (const m of moves) {
    if (!m.redirect) continue;
    if (r === m.from) return m.to;
    if (r.startsWith(m.from + "/")) return m.to + r.slice(m.from.length);
  }
  return r;
};
const MANAGED = [".agents/harness/engineering-workflow.md", ".agents/skills/ai-handoff/", ".claude/skills/ai-handoff/", "docs/plans/_template/",
  "docs/steps/_template/", "docs/adr/_template.md"];
const isManaged = (r) => MANAGED.some((m) => r === m || r.startsWith(m));

// ---- 2. 連結改寫（先算好，搬完再寫） ----
const mdFiles = walk(root);
const rewrites = new Map(); // 新位置 → 新內容
let linkCount = 0;
for (const fileOld of mdFiles) {
  if (removeFiles.includes(fileOld) || isManaged(fileOld)) continue;
  const fileNew = mapPath(fileOld);
  const text = fs.readFileSync(abs(fileOld), "utf8");
  const dirOld = path.posix.dirname(fileOld);
  const dirNew = path.posix.dirname(fileNew);
  let changed = text.replace(/\]\(([^)\s]+?)(#[^)\s]*)?\)/g, (all, target, anchor) => {
    if (/^[a-zA-Z][a-zA-Z0-9+.-]*:/.test(target) || target.startsWith("/") || target.startsWith("#")) return all;
    let decoded;
    try { decoded = decodeURI(target); } catch { return all; }
    const resolved = path.posix.normalize(path.posix.join(dirOld, decoded));
    if (resolved.startsWith("..")) return all;
    const mapped = redirectPath(resolved);
    if (mapped === resolved && fileNew === fileOld) return all;
    let next = path.posix.relative(dirNew, mapped) || ".";
    if (decoded.endsWith("/") && !next.endsWith("/")) next += "/";
    if (target.startsWith("./") && !next.startsWith(".")) next = "./" + next;
    if (next === decoded) return all;
    linkCount++;
    return "](" + (/%[0-9A-Fa-f]{2}/.test(target) ? encodeURI(next) : next) + (anchor || "") + ")";
  });
  // 反引號裡的完整路徑，指向搬走的檔案（例如 `docs/plan/S05-lattice.md`）。
  changed = changed.replace(/`(docs\/plan\/[^`\s<>*]+\.md)`/g, (all, p) => {
    const mapped = redirectPath(p);
    if (mapped === p || !exists(p) || removeFiles.includes(p)) return all;
    linkCount++;
    return "`" + mapped + "`";
  });
  if (changed !== text) rewrites.set(fileNew, changed);
}


// ---- 範本 2.0.0 → 3.0.0 逐字相同的整行（由範本的 git diff 產生；只在整行完全相同時替換） ----
const TEMPLATE_FILES = ["AGENTS.md", "CLAUDE.md", "AI_CONTEXT.md", ".agents/harness/project-rules.md", "MODEL_ROUTING.md",
  ".claude/agents/architect.md", ".claude/agents/architect-deep.md", ".claude/agents/developer.md", ".claude/agents/developer-deep.md",
  "docs/00-master-plan.md"];
const TEMPLATE_LINES = [
 [
  "  E --> F[\"為下一步開新 session（Start prompt 在下一步 plan 檔底部）\"]",
  "  E --> F[\"為下一步開新 session（Start prompt 在下一步計畫 plan.md 的底部）\"]"
 ],
 [
  "4. 非單純需求在實作前，先在 `docs/current-status.md` 的「需求進度表」把需求展開成細項（狀態 ⬜）；完成或變動時逐項更新狀態、分支／commit 與驗證。規則見 `engineering-workflow.md`「需求展開與進度表」。",
  "4. 非單純需求在實作前，先在 `docs/plans/INDEX.md` 找到或建立它所屬的計畫（`docs/plans/active/<ID>-<slug>/`），在該計畫 `plan.md` 的細項表展開細項（狀態 ⬜）；完成或變動時只更新那個計畫的細項表、`status.md` 與 `changelog.md`，不寫進其他計畫或專案層級的檔案。規則見 `engineering-workflow.md`「計畫分開管理」。"
 ],
 [
  "2. [`.agents/harness/engineering-workflow.md`](.agents/harness/engineering-workflow.md) — 共用工作流程：證據、驗證、回報、交接、需求進度表",
  "2. [`.agents/harness/engineering-workflow.md`](.agents/harness/engineering-workflow.md) — 共用工作流程：證據、驗證、回報、交接、計畫分開管理"
 ],
 [
  "5. [`docs/current-status.md`](docs/current-status.md) — 需求進度表（開頭）、目前進度與已知問題",
  "5. [`docs/current-status.md`](docs/current-status.md) — 專案層級的快照：階段、跨計畫的已知問題與待決定\n6. [`docs/plans/INDEX.md`](docs/plans/INDEX.md) — 計畫索引：找出這次要處理的計畫，**只讀那個計畫**的 `plan.md` 與 `status.md`（需要歷史才讀它的 `changelog.md`）"
 ],
 [
  "改架構或做設計決策前，另讀 `docs/architecture.md` 與 `docs/decisions.md`（索引，內容在 `docs/adr/`）；接續跨 session 的需求時，讀進度表「計畫」欄連到的 `docs/plan/` 檔。純問答、解釋、狀態詢問不必走完整流程，但仍遵守上面四條。",
  "改架構或做設計決策前，另讀 `docs/architecture.md` 與 `docs/decisions.md`（索引，內容在 `docs/adr/`）。3.0.0 之前的全域紀錄在 `docs/legacy/`，只在需要追查舊歷史時讀。純問答、解釋、狀態詢問不必走完整流程，但仍遵守上面四條。"
 ],
 [
  "依 `ai-handoff` skill 執行：更新 `docs/current-status.md` 的需求進度表、覆寫 `.ai/handoff.md`、在 `docs/changelog-ai.md` 最上方新增一筆，有設計決策就新增 `docs/adr/NNN-slug.md` 並在 `docs/decisions.md` 加索引。改過入口檔或 skill 後，執行 `scripts/ai/check-ai-structure.ps1`。最後的回覆要總結修改內容（第 1 條）。",
  "依 `ai-handoff` skill 執行：更新本次動到的計畫（`plan.md` 細項表、`status.md`、在 `changelog.md` 最上方新增一筆；計畫層級的狀態改變時才改 `docs/plans/INDEX.md`）、覆寫 `.ai/handoff.md`，有設計決策就新增 `docs/adr/NNN-slug.md` 並在 `docs/decisions.md` 加索引。改過入口檔或 skill 後，執行 `scripts/ai/check-ai-structure.ps1`。最後的回覆要總結修改內容（第 1 條）。"
 ],
 [
  "上面五份每個 session 都會載入（`MODEL_ROUTING.md` 是 Claude 專用的模型路由規則）。`.ai/handoff.md` 與 `docs/current-status.md` 會頻繁變動，不自動匯入；非單純任務開始前，依 `AGENTS.md` 的清單讀取。",
  "上面五份每個 session 都會載入（`MODEL_ROUTING.md` 是 Claude 專用的模型路由規則）。`.ai/handoff.md`、`docs/current-status.md` 與 `docs/plans/` 會頻繁變動，不自動匯入；非單純任務開始前，依 `AGENTS.md` 的清單讀取。"
 ],
 [
  "- **architect／architect-deep 的計畫**：這兩個 subagent 唯讀、只出計畫；由**主 agent** 把回傳的計畫寫進 `docs/current-status.md` 的需求進度表與 `docs/plan/<ID>-<slug>.md`（`engineering-workflow.md`「需求展開與進度表」），再交給 developer 系列或自己實作。",
  "- **architect／architect-deep 的計畫**：這兩個 subagent 唯讀、只出計畫；由**主 agent** 把回傳的計畫寫進 `docs/plans/active/<ID>-<slug>/`（`plan.md` 的細項表）與 `docs/plans/INDEX.md`（`engineering-workflow.md`「計畫分開管理」），再交給 developer 系列或自己實作。"
 ],
 [
  "- **Step Protocol 第 7 步**：用 `start_session` 或 `spawn_task`（工具可用時）為下一步開新 session；prompt 取自下一步 plan 檔底部的「Start prompt」，並提醒新 session 回覆使用繁體中文。",
  "- **Step Protocol 第 7 步**：用 `start_session` 或 `spawn_task`（工具可用時）為下一步開新 session；prompt 取自下一步計畫 `plan.md` 底部的「Start prompt」，並提醒新 session 回覆使用繁體中文。"
 ],
 [
  "| `docs/` | 總體計畫、架構、決策（`adr/`）、需求進度表（`current-status.md`）、計畫檔（`plan/`）、步驟輸出（`steps/`）、AI 工作紀錄 |",
  "| `docs/` | 總體計畫、架構、決策（`adr/`）、專案層級的快照（`current-status.md`）、計畫（`plans/`：`INDEX.md` 與每個計畫的資料夾）、步驟輸出（`steps/`）、3.0.0 之前的封存紀錄（`legacy/`，有的話） |"
 ],
 [
  "| 接續上一位 agent 的工作 | `.ai/handoff.md`、`docs/current-status.md`（需求進度表）、`docs/changelog-ai.md` | 先看 `git status`，別覆蓋未完成的修改 |",
  "| 接續上一位 agent 的工作 | `.ai/handoff.md`、`docs/plans/INDEX.md`、該計畫的 `status.md`（需要歷史才讀 `changelog.md`） | 先看 `git status`，別覆蓋未完成的修改 |"
 ],
 [
  "| 展開需求、追蹤進度 | `docs/current-status.md`、`docs/plan/_template.md`、`engineering-workflow.md`「需求展開與進度表」 | 非單純需求先展開成細項再實作 |",
  "| 新需求、展開細項、追蹤進度 | `docs/plans/INDEX.md`、`docs/plans/_template/`、`engineering-workflow.md`「計畫分開管理」 | 先判斷屬於哪個計畫；非單純需求先展開細項再實作 |"
 ],
 [
  "| 做一個步驟（Step Protocol） | `docs/00-master-plan.md`、`docs/plan/<ID>-*.md`、`project-rules.md`、`docs/steps/_template/` | 功能類需求要交付四種輸出 |",
  "| 做一個步驟（Step Protocol） | `docs/00-master-plan.md`、`docs/plans/active/<ID>-*/plan.md`、`project-rules.md`、`docs/steps/_template/` | 功能類需求要交付四種輸出 |"
 ],
 [
  "本檔放「這個專案的慣例」，可依專案調整。流程在 `engineering-workflow.md`，專案事實在 `AI_CONTEXT.md`。標示 `TODO(project)` 的地方，建立專案時填寫或刪除；其餘段落是範本 2.0.0 的通用規則，專案可依需要改寫，但不要刪掉 SOLID、四種輸出、Step Protocol、Git 與 Commit 訊息的骨架。",
  "本檔放「這個專案的慣例」，可依專案調整。流程在 `engineering-workflow.md`，專案事實在 `AI_CONTEXT.md`。標示 `TODO(project)` 的地方，建立專案時填寫或刪除；其餘段落是範本 3.0.0 的通用規則，專案可依需要改寫，但不要刪掉 SOLID、四種輸出、Step Protocol、Git 與 Commit 訊息的骨架。"
 ],
 [
  "**適用範圍**：Step Protocol 的步驟，以及 `docs/current-status.md` 需求進度表中類型標為「功能」的需求。**小修正（fix、docs、chore、refactor 等）免四種輸出**，只要依 `ai-handoff` skill 更新進度表、交接單與 changelog。",
  "**適用範圍**：Step Protocol 的步驟，以及類型標為「功能」的計畫（`docs/plans/INDEX.md`）。**小修正（fix、docs、chore、refactor 等）免四種輸出**，只要依 `ai-handoff` skill 更新該計畫的細項表、`status.md`、`changelog.md` 與交接單。"
 ],
 [
  "步驟 ID `<ID>` 用總體計畫（`docs/00-master-plan.md`）的編號，例如 `S01`；需求進度表的 `Rnn` 若自成一步也可以當 ID。",
  "步驟 ID `<ID>` 用總體計畫（`docs/00-master-plan.md`）的編號，例如 `S01`；計畫的 `Rnn` 若自成一步也可以當 ID。每個步驟就是一個計畫（`docs/plans/<狀態>/<ID>-<slug>/`）。"
 ],
 [
  "1. 開工：讀 `docs/current-status.md`（需求進度表）與 `docs/plan/<ID>-*.md`。從預設分支建立分支 `step/NN-slug`；把本步驟在進度表裡的列標為 ⏳。",
  "1. 開工：讀 `docs/plans/INDEX.md` 與該步驟計畫的 `plan.md`、`status.md`。從預設分支建立分支 `step/NN-slug`；計畫的狀態改 ⏳（`plan.md` 的 frontmatter、`status.md`、`INDEX.md`）。"
 ],
 [
  "3. 驗證：建置通過、單元/契約測試通過、`node tools/ste-lint.mjs` 無 error，以及 `AI_CONTEXT.md` 與該步驟 plan 檔「完成定義」列出的其他檢查；本步驟的效能指標已量測並與預算（若專案有定義）比對。**失敗就如實回報，不要宣稱完成。**",
  "3. 驗證：建置通過、單元/契約測試通過、`node tools/ste-lint.mjs` 無 error，以及 `AI_CONTEXT.md` 與該步驟 `plan.md`「完成定義」列出的其他檢查；本步驟的效能指標已量測並與預算（若專案有定義）比對。**失敗就如實回報，不要宣稱完成。**"
 ],
 [
  "4. 更新 `docs/current-status.md`（需求進度表該列改 ✅，補分支／commit 與驗證），並依 `ai-handoff` skill 更新 `.ai/handoff.md` 與 `docs/changelog-ai.md`。",
  "4. 依 `ai-handoff` skill 更新該計畫：細項表改 ✅（補分支／commit 與驗證）、`status.md`、`changelog.md`；計畫完成就把資料夾搬到 `docs/plans/completed/` 並更新 `INDEX.md`；最後覆寫 `.ai/handoff.md`。"
 ],
 [
  "5. Commit（格式見下方「Commit 訊息」；該步驟 plan 檔的「Commit 範本」只當內容參考），合併到預設分支（`git merge --no-ff`），打 tag `step-NN-done`。僅限本機，**不要 push**，除非使用者要求。",
  "5. Commit（格式見下方「Commit 訊息」；該步驟 `plan.md` 的「Commit 範本」只當內容參考），合併到預設分支（`git merge --no-ff`），打 tag `step-NN-done`。僅限本機，**不要 push**，除非使用者要求。"
 ],
 [
  "7. 為下一步建立新 session（prompt 取自**下一步** plan 檔底部的「Start prompt」，並提醒新 session 回覆使用繁體中文）：Claude Code 用 `start_session` 或 `spawn_task`（工具可用時）；Codex 或工具不可用時，把 prompt 印出來讓使用者貼上。**到此為止，不要接著做下一步。**",
  "7. 為下一步建立新 session（prompt 取自**下一步**計畫 `plan.md` 底部的「Start prompt」，並提醒新 session 回覆使用繁體中文）：Claude Code 用 `start_session` 或 `spawn_task`（工具可用時）；Codex 或工具不可用時，把 prompt 印出來讓使用者貼上。**到此為止，不要接著做下一步。**"
 ],
 [
  "- 需求進度表（`docs/current-status.md`）與計畫檔（`docs/plan/`）的規則見 `engineering-workflow.md`「需求展開與進度表」。",
  "- 計畫（`docs/plans/`）與專案層級快照（`docs/current-status.md`）的規則見 `engineering-workflow.md`「計畫分開管理」。"
 ],
 [
  "- **規劃與進度由主 agent 寫**：architect 系列只回傳計畫；主 agent 把它寫進 `docs/current-status.md` 的需求進度表與 `docs/plan/`，subagent 不直接改這兩份文件。",
  "- **規劃與進度由主 agent 寫**：architect 系列只回傳計畫；主 agent 把它寫進 `docs/plans/active/<ID>-<slug>/`（`plan.md` 的細項表）與 `docs/plans/INDEX.md`，subagent 不直接改這些文件。"
 ],
 [
  "- 你的計畫由主 agent 寫進 `docs/current-status.md` 的需求進度表與 `docs/plan/`（`engineering-workflow.md`「需求展開與進度表」）：回報時把計畫拆成可編號的細項（`Rnn.m`），每項寫清楚內容、檔案與驗證方式，必要時附計畫檔的完成定義與 Commit 範本草稿。",
  "- 你的計畫由主 agent 寫進 `docs/plans/active/<ID>-<slug>/`（`plan.md` 的細項表）與 `docs/plans/INDEX.md`（`engineering-workflow.md`「計畫分開管理」）：先說明需求屬於哪個既有計畫，或要開新計畫；回報時把計畫拆成可編號的細項（`Rnn.m`），每項寫清楚內容、檔案與驗證方式，必要時附計畫檔的完成定義與 Commit 範本草稿。"
 ],
 [
  "- 不修改檔案（包含 `docs/current-status.md` 與 `docs/plan/`）、不 stage、不 commit、不 push、不再委派其他 agent。",
  "- 不修改檔案（包含 `docs/plans/` 與 `docs/current-status.md`）、不 stage、不 commit、不 push、不再委派其他 agent。"
 ],
 [
  "- 主 agent 沒有明確交付時，不自行改寫 `docs/current-status.md` 的需求進度表與 `docs/plan/`；進度由主 agent 更新，你只在回報中列出每個細項的結果。",
  "- 主 agent 沒有明確交付時，不自行改寫 `docs/plans/`（計畫的細項表、`status.md`、`changelog.md`、`INDEX.md`）與 `docs/current-status.md`；進度由主 agent 更新，你只在回報中列出每個細項的結果。"
 ],
 [
  "> 進度：[current-status.md](current-status.md)（需求進度表）｜ 架構：[architecture.md](architecture.md) ｜ 決策：[decisions.md](decisions.md) ｜ 工作規則：[../AGENTS.md](../AGENTS.md)",
  "> 進度：[plans/INDEX.md](plans/INDEX.md)（計畫索引）｜ 專案快照：[current-status.md](current-status.md) ｜ 架構：[architecture.md](architecture.md) ｜ 決策：[decisions.md](decisions.md) ｜ 工作規則：[../AGENTS.md](../AGENTS.md)"
 ],
 [
  "每一步在 `docs/plan/` 有一份計畫檔（範本 [plan/_template.md](plan/_template.md)），做法依 `project-rules.md` 的 Step Protocol。步驟的狀態寫在 `docs/current-status.md` 的需求進度表，本表只放順序與依賴。",
  "每一步是一個計畫，資料夾在 `docs/plans/<狀態>/<ID>-<slug>/`（範本 [plans/_template/](plans/_template/)），做法依 `project-rules.md` 的 Step Protocol。步驟的狀態寫在該計畫的 `plan.md` 與 [plans/INDEX.md](plans/INDEX.md)，本表只放順序與依賴。"
 ],
 [
  "適用範圍：Step Protocol 的步驟與進度表中類型為「功能」的需求；小修正免。範本在 [steps/_template/](steps/_template/)。",
  "適用範圍：Step Protocol 的步驟與類型為「功能」的計畫；小修正免。範本在 [steps/_template/](steps/_template/)。"
 ],
 [
  "  B -- 是 --> D[\"更新需求進度表、交接單 → commit → merge --no-ff → tag step-NN-done\"]",
  "  B -- 是 --> D[\"更新計畫（細項表、status、changelog、INDEX）與交接單 → commit → merge --no-ff → tag step-NN-done\"]"
 ],
 [
  "為什麼每步開新 session：每個步驟的上下文很大且彼此獨立；新 session 只靠 `AGENTS.md`、`docs/current-status.md` 與該步 plan 檔就能接手，不被前一步的雜訊拖累。",
  "為什麼每步開新 session：每個步驟的上下文很大且彼此獨立；新 session 只靠 `AGENTS.md`、`docs/plans/INDEX.md` 與該步計畫的 `plan.md`、`status.md` 就能接手，不被前一步的雜訊拖累。"
 ]
];
const templateEdits = new Map(); // 檔案 → 新內容
let templateLineCount = 0;
for (const file of TEMPLATE_FILES) {
  if (!exists(file)) continue;
  const original = rewrites.get(file) ?? fs.readFileSync(abs(file), "utf8");
  const eol = original.includes("\r\n") ? "\r\n" : "\n";
  let text = "\n" + original.split("\r\n").join("\n") + "\n";
  for (const [from, to] of TEMPLATE_LINES) {
    const needle = "\n" + from + "\n";
    if (!text.includes(needle)) continue;
    text = text.split(needle).join("\n" + to + "\n");
    templateLineCount++;
  }
  text = text.slice(1, -1).split("\n").join(eol);
  if (text !== original) templateEdits.set(file, text);
}

// ---- 3. 舊進度表：未完成的需求與下一個編號 ----
const statusSource = moves.some((m) => m.from === "docs/current-status.md") ? "docs/current-status.md" : legacyStatus ? "docs/legacy/current-status.md" : null;
const unfinished = [];
let maxR = 0;
if (statusSource) {
  const text = fs.readFileSync(abs(statusSource), "utf8");
  const section = text.split(/^##\s+需求進度表/m)[1] || "";
  for (const line of section.split(/\r?\n/)) {
    if (/^##\s/.test(line)) break;
    const cells = line.trim().replace(/^\||\|$/g, "").split("|").map((c) => c.trim());
    if (cells.length < 4 || !/^[A-Z]+\d+(\.\d+)?$/.test(cells[0])) continue;
    const m = /^R(\d+)/.exec(cells[0]);
    if (m) maxR = Math.max(maxR, Number(m[1]));
    const done = /✅|⛔/.test(cells[3]);
    if (!done) unfinished.push(`${cells[0]}  ${cells[3]}  ${cells[2].slice(0, 70)}`);
  }
}
const nextId = `R${String(maxR + 1).padStart(2, "0")}`;

// ---- 4. 執行 ----
function run(desc, fn) {
  log(`${dryRun ? "DRYRUN" : "DO    "} ${desc}`);
  if (!dryRun) fn();
}
for (const d of ["docs/plans/active", "docs/plans/completed", "docs/plans/cancelled"]) {
  if (!exists(d)) run(`mkdir ${d}`, () => fs.mkdirSync(abs(d), { recursive: true }));
}
for (const r of removeFiles) {
  run(`remove ${r} (3.0.0 template: docs/plans/_template/)`, () => {
    if (tracked(r)) execFileSync("git", ["-C", root, "rm", "-q", r]);
    else fs.rmSync(abs(r));
  });
}
for (const m of moves) {
  run(`move ${m.from} → ${m.to}${isGit && tracked(m.from) ? " (git mv)" : ""}`, () => {
    fs.mkdirSync(path.dirname(abs(m.to)), { recursive: true });
    if (isGit && tracked(m.from)) execFileSync("git", ["-C", root, "mv", m.from, m.to]);
    else fs.renameSync(abs(m.from), abs(m.to));
  });
}
log(`${dryRun ? "DRYRUN" : "DO    "} rewrite ${linkCount} link(s) in ${rewrites.size} file(s)`);
for (const [file, text] of rewrites) {
  log(`         ${file}`);
  if (!dryRun) fs.writeFileSync(abs(file), text);
}
log(`${dryRun ? "DRYRUN" : "DO    "} replace ${templateLineCount} 2.0.0 template line(s) with the 3.0.0 wording in ${templateEdits.size} file(s)`);
for (const [file, text] of templateEdits) {
  log(`         ${file}`);
  if (!dryRun) fs.writeFileSync(abs(file), text);
}
for (const d of ["docs/plans/active", "docs/plans/completed", "docs/plans/cancelled"]) {
  if (dryRun) continue;
  if (fs.readdirSync(abs(d)).length === 0) fs.writeFileSync(abs(d + "/.gitkeep"), "");
}
if (!exists("docs/current-status.md") || moves.some((m) => m.from === "docs/current-status.md")) {
  run("write docs/current-status.md (project-level snapshot, fill TODO(migrate))", () =>
    fs.writeFileSync(abs("docs/current-status.md"), [
      "# 目前狀態（專案層級）",
      "",
      "> 原地修改，保持精簡。只寫**整個專案**的快照：階段、跨計畫的已知問題與待使用者決定。",
      "> 各計畫的細項、進度與變更紀錄在自己的資料夾，入口是 [`plans/INDEX.md`](plans/INDEX.md)；不要把計畫的細節寫進本檔（規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」）。",
      `> 3.0.0 之前的需求進度表與目前狀態封存在 [legacy/current-status.md](legacy/current-status.md)（只讀）。`,
      "",
      `- 更新日期：${today}`,
      "- 階段：TODO(migrate): 一句話說明整個專案現在在哪個階段",
      "",
      "## 計畫",
      "",
      "- 所有計畫與狀態：[`plans/INDEX.md`](plans/INDEX.md)。",
      "",
      "## 跨計畫的已知問題",
      "",
      "- TODO(migrate): 從舊的 current-status.md 搬過來（只屬於某個計畫的放進那個計畫的 status.md）",
      "",
      "## 待使用者決定（跨計畫）",
      "",
      "- TODO(migrate): 同上",
      "",
    ].join("\n")));
}
if (!exists("docs/plans/INDEX.md")) {
  run(`write docs/plans/INDEX.md (next free number ${nextId})`, () =>
    fs.writeFileSync(abs("docs/plans/INDEX.md"), [
      "# 計畫索引",
      "",
      "> 所有計畫的單一入口（AI 結構 3.0.0）。只做導覽：每個計畫一列，細項與進度在各計畫的資料夾裡。",
      "> 只在新增計畫或計畫層級的狀態改變時更新。規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」；新計畫從 [`_template/`](_template/) 複製。",
      "> 3.0.0 之前已完成的需求記在 [`../legacy/current-status.md`](../legacy/current-status.md) 的需求進度表（只讀），這裡只列遷移時還沒完成的需求與之後的新計畫。",
      "",
      `下一個可用編號：**${nextId}**`,
      "",
      "| 編號 | 類型 | 計畫 | 狀態 | 位置 | 依賴／相關 | 分支 |",
      "|---|---|---|---|---|---|---|",
      "",
      "圖例：⬜ 規劃中　⏳ 進行中　⚠️ 受阻／暫停　✅ 完成　⛔ 取消。新的計畫加在表格最上方。",
      "",
    ].join("\n")));
}

// ---- 5. 報告 ----
log("");
log(`Unfinished rows in the old requirements table (${statusSource ?? "none"}): turn each top-level ID into docs/plans/active/<ID>-<slug>/ (plan.md, status.md, changelog.md) and add it to INDEX.md:`);
for (const u of unfinished) log("  " + u);
if (unfinished.length === 0) log("  (none)");
log("");
log("Text that still describes the 2.x layout (project-owned files: rewrite by hand, see README \"Upgrade 2.x to 3.0.0\"):");
const legacyPattern = /需求進度表|changelog-ai\.md|docs\/plan\/(?!s)|docs\/plan`|`plan\/|「需求展開與進度表」|plan 檔/;
let hits = 0;
for (const fileOld of mdFiles) {
  const file = mapPath(fileOld);
  if (removeFiles.includes(fileOld) || file.startsWith("docs/legacy/") || file.startsWith("docs/plans/_template/") || file === ".agents/harness/engineering-workflow.md" ||
      file.startsWith(".agents/skills/ai-handoff/") || file.startsWith(".claude/skills/ai-handoff/")) continue;
  const text = templateEdits.get(file) ?? rewrites.get(file) ?? (exists(file) ? fs.readFileSync(abs(file), "utf8") : fs.readFileSync(abs(fileOld), "utf8"));
  text.split(/\r?\n/).forEach((line, i) => {
    if (legacyPattern.test(line)) {
      hits++;
      log(`  ${file}:${i + 1}: ${line.trim().slice(0, 110)}`);
    }
  });
}
if (hits === 0) log("  (none)");
log("");
log(dryRun ? "Dry run: nothing was written." : "Done. Next: create the active plans listed above, rewrite the reported text, fill TODO(migrate), then run scripts/ai/check-ai-structure.ps1.");
