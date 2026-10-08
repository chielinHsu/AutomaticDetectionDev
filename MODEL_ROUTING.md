# 模型與推理強度路由規則（Claude Code 專用）

目的：依任務的難度與風險，同時決定模型（Haiku、Sonnet、Opus）與推理強度（effort），在正確性與額度之間取平衡。`CLAUDE.md` 每個 session 都匯入本檔。設計決定：[ADR-019](docs/adr/019-model-routing.md)（第一版）、[ADR-020](docs/adr/020-adaptive-model-effort-routing.md)（加上 effort 與評分）。

來源：使用者 2026-10-08 提供的 ChatGPT 討論（<https://chatgpt.com/share/6ac74783-b388-83ee-b335-1f3092f9ee36>）與兩份設定 prompt。規則已改寫成本專案的版本：評分有可檢查的判準、例子換成本專案的模組、與 `engineering-workflow.md` 已有的規則（驗證、回報）不重複。

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
| 風險（Risk） | 唯讀 | 2：局部、有測試、可以還原；3：建置設定、座標與搜尋演算法、效能敏感的程式、根因不明的問題 | 4：分層、port 契約、依賴表、並行與生命週期、影像所有權、跨模組重構；5：加密、金鑰、授權、不可逆的資料操作 |
| 推理深度（Reasoning） | 查資料 | 2：照做的修改；3：除錯、分析、比較 | 4：跨元件找根因、設計取捨、高風險主題；5：兩個以上的高風險主題同時出現 |
| 不確定性（Uncertainty） | 規格清楚 | 2：有錯誤訊息；3：原因不明但能重現 | 4：偶爾發生、無法重現；5：主 session 判斷（例如證據互相矛盾） |

另外兩個：

- **範圍（Scope）**：`single-file`（一個檔案）、`module`（同一個模組，或提到兩個以上的檔案）、`cross-module`（`src/` 底下兩層以上，或跨模組的主題）、`unknown`。
- **驗證成本（Verification Cost）**：`low`（唯讀或只改文件）、`medium`（要建置與跑測試）、`high`（要 HALCON 授權、真實影像、效能量測、人工確認，或問題本來就難重現：並行、根因不明、效能）。

hook 用關鍵字算出初步分數（第 7 節）；Claude 看過實際的程式碼後重新評分。

## 3. 分派表與判斷順序

| 等級 | agent（`.claude/agents/`） | model | effort | 用途 |
|---|---|---|---|---|
| 1 | `fast-worker`（唯讀） | `haiku` | `low` | 找檔案、符號、呼叫點；整理 `build/qmake-*/test-logs/` 與命令輸出；彙整 `git diff --stat`；跑 `check-deps`、`ste-lint` 並摘要 |
| 2（預設） | `developer` | `sonnet` | `medium` | 一般的 C++、Qt（Presenter、Widgets 的功能與版面）、Python、網頁、UI、文案、單元與整合測試、`.pro` 加檔、一般除錯、已知架構內的重構 |
| 3 | `developer-deep` | `sonnet` | `high` | 風險不高但難的工作：偶爾失敗但能重現的測試、效能瓶頸的量測與優化、座標與搜尋演算法的調整、驗證成本高的修改、`developer` 兩次有效嘗試仍失敗的問題 |
| 4 | `architect`（唯讀，出計畫） | `opus` | `high` | 架構、安全、高風險修改：分層、port 契約、依賴表、並行與生命週期、影像所有權、跨模組重構、授權、加密與金鑰、不可逆的資料操作 |
| 5 | `architect-deep`（唯讀，出計畫） | `opus` | `xhigh` | 風險高而且根因不明或推理特別深：無法重現的並行或授權問題、同時牽涉多個高風險主題的設計、`architect` 兩次有效嘗試仍失敗的問題 |

由上而下，第一個符合的就停：

1. 查詢類（唯讀、範圍明確，沒有修改、分析或設計的字眼）→ `fast-worker`。主題有風險也一樣，因為查詢不改任何東西。
2. 風險 ≥ 4：推理 ≥ 5 或不確定性 ≥ 4 → `architect-deep`；其餘 → `architect`。
3. 複雜度 ≥ 4、推理 ≥ 4、不確定性 ≥ 4 或驗證成本 `high` → `developer-deep`。
4. 其餘 → `developer`。

