# 模型與推理強度路由規則（Claude Code 專用）

目的：依任務的難度與風險，同時決定模型（Haiku、Sonnet、Opus）與推理強度（effort），在正確性與額度之間取平衡。`CLAUDE.md` 每個 session 都匯入本檔。設計決策見 `docs/adr/000-ai-agent-structure.md`；本檔是範本的通用版（版本見 `.agents/harness/TEMPLATE_VERSION`），**「本專案案例」一節要由專案填寫**。

## 1. 原則與限制

- 模型用別名 `haiku`、`sonnet`、`opus`；effort 用 `low`、`medium`、`high`、`xhigh`、`max`。不寫版本號，模型升級時規則不必改。
- **主 session 的模型與 effort 不會因為本檔、hook 或 subagent 而改變。** 做法是把子任務委派給指定模型與 effort 的 subagent，實際的模型與 effort 由所委派 subagent 的有效設定（frontmatter 的 `model`、`effort`）決定。不要說「已切換」；要換主 session 的模型或 effort 是使用者的操作，建議換時說出理由，由使用者決定。
- **hook 只注入建議，不強制。** Claude 仍要依實際任務重新評分，不可盲從 hook 的關鍵字分類。
- 沒把握時選 `developer`（Sonnet、medium）。檔案多本身不是升級的理由。
- **先提高推理強度，再換模型**：複雜但風險不高的工作用 `developer-deep`；涉及架構、安全或高風險修改時，直接用 `architect`。
- 不要沒有理由地降級（省下的額度抵不過一次改錯的重工），也不要無限制重試或開不必要的 agent。

## 2. 評分

每個需求評六個因素。前四個 1–5 分，取命中條件中最高的一項（不相加）：

| 因素 | 1 | 2–3 | 4–5 |
|---|---|---|---|
| 複雜度（Complexity） | 查詢、讀程式 | 2：照做的局部修改；3：一個模組內的功能、新的 UI 元件與測試、三個以上的檔案 | 4：跨兩層以上、效能工作、新的機制；5：跨模組重構、架構重整 |
| 風險（Risk） | 唯讀 | 2：局部、有測試、可以還原；3：建置設定、演算法、效能敏感的程式、根因不明的問題 | 4：分層、介面契約、依賴規則、並行與生命週期、資源所有權、跨模組重構；5：加密、金鑰、授權、不可逆的資料操作 |
| 推理深度（Reasoning） | 查資料 | 2：照做的修改；3：除錯、分析、比較 | 4：跨元件找根因、設計取捨、高風險主題；5：兩個以上的高風險主題同時出現 |
| 不確定性（Uncertainty） | 規格清楚 | 2：有錯誤訊息；3：原因不明但能重現 | 4：偶爾發生、無法重現；5：主 session 判斷（例如證據互相矛盾） |

另外兩個：

- **範圍（Scope）**：`single-file`（一個檔案）、`module`（同一個模組，或提到兩個以上的檔案）、`cross-module`（`src/` 底下兩個以上的資料夾，或跨模組的主題）、`unknown`。
- **驗證成本（Verification Cost）**：`low`（唯讀或只改文件）、`medium`（要建置與跑測試）、`high`（要授權、真實資料、截圖、效能量測、人工確認，或問題本來就難重現：並行、根因不明、效能）。

hook 用關鍵字算出初步分數（第 7 節）；Claude 看過實際的程式碼後重新評分。專案若有特殊的高風險主題（例如特定演算法、資料格式），在「本專案案例」一節補充，並視需要調整 hook 的 `TOPICS`。

## 3. 分派表與判斷順序

