# AI 專案共用結構（Codex 與 Claude Code）

版本 3.0.0。目標：任何新資料夾，不論用 Claude Code 或 Codex 開發，都套用同一份規範，只維護一份知識，兩個平台各自的入口檔只做轉接。2.0.0 把 AutoCadCreated 實際在用的整套 AI 結構（入口、工作流程、SOLID 與四種輸出、Step Protocol、Commit 規則、ADR、總體計畫、模型路由）整理成範本；3.0.0 把「所有需求共用一份進度表與 changelog」改成**每個計畫一個資料夾**（專案 → 計畫 → 細項 → 歷史）。所有文件與回覆一律使用繁體中文（zh-TW）。

## 修改紀錄

| 日期 | 版本 | 說明 |
|---|---|---|
| 2026-10-27 | 3.0.0 | 計畫分開管理：`docs/plans/INDEX.md`（計畫索引）＋ `docs/plans/<active｜completed｜cancelled>/<ID>-<slug>/`（`plan.md` 的細項表、`status.md` 快照、`changelog.md`、選用的 `tasks/`）；`docs/current-status.md` 改成專案層級的快照；`docs/changelog-ai.md` 與 `docs/plan/` 移除（舊專案封存到 `docs/legacy/`）；入口第 4 條、`engineering-workflow.md`「計畫分開管理」、`ai-handoff` skill、architect／developer 的定義檔改寫；`check-ai-structure.ps1` 檢查計畫索引與資料夾、搬動後的連結；新增 `scripts/ai/migrate-ai-structure-3.mjs`（2.x 升級用）；`New-AiProject.ps1 -Refresh` 不再把專案擁有的檔案與新專案的範例計畫複製進已有結構的專案 |
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

腳本會：複製範本、替換 `{{PROJECT_NAME}}` 與 `{{DATE}}`、補上 `.gitignore` 缺的行、把 skill 鏡像到 `.claude/skills/`、跑一次結構檢查。它不會 `git init`、不會 commit，也不會覆蓋任何已存在的檔案（`-Refresh` 只覆蓋範本管理的檔案）。目標資料夾已經有 `AGENTS.md`（已有 AI 結構）時，只新增或更新範本管理的檔案，其他缺少的檔案列為 `MISS` 讓人決定要不要手動合併，新專案的範例計畫（`INDEX.md`、R01、R02）不會複製。複製 `.ps1` 時一律寫成 UTF-8 with BOM，避免 Windows PowerShell 5.1 把中文解析成亂碼。

建立後的第一件事：計畫 R02（`docs/plans/active/R02-fill-project-todos/`）——填寫 `AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`MODEL_ROUTING.md`「本專案案例」、`docs/00-master-plan.md`、`docs/architecture.md` 裡所有 `TODO(project)`，用 `scripts/ai/check-ai-structure.ps1 -Strict` 確認都填完了（範本剛建立時 `-Strict` 預期只會因為 TODO 而失敗）。

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
│  │  ├─ engineering-workflow.md  通用工作流程，含「計畫分開管理」（範本管理）
│  │  ├─ project-rules.md         專案規則：風格、SOLID、四種輸出、Step Protocol、Git、Commit 訊息、文件、驗證
│  │  ├─ skill-sync.json          skill 鏡像規則
│  │  └─ TEMPLATE_VERSION         3.0.0（範本管理）
│  └─ skills/<name>/SKILL.md      skill 的來源（Codex 直接讀）；內建 ai-handoff（範本管理）
├─ .claude/
│  ├─ settings.json               權限護欄（deny git add -A / .，ask push 與破壞性指令）、UserPromptSubmit hook
│  ├─ agents/                     fast-worker、developer、developer-deep、architect、architect-deep
│  ├─ hooks/model_router.mjs      依需求關鍵字建議 agent、model、effort（只是建議）
│  └─ skills/<name>/SKILL.md      鏡像複本（由腳本產生，不手改）
├─ .ai/handoff.md                 交接單（覆寫式）：上一個 session 動了哪些計畫、未提交的變更、不要動的東西
├─ docs/
│  ├─ 00-master-plan.md           總體計畫：要做什麼、不做什麼、架構總覽、步驟清單
│  ├─ architecture.md             現在的結構與依賴規則
│  ├─ decisions.md                設計決策「索引」（內容在 adr/，跨計畫共用）
│  ├─ adr/                        000-ai-agent-structure.md、_template.md（範本管理），之後每個決策一檔
│  ├─ current-status.md           專案層級的快照：階段、跨計畫的已知問題與待使用者決定（不放計畫細節）
│  ├─ plans/
│  │  ├─ INDEX.md                 計畫索引：每個計畫一列、下一個可用編號
│  │  ├─ _template/               plan.md、status.md、changelog.md、tasks/_task.md（範本管理）
│  │  ├─ active/<ID>-<slug>/      規劃中、進行中、受阻的計畫：plan.md、status.md、changelog.md、tasks/
│  │  ├─ completed/<ID>-<slug>/   完成的計畫（整個資料夾搬過來）
│  │  └─ cancelled/<ID>-<slug>/   取消的計畫
│  ├─ steps/_template/            四種輸出的範本：spec、diagram、explainer.html、perf、storyboard（範本管理）
│  └─ legacy/                     （2.x 升級的專案才有）舊的 current-status.md、changelog-ai.md、plan/，只讀
├─ tools/ste-lint.mjs             STE 句長與被動語態檢查（範本管理）
└─ scripts/ai/
   ├─ sync-skills.ps1             .agents/skills → .claude/skills（範本管理）
   ├─ check-ai-structure.ps1      結構檢查（範本管理）
   ├─ migrate-ai-structure-3.mjs  2.x → 3.0.0 的機械性遷移（範本管理）
   ├─ check-model-routing.mjs     模型路由設定檢查（專案擁有；通用範例，專案可調整）
   └─ bench-model-routing.mjs     模型路由實測 verify／bench（專案擁有；會花額度，沒有 --yes 只印計畫）
```

