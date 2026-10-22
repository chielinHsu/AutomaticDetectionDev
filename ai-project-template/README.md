# AI 專案共用結構（Codex 與 Claude Code）

版本 2.0.0。目標：任何新資料夾，不論用 Claude Code 或 Codex 開發，都套用同一份規範，只維護一份知識，兩個平台各自的入口檔只做轉接。2.0.0 把 AutoCadCreated 實際在用的整套 AI 結構（入口、工作流程、SOLID 與四種輸出、Step Protocol、Commit 規則、ADR、總體計畫、模型路由）加上「需求進度表」整理成範本；所有文件與回覆一律使用繁體中文（zh-TW）。

## 修改紀錄

| 日期 | 版本 | 說明 |
|---|---|---|
| 2026-10-21 | 2.0.0 | 整套照搬 AutoCadCreated 的 AI 結構：入口改為「不可省略的四條」（加繁中回覆與完成總結、需求進度表）；新增 `MODEL_ROUTING.md`、`.claude/agents/`、`.claude/hooks/`、`docs/00-master-plan.md`、`docs/plan/_template.md`、`docs/steps/_template/`、`docs/adr/`、`tools/ste-lint.mjs`、`scripts/ai/check-model-routing.mjs`、`bench-model-routing.mjs`；`engineering-workflow.md` 新增「需求展開與進度表」；`current-status.md` 開頭為需求進度表；`decisions.md` 改為索引；`check-ai-structure.ps1` 檢查 2.0.0 的必要檔案與規則 |
| 2026-10-05 | 1.0.0 | 初版：薄入口、共用工作流程、`ai-handoff` skill、結構檢查 |

## 用法

```powershell
powershell -ExecutionPolicy Bypass -File <本資料夾>\New-AiProject.ps1 -Path D:\Work\MyApp
```

| 參數 | 作用 |
|---|---|
| `-Name "My App"` | 專案名稱，預設用資料夾名稱 |
| `-DryRun` | 只列出會做什麼，不寫入 |
| `-Refresh` | 同時更新「範本管理」的檔案（見下方維護規則） |

腳本會：複製範本、替換 `{{PROJECT_NAME}}` 與 `{{DATE}}`、補上 `.gitignore` 缺的行、把 skill 鏡像到 `.claude/skills/`、跑一次結構檢查。它不會 `git init`、不會 commit，也不會覆蓋任何已存在的檔案（`-Refresh` 只覆蓋範本管理的檔案）。複製 `.ps1` 時一律寫成 UTF-8 with BOM，避免 Windows PowerShell 5.1 把中文解析成亂碼。

建立後的第一件事：填寫 `AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`MODEL_ROUTING.md`「本專案案例」、`docs/00-master-plan.md`、`docs/architecture.md` 裡所有 `TODO(project)`，用 `scripts/ai/check-ai-structure.ps1 -Strict` 確認都填完了（範本剛建立時 `-Strict` 預期只會因為 TODO 而失敗）。

本資料夾可以整個搬到任何位置，腳本是用自己所在的位置找 `template/` 的。

## 結構

```text
<project>/
├─ AGENTS.md                      入口（Codex 讀；Claude 經 CLAUDE.md 匯入）：不可省略的四條、必讀清單
├─ CLAUDE.md                      入口（Claude 讀）：匯入 AGENTS.md、AI_CONTEXT.md、工作流程、專案規則、MODEL_ROUTING.md
├─ AI_CONTEXT.md                  專案事實（用途、技術棧、目錄、建置與測試、限制、Context Routing）
├─ MODEL_ROUTING.md               Claude 專用：五級模型與推理強度路由、「本專案案例」（專案填）
├─ .agents/
│  ├─ harness/
│  │  ├─ engineering-workflow.md  通用工作流程，含「需求展開與進度表」（範本管理）
│  │  ├─ project-rules.md         專案規則：風格、SOLID、四種輸出、Step Protocol、Git、Commit 訊息、文件、驗證
│  │  ├─ skill-sync.json          skill 鏡像規則
│  │  └─ TEMPLATE_VERSION         2.0.0（範本管理）
│  └─ skills/<name>/SKILL.md      skill 的來源（Codex 直接讀）；內建 ai-handoff（範本管理）
├─ .claude/
│  ├─ settings.json               權限護欄（deny git add -A / .，ask push 與破壞性指令）、UserPromptSubmit hook
│  ├─ agents/                     fast-worker、developer、developer-deep、architect、architect-deep
│  ├─ hooks/model_router.mjs      依需求關鍵字建議 agent、model、effort（只是建議）
│  └─ skills/<name>/SKILL.md      鏡像複本（由腳本產生，不手改）
├─ .ai/handoff.md                 交接單（覆寫式）
├─ docs/
│  ├─ 00-master-plan.md           總體計畫：要做什麼、不做什麼、架構總覽、步驟清單
│  ├─ architecture.md             現在的結構與依賴規則
│  ├─ decisions.md                設計決策「索引」（內容在 adr/）
│  ├─ adr/                        000-ai-agent-structure.md、_template.md（範本管理），之後每個決策一檔
│  ├─ current-status.md           開頭是「需求進度表」，之後是目前進度與已知問題
│  ├─ plan/_template.md           跨 session 需求的計畫檔範本（範本管理）
│  ├─ steps/_template/            四種輸出的範本：spec、diagram、explainer.html、perf、storyboard（範本管理）
│  └─ changelog-ai.md             每個 agent session 的紀錄（只新增）
├─ tools/ste-lint.mjs             STE 句長與被動語態檢查（範本管理）
└─ scripts/ai/
   ├─ sync-skills.ps1             .agents/skills → .claude/skills（範本管理）
   ├─ check-ai-structure.ps1      結構檢查（範本管理）
   ├─ check-model-routing.mjs     模型路由設定檢查（專案擁有；通用範例，專案可調整）
   └─ bench-model-routing.mjs     模型路由實測 verify／bench（專案擁有；會花額度，沒有 --yes 只印計畫）