| 等級 | agent（`.claude/agents/`） | model | effort | 用途 |
|---|---|---|---|---|
| 1 | `fast-worker`（唯讀） | `haiku` | `low` | 找檔案、符號、呼叫點；整理建置與測試的 log 與命令輸出；彙整 `git diff --stat`；跑結構檢查或 lint 並摘要 |
| 2（預設） | `developer` | `sonnet` | `medium` | 既有架構內的功能與介面修改、單元與整合測試、建置設定加檔、一般除錯、已知架構內的重構、文件與文案 |
| 3 | `developer-deep` | `sonnet` | `high` | 風險不高但難的工作：偶爾失敗但能重現的測試、效能瓶頸的量測與優化、演算法與數值的調整、驗證成本高的修改、`developer` 兩次有效嘗試仍失敗的問題 |
| 4 | `architect`（唯讀，出計畫） | `opus` | `high` | 架構、安全、高風險修改：分層、介面契約、依賴規則、並行與生命週期、資源所有權、跨模組重構、授權、加密與金鑰、不可逆的資料操作 |
| 5 | `architect-deep`（唯讀，出計畫） | `opus` | `xhigh` | 風險高而且根因不明或推理特別深：無法重現的並行或授權問題、同時牽涉多個高風險主題的設計、`architect` 兩次有效嘗試仍失敗的問題 |

由上而下，第一個符合的就停：

1. 查詢類（唯讀、範圍明確，沒有修改、分析或設計的字眼）→ `fast-worker`。主題有風險也一樣，因為查詢不改任何東西。
2. 風險 ≥ 4：推理 ≥ 5 或不確定性 ≥ 4 → `architect-deep`；其餘 → `architect`。
3. 複雜度 ≥ 4、推理 ≥ 4、不確定性 ≥ 4 或驗證成本 `high` → `developer-deep`。
4. 其餘 → `developer`。

風險 3 的主題（演算法、效能、建置、根因不明）預設走 `developer-deep`。要改 ADR 已記錄的規則時算架構，改用 `architect`。

## 4. 動態升級與降級

- **升級**：同一個問題兩次有效嘗試仍沒解決，就升一級（`developer` → `developer-deep` → `architect` → `architect-deep`），先重新分析根因。「有效」＝依證據改了做法；重複同一個做法不算。升級後的等級是下限，直到問題解決。
- **直接升到 Opus**：修正需要改變架構或介面契約、測試失敗暴露系統層的問題、發現安全或資料完整性的風險、無法建立可靠的根因（`engineering-workflow.md` 第 4 節）。
- **最高一級仍失敗**：`architect-deep` 也兩次失敗時，停下來整理已知的證據，請使用者決定。不要無限制重試。
- **降級**：問題解決後拿掉下限，後續明確的小任務照常分級（例如解決 race condition 之後，改文案用 `developer`、查檔案用 `fast-worker`）。
- 升級時把已知的發現交過去：失敗的指令與輸出、已排除的假設、讀過的檔案。不要讓新的模型從頭來過。

hook 會從使用者的話記錄失敗與成功（第 7 節）。Claude 自己在一個回合裡的失敗也算，要自己計數。

## 5. 委派（`Agent` 工具）

- **主動評估，只在值得時委派**：獨立的子任務、能平行做的事、會產生大量輸出的事（大範圍搜尋、整理很長的 log）、需要較強推理的分析。小任務自己做；不要為了「分級」而開 agent，每個 agent 的額度都算在使用者身上。
- **委派有固定成本**：每個 subagent 啟動時都載入 `CLAUDE.md` 與它匯入的文件，約 49K–70K tokens（其他專案的實測；本專案的數字記在第 9 節）。所以「交給便宜的模型比較省」只對大量讀取的工作成立。
- **主 session 的模型與 effort 已經足夠時，自己做**，不要委派給同一級或更低的設定。主 session 目前的模型與 effort 要自己判斷：hook 只在拿得到時回報 `main_session_effort`。
- **怎麼委派**：`subagent_type` 填第 3 節的 agent。不要另外傳 `model` 或 `effort` 參數蓋過 agent 的設定，要不同的強度就選對應的 agent。prompt 寫清楚目標、範圍、要讀的檔案、驗收、已排除的事、要回報什麼，並**明寫「回覆使用繁體中文（zh-TW）」**。subagent 看不到這段對話。
- **subagent 的回報是未驗證的**：重要的結論自己查證後才寫進回報；subagent 說「測試通過」不能當 `VERIFIED`（`AGENTS.md` 不可省略的第 2 條）。
- **subagent 一樣受 `AGENTS.md` 的規則限制**：繁體中文回報、不虛構驗證、不 commit 與 push。五個 subagent 的定義檔已寫入這些規則，而且都不能再委派。
- **規劃與進度由主 agent 寫**：architect 系列只回傳計畫；主 agent 把它寫進 `docs/current-status.md` 的需求進度表與 `docs/plan/`，subagent 不直接改這兩份文件。
- 同一個任務不要同時自己做又交給 subagent。