## 各檔職責（互不重疊）

| 檔案 | 回答的問題 | 變動頻率 | 寫入方式 |
|---|---|---|---|
| `AGENTS.md`、`CLAUDE.md` | 開工先讀什麼、哪四條不能省 | 幾乎不變 | 原地修改 |
| `AI_CONTEXT.md` | 這個專案是什麼、怎麼建置與驗證 | 少 | 原地修改 |
| `MODEL_ROUTING.md` | 什麼工作交給哪一級模型與推理強度 | 少 | 原地修改（專案填案例） |
| `engineering-workflow.md` | agent 怎麼工作、需求怎麼歸到計畫（所有專案相同） | 範本升級時 | 範本管理 |
| `project-rules.md` | 這個專案的慣例：SOLID、四種輸出、Step Protocol、Git | 少 | 原地修改 |
| `docs/00-master-plan.md` | 整個專案做什麼、分幾步 | 少 | 原地修改 |
| `docs/current-status.md` | 整個專案在哪個階段、有哪些跨計畫的問題與待決定 | 偶爾 | 原地修改，保持精簡 |
| `docs/plans/INDEX.md` | 有哪些計畫、各在哪裡、什麼狀態、下一個編號 | 新增計畫或計畫的狀態改變時 | 原地修改，一個計畫一列 |
| `…/<ID>-<slug>/plan.md` | 這個計畫要做什麼、細項做到哪（細項表是單一事實來源） | 開工前建立；細項變動時 | 原地修改，完成的列不刪除 |
| `…/<ID>-<slug>/status.md` | 這個計畫現在卡在哪、下一步、換人時要知道什麼 | 每次有實質進度 | 原地修改（快照） |
| `…/<ID>-<slug>/changelog.md` | 這個計畫發生過哪些變更與驗證 | 每次收工 | 只新增，新的在上 |
| `docs/adr/NNN-slug.md`、`docs/decisions.md` | 為什麼這樣設計 | 有決策時 | 只新增；索引新的在上 |
| `docs/steps/<ID>-<slug>/` | 功能類步驟的四種輸出與 perf | 每個功能步驟 | 新增 |
| `.ai/handoff.md` | 下一位 agent 第一件要知道的事 | 每次收工 | 整份覆寫 |

入口檔裡只有四條「不可省略」的規則（繁中回覆與完成總結、不虛構驗證、不擅自 commit 與 stage 要明確列路徑、非單純需求先歸到計畫並展開細項）重複出現一行，因為簡單任務時 agent 可能不會往下讀。其他共通內容都只存在一處。