```

## 各檔職責（互不重疊）

| 檔案 | 回答的問題 | 變動頻率 | 寫入方式 |
|---|---|---|---|
| `AGENTS.md`、`CLAUDE.md` | 開工先讀什麼、哪四條不能省 | 幾乎不變 | 原地修改 |
| `AI_CONTEXT.md` | 這個專案是什麼、怎麼建置與驗證 | 少 | 原地修改 |
| `MODEL_ROUTING.md` | 什麼工作交給哪一級模型與推理強度 | 少 | 原地修改（專案填案例） |
| `engineering-workflow.md` | agent 怎麼工作、需求怎麼展開（所有專案相同） | 範本升級時 | 範本管理 |
| `project-rules.md` | 這個專案的慣例：SOLID、四種輸出、Step Protocol、Git | 少 | 原地修改 |
| `docs/00-master-plan.md` | 整個專案做什麼、分幾步 | 少 | 原地修改 |
| `docs/current-status.md` | 需求做到哪、還剩什麼 | 每次收工 | 原地修改，需求進度表的列不刪除 |
| `docs/plan/<ID>-<slug>.md` | 跨 session 需求的計畫、完成定義、Start prompt | 開工前建立 | 原地修改 |
| `docs/adr/NNN-slug.md`、`docs/decisions.md` | 為什麼這樣設計 | 有決策時 | 只新增；索引新的在上 |
| `docs/steps/<ID>-<slug>/` | 功能類步驟的四種輸出與 perf | 每個功能步驟 | 新增 |
| `docs/changelog-ai.md` | 上一個 agent 做了什麼 | 每次收工 | 只新增，新的在上 |
| `.ai/handoff.md` | 下一位 agent 第一件要知道的事 | 每次收工 | 整份覆寫 |

入口檔裡只有四條「不可省略」的規則（繁中回覆與完成總結、不虛構驗證、不擅自 commit 與 stage 要明確列路徑、非單純需求先展開進度表）重複出現一行，因為簡單任務時 agent 可能不會往下讀。其他共通內容都只存在一處。

## 需求進度表

`docs/current-status.md` 開頭的表格是需求的單一事實來源：

```markdown
| 編號 | 類型 | 需求／細項 | 狀態 | 計畫 | 分支／commit | 驗證 |
|---|---|---|---|---|---|---|
| R01 | 功能 | 某個功能 | ✅ | [R01](plan/R01-slug.md) | `feat/slug` abc1234 | VERIFIED |
| R01.1 | | core：某個細項 | ✅ | | | 單元測試 |
| R02 | 修正 | … | ⬜ | | | |
```

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消。非單純需求在實作前先展開成細項（⬜）；完成的列不刪除，改狀態並補 commit 與驗證；待使用者決定的事列在表格下方。architect 系列 subagent 只回傳計畫，由主 agent 寫進進度表與 `docs/plan/`。四種輸出只適用於 Step Protocol 的步驟與類型為「功能」的需求，小修正（fix、docs、chore）免。

## 兩個平台怎麼讀（已查證的事實）

| 項目 | Claude Code | Codex |
|---|---|---|
| 入口檔 | `CLAUDE.md`。沒有 CLAUDE.md 時才自動讀 `AGENTS.md`（v2.1.277 以上）；兩者都有時只讀 CLAUDE.md | `AGENTS.md`，從 git 根目錄往下到工作目錄逐層合併 |
| 匯入其他檔案 | 支援，`@path` 寫在獨立一行 | **不支援**，所以 AGENTS.md 用「必讀清單」 |
| 大小限制 | 官方建議保持精簡（越具體簡短，遵守度越高） | 合併後預設上限 32 KiB，超過就不再加入 |
| Skill 位置 | `.claude/skills/` | `.agents/skills/`（repo 內），不讀 `.claude/skills/` |
| `.agents/` 下的內容 | 不會自動讀 | 只有 skill 會被掃描，`.agents/harness/` 不會自動載入 |
| 模型路由（`MODEL_ROUTING.md`、`.claude/agents/`、hook） | 使用 | 不讀 |

因此設計成：CLAUDE.md 一定存在，並用 `@AGENTS.md` 匯入。官方說明這樣不會重複載入，而且在讀不到 AGENTS.md 的舊版或 Bedrock 環境也能運作。Skill 因為兩邊不共用目錄，只能用複本，所以 `check-ai-structure.ps1` 會檢查兩份是否一致。

來源：<https://code.claude.com/docs/en/memory>、<https://learn.chatgpt.com/docs/agent-configuration/agents-md>、<https://learn.chatgpt.com/docs/build-skills>。

## 來源

1.0.0 由兩份來源合併而成（細節見 git 歷史中 1.0.0 的 README）：ChatGPT 文章（<https://chatgpt.com/share/6ac21911-ddf4-83ee-b3b6-8fdb31144456>）的「入口檔是 bootloader、共用知識集中一份、四份 docs、交接單」，以及 AlgoParamFineTune 的工作流程（證據分級、驗證三種結果、決策審查、修改前後流程圖、skill 來源與鏡像）；該專案綁 MSBuild 與 AOI 模組的 verify、review map、專屬 skill 都沒有帶入。2.0.0 再加入 AutoCadCreated 實際運作過的部分：SOLID 與四種輸出、Step Protocol、ADR 資料夾、總體計畫與計畫檔、模型路由，並新增需求進度表；AOI／HALCON／晶格等專屬內容都已去除，改為 `TODO(project)` 由專案填寫。

## 檢查腳本

| 指令 | 檢查什麼 |
|---|---|
| `scripts/ai/check-ai-structure.ps1` | 2.0.0 必要檔案、`TEMPLATE_VERSION`（主版本 2）、`AGENTS.md` 的必讀清單與四條規則關鍵字、`CLAUDE.md` 的匯入、`current-status.md` 有「需求進度表」、`decisions.md` 是索引且 `docs/adr/` 存在、未取代的 `{{…}}` 佔位、skill 鏡像一致；`MODEL_ROUTING.md` 存在時另查五個 agent、hook 接線並呼叫 `check-model-routing.mjs`。`-Strict` 讓未填的 `TODO(project):` 也算失敗 |
| `scripts/ai/sync-skills.ps1`（`-Check`） | `.agents/skills` → `.claude/skills` 鏡像；`-Check` 只比對 |
| `node scripts/ai/check-model-routing.mjs` | 五個 subagent 的 model、effort、工具邊界；settings 與 hook；通用範例需求的分級；升降級；「本專案案例」逐列檢查（TODO 時只警告）。不呼叫 Claude、不花額度 |
| `node tools/ste-lint.mjs [檔案…]` | `<!--STE-->` 區塊的句長與被動語態；不給檔案時掃 `docs/`（跳過 `_template`） |
| `node scripts/ai/bench-model-routing.mjs verify --yes` | 會花額度（約 US$1）：實測五個 subagent 實際套用的 model 與 effort |

## 維護規則

**範本管理**（`-Refresh` 會覆蓋，不要在專案裡改）：`engineering-workflow.md`、`TEMPLATE_VERSION`、`.agents/skills/ai-handoff/`、`scripts/ai/check-ai-structure.ps1`、`scripts/ai/sync-skills.ps1`、`tools/ste-lint.mjs`、`docs/steps/_template/*`、`docs/plan/_template.md`、`docs/adr/_template.md`。
**專案擁有**（建立後永遠不會被覆蓋）：其餘所有檔案，包含 `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`project-rules.md`、`MODEL_ROUTING.md`、`.claude/`（agents、hook、settings）、`scripts/ai/check-model-routing.mjs`、`scripts/ai/bench-model-routing.mjs`。後兩者刻意不列入管理：它們含專案調好的案例與任務，覆蓋會洗掉專案的調整。

- 新增共用必讀文件：在 `AGENTS.md` 的清單加一行，在 `CLAUDE.md` 加 `@路徑`，執行 `check-ai-structure.ps1` 確認。
- 新增 skill：放進 `.agents/skills/<name>/SKILL.md`，執行 `sync-skills.ps1`。只給 Claude 用的 skill 放 `.claude/skills/`，並登記到 `skill-sync.json` 的 `claudeOnly`；只給 Codex 用的登記 `codexOnly`。
- 升級範本：改 `template/` 後遞增 `TEMPLATE_VERSION`，對各專案執行 `New-AiProject.ps1 -Path <專案> -Refresh`，再用 `check-ai-structure.ps1` 確認。
- 模型路由的環境變數：`AI_MODEL_ROUTER=off` 關掉 hook，`AI_MODEL_ROUTER_STATE_DIR` 改狀態資料夾，`AI_CLAUDE_BIN` 指定 `bench` 用的 claude 執行檔。已有自己環境變數名稱的專案（例如 `AOI_MODEL_ROUTER`）維持原狀即可。

### 套用到已有結構的專案

腳本不會動既有的 `AGENTS.md`、`CLAUDE.md` 與其他專案擁有的檔案，檢查會列出缺少的項目，請手動**合併**（保留專案既有的事實與特殊規則，補上 2.0.0 的通用段落；專案已調好的模型路由案例與檢查不要換掉）：

1. 先確認沒有進行中的分支或 worktree，並備份或確認沒有他人的未提交修改。
2. 若已有自己的狀態檔（如 `docs/STATUS.md`），在套用前先更名為 `docs/current-status.md`，再全專案掃描並更新舊引用，用區分大小寫的搜尋確認沒有殘留。
3. 把舊入口檔的內容搬到共用文件：專案事實進 `AI_CONTEXT.md`，結構與依賴規則進 `docs/architecture.md`，慣例與流程進 `project-rules.md`。舊入口檔改成薄入口，並補上「不可省略的四條」。
4. 若舊入口檔裡有「每個 session 都該看到」的護欄（如分層規則），把搬走後的文件加進 `AGENTS.md` 必讀清單與 `CLAUDE.md` 的 `@` 匯入，否則護欄會從常駐 context 消失。
5. `docs/decisions.md` 內嵌 ADR 的專案：拆成 `docs/adr/NNN-slug.md`（編號不變），`decisions.md` 改成索引（一行一筆，連到 ADR 檔）。
6. 建立 `docs/00-master-plan.md`（依 README、`AI_CONTEXT.md`、architecture 回填）與需求進度表（依 `docs/changelog-ai.md`、`git log` 回填已完成的需求，只填主要細項；原有的狀態段落保留在表格下方）。
7. 若專案的流程授權了 commit（例如每步在本機 commit），寫進 `project-rules.md` 的 Git 一節，並限定範圍。
8. 執行 `check-ai-structure.ps1 -Strict`、`sync-skills.ps1 -Check`、`node scripts/ai/check-model-routing.mjs`，再用 `claude -p` 實測匯入有載入。

## 已知限制

- **Codex 端沒有在本機端到端測過**：Codex 的行為依官方文件設計，並由檢查腳本驗證結構（含 32 KiB 上限）。Claude Code 端的匯入已在 AutoCadCreated 實測載入。
- `.claude/settings.json` 的 deny 規則是護欄，不是安全邊界。Claude Code 在新資料夾要先接受信任對話框，`permissions` 才會生效。
- Skill 鏡像是複製，不是連結（Windows 符號連結需要權限，git 設定也會影響）。所以改了來源要記得跑 `sync-skills.ps1`；忘記時 `check-ai-structure.ps1` 會擋。
- commit 格式結尾的 `[Test]` 與測試結果行是使用者的固定規則，沒有建置的專案也照寫，所以它不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊，且必須如實。
- 模型路由 hook 只用關鍵字評分，是建議不是判斷；範本的 `TOPICS` 是通用的（架構、並行、安全、不可逆資料操作、跨模組重構、效能、根因不明、建置系統），專案若有特殊的高風險主題，要自己加。`check-model-routing.mjs` 的通用範例只驗證 hook 的關鍵字邏輯；`verify` 才能證明 Claude Code 實際套用了 model 與 effort（會花額度，範本沒有執行過）。
- 腳本只用 Windows PowerShell 5.1 測過，沒有在 PowerShell 7 與 macOS／Linux 上測。`check-ai-structure.ps1` 在 PowerShell 內呼叫 `node`；找不到 `node` 時只給警告。
- `check-ai-structure.ps1` 對 `current-status.md` 的長度上限是 150 行（警告），因為需求進度表會讓它變長；`.ai/handoff.md` 維持 60 行。