## 6. 回報

有委派、升級或降級時，在最終回報的 Decision Review 加一行：評分、用了哪個 agent、實際的模型與 effort、理由。瑣碎的動作不必寫。

## 7. Hook

`.claude/settings.json` 的 `UserPromptSubmit` 在每次送出需求時執行 `.claude/hooks/model_router.mjs`，把一行 JSON 經 `additionalContext` 交給 Claude：

| 欄位 | 內容 |
|---|---|
| `recommended_agent`、`recommended_model`、`recommended_effort` | 第 3 節的分派結果；model 與 effort 和該 agent 的 frontmatter 一致（檢查腳本會比對） |
| `complexity_score`、`risk_score`、`reasoning_score`、`uncertainty_score` | 第 2 節的 1–5 分 |
| `scope`、`verification_cost` | 第 2 節 |
| `routing_reason` | 套用了哪一條規則、命中哪些主題、有沒有升級的下限 |
| `failure_count`、`escalation_count`、`escalated` | 這個 session 裡使用者回報失敗的次數、累計升級的次數、這一次有沒有升級 |
| `main_session_effort` | 主 session 目前的 effort（讀環境變數 `CLAUDE_EFFORT`，拿不到就沒有這個欄位；桌面 app 的 `UserPromptSubmit` 可能拿不到） |

- 提示寫明：這只是建議；主 session 的模型與 effort 沒有切換；實際的模型與 effort 由所委派 subagent 的設定決定。
- **狀態**：同一個 session 的失敗次數、升級次數與升級的下限，存在 `%TEMP%\claude-model-router\<session>.json`（環境變數 `AI_MODEL_ROUTER_STATE_DIR` 可以改；寫不進去就當沒有狀態），7 天沒更新的檔案會被刪掉。「還是不行」「又失敗了」算失敗；「好了」「解決了」算成功，會拿掉下限。
- **不處理**：斜線指令（`/…`），以及 agent 與背景工作的通知。太短的回覆（少於 6 字）不重新分級，只更新狀態；升級或到了最高一級時才輸出。
- 不呼叫外部 API。任何失敗都以結束碼 0、不輸出收場，**不會擋住你的需求**。環境變數 `AI_MODEL_ROUTER=off` 可以整個關掉。
- 限制：關鍵字比對不理解程式碼。貼上很長的文字時容易誤判（例如文字裡提到「架構」不代表任務是架構），這時提示會註明。
- 命令寫成 `node "${CLAUDE_PROJECT_DIR}/.claude/hooks/model_router.mjs"`：Windows 上 Claude Code 可能以 PowerShell 執行 hook，其他寫法的環境變數可能展開成空字串；`check-model-routing.mjs` 會用 Git Bash 實際跑一次這個指令字串。

## 8. 驗證與效能評估