## 計畫分開管理（3.0.0）

規則的全文在 `engineering-workflow.md`「計畫分開管理」，這裡只摘要：

- **路由**：開工先讀 `docs/plans/INDEX.md`，判斷需求屬於哪個計畫；無關的需求開新計畫，已完成計畫的後續也開新計畫（「相關」連回原計畫），不重新打開舊的 changelog；分不出來就問使用者。
- **只讀、只寫一個計畫**：接手時只讀那個計畫的 `plan.md` 與 `status.md`（要歷史才讀 `changelog.md`）；收工時只更新它。動到別的計畫管的介面時，只在那個計畫的 `changelog.md` 記一筆受影響的內容並互相連結。
- **細項表是單一事實來源**，`status.md` 是快照（不重抄細項、不累積歷史），`changelog.md` 只記這個計畫。
- **`INDEX.md` 只做導覽**，只在新增計畫或計畫層級的狀態改變時更新，減少多個 session 同時工作時的衝突。並行的工作仍建議用 git 分支或 worktree 隔離。
- **狀態**：⬜ 規劃中 → ⏳ 進行中 →（⚠️ 受阻）→ ✅ 完成；⛔ 取消。完成或取消時整個資料夾搬到 `completed/`、`cancelled/`，`check-ai-structure.ps1` 會擋下斷掉的連結。
- **編號**：計畫 `Rnn`（整個專案一個序列，下一個可用編號寫在 `INDEX.md`；照總體計畫分步驟的專案，步驟 `Snn` 也是計畫），細項 `<ID>.<m>`。2.x 用過的編號照舊。

範例（`INDEX.md`）：

```markdown
下一個可用編號：**R08**

| 編號 | 類型 | 計畫 | 狀態 | 位置 | 依賴／相關 | 分支 |
|---|---|---|---|---|---|---|
| R07 | 功能 | QR Code 解鎖 | ⏳ 進行中 | [active/R07-qr-unlock](active/R07-qr-unlock/plan.md) | R06 | `feat/qr-unlock` |
| R06 | 功能 | AES-256 加密 | ✅ 完成 | [completed/R06-aes](completed/R06-aes/plan.md) | | `feat/aes` |
```

architect 系列 subagent 只回傳計畫，由主 agent 寫進計畫資料夾與 `INDEX.md`。四種輸出只適用於 Step Protocol 的步驟與類型為「功能」的計畫，小修正（fix、docs、chore）免。

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

1.0.0 由兩份來源合併而成（細節見 git 歷史中 1.0.0 的 README）：ChatGPT 文章（<https://chatgpt.com/share/6ac21911-ddf4-83ee-b3b6-8fdb31144456>）的「入口檔是 bootloader、共用知識集中一份、四份 docs、交接單」，以及 AlgoParamFineTune 的工作流程（證據分級、驗證三種結果、決策審查、修改前後流程圖、skill 來源與鏡像）；該專案綁 MSBuild 與 AOI 模組的 verify、review map、專屬 skill 都沒有帶入。2.0.0 再加入 AutoCadCreated 實際運作過的部分：SOLID 與四種輸出、Step Protocol、ADR 資料夾、總體計畫與計畫檔、模型路由，並新增需求進度表；AOI／HALCON／晶格等專屬內容都已去除，改為 `TODO(project)` 由專案填寫。3.0.0 依 ChatGPT 的「架構分類建議」（<https://chatgpt.com/share/6ae038fd-38b8-83ec-a2f0-10803a8a52a3>）把全域的進度表與 changelog 改成「專案 → 計畫 → 細項 → 歷史」：計畫索引、每個計畫自己的 PLAN／狀態快照／變更紀錄／子任務、完成封存、路由規則、跨計畫的關聯、文件一致性檢查。沒有照搬的部分：檔名與編號沿用 2.x 的慣例（`Rnn`、小寫的 `plan.md`、`status.md`、`changelog.md`），工作流程仍在 `.agents/harness/`（不另建 `.ai/WORKFLOW.md`），檢查用既有的 PowerShell 腳本而不是 Python（這台機器的 `python.exe` 只是 Store 捷徑），每個計畫不另放 `DECISIONS.md`（跨計畫的決策仍用 `docs/adr/`，計畫的 `plan.md` 有「決策」一節）。

## 檢查腳本