風險 3 的主題（座標與搜尋演算法、效能、建置、根因不明）預設走 `developer-deep`。要改 ADR 已記錄的規則（例如 ADR-010 的座標慣例、ADR-013 的晶格接受規則）時算架構，改用 `architect`。

## 4. 動態升級與降級

- **升級**：同一個問題兩次有效嘗試仍沒解決，就升一級（`developer` → `developer-deep` → `architect` → `architect-deep`），先重新分析根因。「有效」＝依證據改了做法；重複同一個做法不算。升級後的等級是下限，直到問題解決。
- **直接升到 Opus**：修正需要改變架構或 port、測試失敗暴露系統層的問題、發現安全或資料完整性的風險、無法建立可靠的根因（`engineering-workflow.md` 第 4 節）。
- **最高一級仍失敗**：`architect-deep` 也兩次失敗時，停下來整理已知的證據，請使用者決定。不要無限制重試。
- **降級**：問題解決後拿掉下限，後續明確的小任務照常分級（例如解決 race condition 之後，改文案用 `developer`、查檔案用 `fast-worker`）。
- 升級時把已知的發現交過去：失敗的指令與輸出、已排除的假設、讀過的檔案。不要讓新的模型從頭來過。

hook 會從使用者的話記錄失敗與成功（第 7 節）。Claude 自己在一個回合裡的失敗也算，要自己計數。

## 5. 委派（`Agent` 工具）

- **主動評估，只在值得時委派**：獨立的子任務、能平行做的事、會產生大量輸出的事（大範圍搜尋、整理很長的 log）、需要較強推理的分析。小任務自己做；不要為了「分級」而開 agent，每個 agent 的額度都算在使用者身上。
- **委派有固定成本**（實測，第 9 節）：每個 subagent 啟動時都載入 `CLAUDE.md` 與它匯入的文件，約 49K–70K tokens。所以「交給便宜的模型比較省」只對大量讀取的工作成立。
- **主 session 的模型與 effort 已經足夠時，自己做**，不要委派給同一級或更低的設定。主 session 目前的模型與 effort 要自己判斷：hook 只在拿得到時回報 `main_session_effort`，實測 `UserPromptSubmit` 拿不到（第 7 節）。
- **怎麼委派**：`subagent_type` 填第 3 節的 agent。不要另外傳 `model` 或 `effort` 參數蓋過 agent 的設定，要不同的強度就選對應的 agent。prompt 寫清楚目標、已排除的事、要讀的檔案、要回報什麼。subagent 看不到這段對話。
- **subagent 的回報是未驗證的**：重要的結論自己查證後才寫進回報；subagent 說「測試通過」不能當 `VERIFIED`（`AGENTS.md` 不可省略的第 2 條）。
- **subagent 一樣受 `AGENTS.md` 的三條規則限制**：繁體中文回報、不虛構驗證、不 commit 與 push。五個 subagent 的定義檔已寫入這些規則，而且都不能再委派。
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
| `main_session_effort` | 主 session 目前的 effort（讀環境變數 `CLAUDE_EFFORT`，拿不到就沒有這個欄位）。實測桌面 app（2.1.293）的 `UserPromptSubmit` hook 拿不到（同一個 session 的 Bash 工具拿得到 `max`），所以目前不會出現 |

- 提示寫明：這只是建議；主 session 的模型與 effort 沒有切換；實際的模型與 effort 由所委派 subagent 的設定決定。
- **狀態**：同一個 session 的失敗次數、升級次數與升級的下限，存在 `%TEMP%\claude-model-router\<session>.json`（環境變數 `AOI_MODEL_ROUTER_STATE_DIR` 可以改；寫不進去就當沒有狀態），7 天沒更新的檔案會被刪掉。「還是不行」「又失敗了」算失敗；「好了」「解決了」算成功，會拿掉下限。
- **不處理**：斜線指令（`/…`），以及 agent 與背景工作的通知。實測它們也會觸發 `UserPromptSubmit`：第一版把一份 subagent 的回報當成 8 千字的需求打分；改版後，桌面 app 裡 subagent 的回報不再帶出提示。太短的回覆（少於 6 字）不重新分級，只更新狀態；升級或到了最高一級時才輸出。
- 不呼叫外部 API。任何失敗都以結束碼 0、不輸出收場，**不會擋住你的需求**。環境變數 `AOI_MODEL_ROUTER=off` 可以整個關掉。
- 限制：關鍵字比對不理解程式碼。貼上很長的文字時容易誤判（例如文字裡提到「架構」不代表任務是架構），這時提示會註明。
- 為什麼用 Node 不用 Python：這台機器的 `python.exe` 只是 Store 捷徑（`AI_CONTEXT.md`），Node 是本專案已有的工具鏈（`tools/*.mjs`）。