| 指令 | 花額度 | 檢查什麼 |
|---|---|---|
| `node scripts/ai/check-model-routing.mjs` | 不花 | 五個 subagent 的 `model`、`effort` 與工具邊界；`settings.json` 合法、hook 已登錄、和 git HEAD 相比沒有少設定；通用範例需求分到預期的 agent、欄位與分數範圍、hook 建議的 model 與 effort 等於 frontmatter；升級、降級、最高一級；agent 的通知與壞掉的輸入不處理；Windows 上用 Git Bash 跑實際的指令字串；`CLAUDE.md` 匯入本檔；「本專案案例」填寫後逐列檢查（見下） |
| `node scripts/ai/bench-model-routing.mjs verify --yes` | 約 US$1 | 開一個 headless session，同時委派五個 subagent，從 session 紀錄讀出**實際的** model 與 effort，和 frontmatter 比對；不同就報「降級」 |
| `node scripts/ai/bench-model-routing.mjs bench --yes` | 數美元 | 同一組任務（查詢、讀程式推理、純推理）讓五個 agent 各跑一次：正確與否、時間、tokens（含思考）、Claude Code 回報的費用、實際的 model 與 effort、是否建議升級 |

改過本檔、`.claude/agents/`、`.claude/hooks/` 或 `.claude/settings.json` 之後跑第一個；換 Claude Code 版本或帳號之後跑 `verify`。`verify` 與 `bench` 沒有 `--yes` 只印計畫，結果寫到 `perf/results/model-routing-*.json`（不入版控）。數字只取 Claude Code 回報的值，拿不到的寫 `n/a`，不估算。

**「本專案案例」的檢查規則**：表格欄位固定為 `| 需求 | 建議 |`，建議是五個 agent 名稱之一；需求欄仍以 `TODO(project)` 開頭的列，檢查只給警告。填寫後，檢查腳本把每一列的需求交給 hook：agent 名稱不是五個之一算失敗；hook 的建議和表格不同只給警告（hook 只是關鍵字建議，案例表是專案對「正確分級」的判斷）。

**怎麼知道實際的 effort**：Claude Code 的 session 紀錄（`~/.claude/projects/<專案>/<session>.jsonl`，subagent 在 `<session>/subagents/agent-<id>.jsonl`）每一筆 assistant 訊息都有 `effort` 欄位，就是那次實際送出的值；模型不支援 effort 時沒有這個欄位。官方文件提到的其他管道：statusline 的 `effort.level`、hook 的 `$CLAUDE_EFFORT`、`/tasks`、OpenTelemetry。

**Claude Code 的行為**（以官方文件為準，實測請記在第 9 節）：

- subagent 的 frontmatter `effort` 會套用，它蓋過 session 的 effort，但蓋不過環境變數 `CLAUDE_CODE_EFFORT_LEVEL`；檢查腳本在這個變數有設定時會警告。
- `claude --agent <name>` 只套用 agent 的模型與工具限制，不一定套用 frontmatter 的 effort，所以 `bench` 明確傳 `--effort`。
- 模型不支援設定的等級時，降到它支援的最高等級；模型完全不支援 effort 時，Claude Code 不送 effort，請求照樣成功。`verify` 會把這兩種情況報成「降級」。
- `haiku` 別名依 Claude Code 版本而定，是否支援 effort 以官方文件與 `verify` 的結果為準。

## 本專案案例

> 專案填寫：列出這個專案常見的需求與你認為正確的分級，供 hook 與日後調整時對照。至少填五到八列，涵蓋五個等級。格式固定（見第 8 節）。

| 需求 | 建議 |
|---|---|
| TODO(project): 一個明確唯讀的查詢（例如「找出某函式定義在哪個檔案」） | fast-worker |
| TODO(project): 一個局部、有測試的修改（例如「修正 README 錯字」） | developer |
| TODO(project): 一個複雜但低風險、驗證成本高的修改（例如「調整畫面並截圖驗證」） | developer-deep |
| TODO(project): 一個高風險的修改（例如「修改核心演算法或資料格式」） | architect |
| TODO(project): 一個高風險又根因不明的問題（例如「分析偶發的資料損毀」） | architect-deep |

## 9. 實測紀錄

> 專案填寫：跑過 `verify`、`bench` 或在實際 session 觀察到 hook 行為後，在這裡記下日期、Claude Code 版本、指令、結果（實際的 model 與 effort、費用、tokens）與發現。沒實測就留空，不要抄別的專案的數字。

（尚無）

Codex 沒有對應機制，不讀本檔。