| 指令 | 檢查什麼 |
|---|---|
| `scripts/ai/check-ai-structure.ps1` | 3.0.0 必要檔案、`TEMPLATE_VERSION`（主版本 3）、`AGENTS.md` 的必讀清單與四條規則關鍵字、`CLAUDE.md` 的匯入、計畫（`INDEX.md` 每列的編號不重複、連到 `<狀態>/<ID>-<slug>/plan.md`、狀態和所在資料夾一致、下一個編號沒被用過；每個計畫資料夾都在索引裡、有三個檔、frontmatter 的 id 和資料夾一致；計畫裡、`current-status.md`、`handoff.md` 的相對連結都存在；殘留的 2.x 進度表 FAIL、`docs/changelog-ai.md` 與 `docs/plan/` WARN）、`decisions.md` 是索引且 `docs/adr/` 存在、未取代的 `{{…}}` 佔位、skill 鏡像一致；`MODEL_ROUTING.md` 存在時另查五個 agent、hook 接線並呼叫 `check-model-routing.mjs`。`-Strict` 讓未填的 `TODO(project):` 也算失敗 |
| `node scripts/ai/migrate-ai-structure-3.mjs`（`--dry-run`） | 2.x → 3.0.0 的機械性遷移（見「2.x 升級到 3.0.0」）：搬舊紀錄到 `docs/legacy/`、改寫連結、建立 `INDEX.md` 與新的快照、列出要拆的需求與要改寫的文字 |
| `scripts/ai/sync-skills.ps1`（`-Check`） | `.agents/skills` → `.claude/skills` 鏡像；`-Check` 只比對 |
| `node scripts/ai/check-model-routing.mjs` | 五個 subagent 的 model、effort、工具邊界；settings 與 hook；通用範例需求的分級；升降級；「本專案案例」逐列檢查（TODO 時只警告）。不呼叫 Claude、不花額度 |
| `node tools/ste-lint.mjs [檔案…]` | `<!--STE-->` 區塊的句長與被動語態；不給檔案時掃 `docs/`（跳過 `_template`） |
| `node scripts/ai/bench-model-routing.mjs verify --yes` | 會花額度（約 US$1）：實測五個 subagent 實際套用的 model 與 effort |

## 維護規則

**範本管理**（`-Refresh` 會覆蓋，不要在專案裡改）：`engineering-workflow.md`、`TEMPLATE_VERSION`、`.agents/skills/ai-handoff/`、`scripts/ai/check-ai-structure.ps1`、`scripts/ai/sync-skills.ps1`、`tools/ste-lint.mjs`、`scripts/ai/migrate-ai-structure-3.mjs`、`docs/steps/_template/*`、`docs/plans/_template/*`、`docs/adr/_template.md`。
**專案擁有**（建立後永遠不會被覆蓋）：其餘所有檔案，包含 `AGENTS.md`、`CLAUDE.md`、`AI_CONTEXT.md`、`project-rules.md`、`MODEL_ROUTING.md`、`.claude/`（agents、hook、settings）、`scripts/ai/check-model-routing.mjs`、`scripts/ai/bench-model-routing.mjs`。後兩者刻意不列入管理：它們含專案調好的案例與任務，覆蓋會洗掉專案的調整。

- 新增共用必讀文件：在 `AGENTS.md` 的清單加一行，在 `CLAUDE.md` 加 `@路徑`，執行 `check-ai-structure.ps1` 確認。
- 新增 skill：放進 `.agents/skills/<name>/SKILL.md`，執行 `sync-skills.ps1`。只給 Claude 用的 skill 放 `.claude/skills/`，並登記到 `skill-sync.json` 的 `claudeOnly`；只給 Codex 用的登記 `codexOnly`。
- 升級範本：改 `template/` 後遞增 `TEMPLATE_VERSION`，對各專案執行 `New-AiProject.ps1 -Path <專案> -Refresh`，再用 `check-ai-structure.ps1` 確認。主版本改變（例如 2.x → 3.0.0）時另有遷移步驟，見下方。
- 模型路由的環境變數：`AI_MODEL_ROUTER=off` 關掉 hook，`AI_MODEL_ROUTER_STATE_DIR` 改狀態資料夾，`AI_CLAUDE_BIN` 指定 `bench` 用的 claude 執行檔。已有自己環境變數名稱的專案（例如 `AOI_MODEL_ROUTER`）維持原狀即可。