## 8. 驗證與效能評估

| 指令 | 花額度 | 檢查什麼 |
|---|---|---|
| `node scripts/ai/check-model-routing.mjs` | 不花 | 五個 subagent 的 `model`、`effort` 與工具；`settings.json` 和 `git HEAD` 相比沒有少設定；17 個範例需求分到預期的 agent、欄位與分數範圍、hook 建議的 model 與 effort 等於 frontmatter；升級、降級、最高一級；agent 的通知與壞掉的輸入不處理；用 Git Bash 跑實際的指令字串；`CLAUDE.md` 的匯入 |
| `node scripts/ai/bench-model-routing.mjs verify --yes` | 約 US$1 | 開一個 headless session，同時委派五個 subagent，從 session 紀錄讀出**實際的** model 與 effort，和 frontmatter 比對；不同就報「降級」 |
| `node scripts/ai/bench-model-routing.mjs bench --yes` | 數美元 | 同一組任務（查詢、讀程式推理、純推理）讓五個 agent 各跑一次：正確與否、時間、tokens（含思考）、Claude Code 回報的費用、實際的 model 與 effort、是否建議升級 |

改過本檔、`.claude/agents/`、`.claude/hooks/` 或 `.claude/settings.json` 之後跑第一個；換 Claude Code 版本或帳號之後跑 `verify`。`verify` 與 `bench` 沒有 `--yes` 只印計畫，結果寫到 `perf/results/model-routing-*.json`（不入版控）。數字只取 Claude Code 回報的值，拿不到的寫 `n/a`，不估算。

**怎麼知道實際的 effort**：Claude Code 的 session 紀錄（`~/.claude/projects/<專案>/<session>.jsonl`，subagent 在 `<session>/subagents/agent-<id>.jsonl`）每一筆 assistant 訊息都有 `effort` 欄位，就是那次實際送出的值；模型不支援 effort 時沒有這個欄位（實測，文件沒有寫）。官方文件提到的其他管道：statusline 的 `effort.level`、hook 的 `$CLAUDE_EFFORT`、`/tasks`、OpenTelemetry。

**Claude Code 的行為**（2026-10-08；「文件」指 code.claude.com 的官方文件，「實測」指本機 2.1.292–2.1.294）：

- subagent 的 frontmatter `effort` 會套用（實測，五個 agent 都照設定）。它蓋過 session 的 effort，但蓋不過環境變數 `CLAUDE_CODE_EFFORT_LEVEL`（文件）；檢查腳本在這個變數有設定時會警告。
- `claude --agent <name>` 只套用 agent 的模型與工具限制，**不套用 frontmatter 的 effort**（實測：設 `low`，實際是預設的 `medium`）。所以 `bench` 明確傳 `--effort`。
- 模型不支援設定的等級時，降到它支援的最高等級（文件）；模型完全不支援 effort 時（例如 Haiku 4.5），Claude Code 不送 effort，請求照樣成功（文件；實測紀錄沒有 `effort` 欄位）。`verify` 會把這兩種情況報成「降級」。
- Opus 5.5、Sonnet 5.5、Haiku 5.5 都支援 low 到 max（文件）；預設 effort 是 `medium`（文件）。
- **`haiku` 別名依 Claude Code 版本而定**：2.1.292 以前是 Haiku 4.5（不支援 effort，`fast-worker` 的 `low` 不會生效），2.1.293 起是 Haiku 5.5（文件；實測 2.1.292 是 `claude-haiku-4-5-20251001`、2.1.294 是 `claude-haiku-5-5`）。Claude Code 會自動更新，本 session 期間就從 2.1.292 更新到 2.1.294。

## 9. 實測紀錄

### 2026-10-08：第一版（三個 subagent，沒有 effort；Claude Code 2.1.292，headless）

