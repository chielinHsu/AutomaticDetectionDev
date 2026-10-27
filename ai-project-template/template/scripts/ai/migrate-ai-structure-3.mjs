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
//   4. 新的 docs/current-status.md（專案層級的快照）與 docs/plans/INDEX.md（下一個可用編號由舊的進度表推算）。
//   5. 報告：舊進度表裡還沒完成的需求（要拆成計畫），以及還在講 2.x 做法的文字（要改寫）。
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
const legacyPattern = /需求進度表|changelog-ai\.md|docs\/plan\/(?!s)|docs\/plan`|`plan\/|「需求展開與進度表」/;
let hits = 0;
for (const fileOld of mdFiles) {
  const file = mapPath(fileOld);
  if (removeFiles.includes(fileOld) || file.startsWith("docs/legacy/") || file.startsWith("docs/plans/_template/") || file === ".agents/harness/engineering-workflow.md" ||
      file.startsWith(".agents/skills/ai-handoff/") || file.startsWith(".claude/skills/ai-handoff/")) continue;
  const text = rewrites.get(file) ?? (exists(file) ? fs.readFileSync(abs(file), "utf8") : fs.readFileSync(abs(fileOld), "utf8"));
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