### 2.x 升級到 3.0.0（Upgrade 2.x to 3.0.0）

照順序做；機械性的部分由腳本做，需要判斷的部分由 agent 做。原則：**未完成的需求拆成計畫，已完成的紀錄原樣封存**，專案擁有的規則文字保留專案自己的事實與特殊規則，只把 2.x 的做法改寫成 3.0.0 的說法。

1. 確認工作目錄乾淨（`git status`），沒有進行中的分支或 worktree 會動到 `docs/`；有 git 時從預設分支開 `docs/ai-structure-v3` 分支。
2. `New-AiProject.ps1 -Path <專案> -Refresh`：更新範本管理的檔案（工作流程、`ai-handoff` skill、檢查腳本、`_template`、遷移腳本）。這時結構檢查會失敗，正常。
3. `node scripts/ai/migrate-ai-structure-3.mjs --dry-run` 看一遍，再不加 `--dry-run` 執行：舊的 `docs/current-status.md`、`docs/changelog-ai.md`、`docs/plan/` 搬到 `docs/legacy/`（git mv），改寫相對連結，建立新的 `docs/current-status.md`（含 `TODO(migrate)`）與空的 `docs/plans/INDEX.md`（下一個可用編號已算好），並列出「舊進度表裡未完成的需求」與「還在講 2.x 做法的文字」。
4. **拆出計畫**：腳本列出的每個未完成的頂層需求（⬜、⏳、⚠️）建一個 `docs/plans/active/<ID>-<slug>/`（從 `_template/` 複製）。編號照舊；`plan.md` 的細項表放它所有的細項（含已完成的，狀態照抄）；舊的 `docs/legacy/plan/<ID>-*.md` 有內容就整理進 `plan.md`（目標、範圍、完成定義、Start prompt）；`status.md` 寫現在的狀態（取自舊 current-status 的相關段落與交接單）；`changelog.md` 寫一筆「3.0.0 遷移」，說明舊紀錄在 `docs/legacy/changelog-ai.md`（不搬舊紀錄）。在 `INDEX.md` 各加一列。已完成的需求不建資料夾，留在 `docs/legacy/current-status.md`。
5. **專案層級的快照**：新的 `docs/current-status.md` 填 `TODO(migrate)`：階段一句話、跨計畫的已知問題與待使用者決定（只屬於某個計畫的放進那個計畫的 `status.md`）。
6. **改寫專案擁有的規則文字**（腳本報告列出的位置）：`AGENTS.md` 第 4 條、必讀清單（加 `docs/plans/INDEX.md`，`docs/current-status.md` 的說明改成專案快照）與「收工前」；`CLAUDE.md` 的不自動匯入清單與 architect 計畫一行；`AI_CONTEXT.md` 的目錄與 Context Routing；`project-rules.md` 的四種輸出適用範圍、Step Protocol、文件一節；`.claude/agents/` 四個定義檔；`MODEL_ROUTING.md` 的「規劃與進度由主 agent 寫」；`docs/00-master-plan.md` 的進度連結與步驟說明。可以參考範本 `template/` 裡同名檔案的 3.0.0 寫法，但**保留專案自己的內容**。歷史文件（舊 ADR、`docs/legacy/`、`docs/steps/` 的舊輸出）不改寫。
7. 新增一筆 ADR（`docs/adr/NNN-ai-structure-v3.md`，編號接專案自己的序列）並在 `docs/decisions.md` 索引最上方加一行；在 `INDEX.md` 加一個已完成的計畫記錄這次遷移（例如 `completed/<下一個編號>-ai-structure-v3/`，細項就是上面這些步驟），並覆寫 `.ai/handoff.md`。
8. 驗證：`scripts/ai/check-ai-structure.ps1`（填完 `TODO(project)` 的專案加 `-Strict`）、`sync-skills.ps1 -Check`、有模型路由時 `node scripts/ai/check-model-routing.mjs`；再用 `git grep -n "需求進度表\|changelog-ai.md\|docs/plan/"` 確認只剩 `docs/legacy/`、歷史 ADR 與 3.0.0 規則裡說明遷移的句子。
9. 依專案的 Git 規則與上層共用規則檔提交（訊息依 `project-rules.md`「Commit 訊息」）。