- hook 有執行：新 session 的 context 有路由提示，內容和 hook 的輸出相同。互動式（桌面 app 的 Code 分頁）也有：在進行中的 session 改完 `settings.json` 之後，下一則使用者訊息的 context 就有路由提示，不必重新開 session。
- 三個同時委派，回報都回到主 session。實際模型：`fast-worker` → `claude-haiku-4-5-20251001`、`developer` → `claude-sonnet-5-5`、`architect` → `claude-opus-5-5`。
- 一次測試（主 session 用 Haiku、三個 subagent 各回一句話）共 US$0.59（Claude Code 回報的 list 價）。

### 2026-10-08：加上 effort（五個 subagent；Claude Code 2.1.294）

改版後的 hook 在桌面 app 的 session 也有執行：下一則使用者訊息的 context 是新的 JSON 格式（沒有 `main_session_effort`，見第 7 節）。

`verify`：五個 subagent 的 model 與 effort 都照設定套用，沒有降級。整個 session US$0.86。

| agent | 設定 | 實際 model | 實際 effort | tokens | 時間 |
|---|---|---|---|---|---|
| `fast-worker` | haiku / low | `claude-haiku-5-5` | low | 48 920 | 1.0 s |
| `developer` | sonnet / medium | `claude-sonnet-5-5` | medium | 69 429 | 2.3 s |
| `developer-deep` | sonnet / high | `claude-sonnet-5-5` | high | 69 572 | 2.1 s |
| `architect` | opus / high | `claude-opus-5-5` | high | 49 069 | 1.6 s |
| `architect-deep` | opus / xhigh | `claude-opus-5-5` | xhigh | 49 256 | 1.6 s |

tokens 幾乎都是啟動時載入的專案文件（每個只回一句話）。`developer` 系列沒有列 `tools`（繼承全部工具），所以比只列 5 個工具的 agent 多約 20K tokens。

`bench`（三個任務 × 五個 agent × 1 次，`claude -p --agent <agent> --effort <設定>`）：15 次的實際 model 與 effort 都和設定相同。費用是 Claude Code 回報的 `total_cost_usd`（list 價），時間是它回報的 `duration_ms`。

| agent | 查詢（容易） | 讀程式推理（中等） | 純推理（困難） | 正確 | 費用合計 |
|---|---|---|---|---|---|
| `fast-worker`（haiku/low） | 對，4.3 s，US$0.010 | 對，5.8 s，US$0.011 | 對，7.3 s，US$0.009 | 3/3 | US$0.03 |
| `developer`（sonnet/medium） | 對，4.1 s，US$0.295 | 對，6.5 s，US$0.250 | 對，7.0 s，US$0.211 | 3/3 | US$0.76 |
| `developer-deep`（sonnet/high） | 對，4.1 s，US$0.201 | 對，7.6 s，US$0.251 | 對，7.8 s，US$0.212 | 3/3 | US$0.66 |
| `architect`（opus/high） | 對，4.0 s，US$0.382 | 對，6.2 s，US$0.415 | 對，13.9 s，US$0.356 | 3/3 | US$1.15 |
| `architect-deep`（opus/xhigh） | 對，5.9 s，US$0.349 | 對，8.0 s，US$0.418 | 對，6.1 s，US$0.357 | 3/3 | US$1.12 |

- 三個任務：「座標慣例在哪個檔案」、「讀 hook 的程式，回答某個需求分給哪個 agent」、「n² + 85n + 2017 是完全平方數的正整數 n 的總和」（答案 195）。
- **正確率分不出高下**：15 次全對。這三題對所有設定都太簡單；要比出差異，需要更難、但答案仍能自動判斷的任務（例如真實的除錯），每次要多花數美元。
- **費用幾乎都是啟動成本**：每次寫入快取 41K–74K tokens（專案文件、工具定義、agent 的提示），output 只有 12–536 tokens（思考 0–533）。同一題 Opus 的費用約是 Haiku 的 35–40 倍，Sonnet 約 20–30 倍。
- **effort 對這三題的影響很小**：思考 tokens 都在 600 以內，沒有隨 effort 單調增加（例如純推理題 Haiku/low 想了 533 tokens，Opus/xhigh 250）。n ＝ 1，時間的差異（例如 Opus/high 的 13.9 s）可能只是雜訊。
- 15 次都沒有「建議升級」。
- 這次 `verify` 與 `bench` 加上前面的 effort 探測，共花約 US$4.7（Claude Code 回報）。

Codex 沒有對應機制，不讀本檔。