### 套用到已有結構的專案（2.0.0 之前的專案）

腳本不會動既有的 `AGENTS.md`、`CLAUDE.md` 與其他專案擁有的檔案，檢查會列出缺少的項目，請手動**合併**（保留專案既有的事實與特殊規則，補上 3.0.0 的通用段落；專案已調好的模型路由案例與檢查不要換掉）：

1. 先確認沒有進行中的分支或 worktree，並備份或確認沒有他人的未提交修改。
2. 若已有自己的狀態檔（如 `docs/STATUS.md`），在套用前先更名為 `docs/current-status.md`，再全專案掃描並更新舊引用，用區分大小寫的搜尋確認沒有殘留。
3. 把舊入口檔的內容搬到共用文件：專案事實進 `AI_CONTEXT.md`，結構與依賴規則進 `docs/architecture.md`，慣例與流程進 `project-rules.md`。舊入口檔改成薄入口，並補上「不可省略的四條」。
4. 若舊入口檔裡有「每個 session 都該看到」的護欄（如分層規則），把搬走後的文件加進 `AGENTS.md` 必讀清單與 `CLAUDE.md` 的 `@` 匯入，否則護欄會從常駐 context 消失。
5. `docs/decisions.md` 內嵌 ADR 的專案：拆成 `docs/adr/NNN-slug.md`（編號不變），`decisions.md` 改成索引（一行一筆，連到 ADR 檔）。
6. 建立 `docs/00-master-plan.md`（依 README、`AI_CONTEXT.md`、architecture 回填）與 `docs/plans/INDEX.md`（依舊的狀態檔與 `git log` 把未完成的需求拆成計畫；舊的狀態檔與 changelog 搬到 `docs/legacy/`）。
7. 若專案的流程授權了 commit（例如每步在本機 commit），寫進 `project-rules.md` 的 Git 一節，並限定範圍。
8. 執行 `check-ai-structure.ps1 -Strict`、`sync-skills.ps1 -Check`、`node scripts/ai/check-model-routing.mjs`，再用 `claude -p` 實測匯入有載入。

## 已知限制

- **Codex 端沒有在本機端到端測過**：Codex 的行為依官方文件設計，並由檢查腳本驗證結構（含 32 KiB 上限）。Claude Code 端的匯入已在 AutoCadCreated 實測載入。
- `.claude/settings.json` 的 deny 規則是護欄，不是安全邊界。Claude Code 在新資料夾要先接受信任對話框，`permissions` 才會生效。
- Skill 鏡像是複製，不是連結（Windows 符號連結需要權限，git 設定也會影響）。所以改了來源要記得跑 `sync-skills.ps1`；忘記時 `check-ai-structure.ps1` 會擋。
- commit 格式結尾的 `[Test]` 與測試結果行是使用者的固定規則，沒有建置的專案也照寫，所以它不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊，且必須如實。
- 模型路由 hook 只用關鍵字評分，是建議不是判斷；範本的 `TOPICS` 是通用的（架構、並行、安全、不可逆資料操作、跨模組重構、效能、根因不明、建置系統），專案若有特殊的高風險主題，要自己加。`check-model-routing.mjs` 的通用範例只驗證 hook 的關鍵字邏輯；`verify` 才能證明 Claude Code 實際套用了 model 與 effort（會花額度，範本沒有執行過）。
- 腳本只用 Windows PowerShell 5.1 測過，沒有在 PowerShell 7 與 macOS／Linux 上測。`check-ai-structure.ps1` 在 PowerShell 內呼叫 `node`；找不到 `node` 時只給警告。
- `check-ai-structure.ps1` 對 `current-status.md` 的長度上限是 80 行（警告：3.0.0 起它只是專案層級的快照）；`.ai/handoff.md` 維持 60 行。
- 計畫的文件是 Markdown，不是多人即時資料庫：兩個 session 同時改同一個 `INDEX.md` 或 `.ai/handoff.md` 會衝突。並行的工作用 git 分支或 worktree 隔離，合併時再處理；各自只改自己的計畫資料夾。
- `migrate-ai-structure-3.mjs` 只改寫 Markdown 的相對連結與反引號裡的具體舊計畫檔路徑；HTML 頁面裡的連結、規則文字的語意不改（報告列出位置，由 agent 改寫）。
