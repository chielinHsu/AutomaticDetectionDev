# ADR-020 模型與推理強度的路由：評分、五個 subagent 的 effort、動態升級與降級

- 日期：2026-10-08
- 決定者：使用者 2026-10-08 要求把 ADR-019 的路由升級成「Adaptive Model & Reasoning Effort Routing System」：每個需求評複雜度、風險、推理深度、不確定性（1–5）、範圍與驗證成本，同時建議 model 與 effort；五個 subagent（`fast-worker` haiku/low、`developer` sonnet/medium、`developer-deep` sonnet/high、`architect` opus/high、`architect-deep` opus/xhigh），用官方的 frontmatter `model` 與 `effort`；動態升級與降級；hook 回傳指定的欄位；驗證各 agent 的設定有效、記錄時間與 tokens、比較不同模型與強度。評分的判準、判斷順序、失敗與成功的狀態、驗證與實測的腳本是 Claude Code（Claude Opus 5.5）的提案並實作。
- 狀態：採用（Claude Code 專用；Codex 沒有對應機制）。取代 ADR-019 的兩個部分：「判準不用 1–5 分」與「三個 subagent」；其餘（hook 只注入提示、Node、唯讀的 architect、不能再委派、驗證腳本）沿用。

## 背景

ADR-019 只決定模型。使用者要同一個模型也能用不同的推理強度：複雜但不危險的工作先提高強度，不必換到 Opus；高風險的工作直接用 Opus。查證的事實（官方文件經 `claude-code-guide` 查證；實測在本機 Claude Code 2.1.292–2.1.294）：

- subagent 的 frontmatter 有 `effort`（low、medium、high、xhigh、max），它蓋過 session 的 effort，但蓋不過環境變數 `CLAUDE_CODE_EFFORT_LEVEL`（文件）。Opus 5.5、Sonnet 5.5、Haiku 5.5 都支援 low 到 max（文件）。
- 實測：在 scratchpad 的臨時 agent 定義檔上，haiku/low、haiku/xhigh、sonnet/medium、sonnet/high、sonnet/xhigh、opus/high、opus/xhigh 委派後，session 紀錄的 `effort` 都等於設定；`claude --agent` 只套用模型，effort 是預設的 medium；Haiku 4.5 不支援 effort，紀錄沒有 `effort` 欄位、請求照樣成功。
- `haiku` 別名：Claude Code 2.1.292 以前是 Haiku 4.5，2.1.293 起是 Haiku 5.5（文件；實測兩個版本都對得上）。ADR-019 寫的「`haiku` 對應 Haiku 4.5」只對 2.1.292 成立。

## 決定

1. **評分有判準**（`MODEL_ROUTING.md` 第 2 節）：四個 1–5 分的因素各有可檢查的條件，取命中條件中最高的一項，不相加；範圍與驗證成本是類別。hook 依關鍵字算初步分數，Claude 看過程式後重新評分。
2. **五個 subagent**（第 3 節）：在 ADR-019 的三個之外，新增 `developer-deep`（sonnet/high，能修改，不能再委派）與 `architect-deep`（opus/xhigh，唯讀）；五個都寫 `effort`。判斷順序：查詢 → `fast-worker`；風險 ≥ 4 → `architect`，再加上推理 ≥ 5 或不確定性 ≥ 4 → `architect-deep`；複雜度、推理或不確定性 ≥ 4，或驗證成本高 → `developer-deep`；其餘 → `developer`。
3. **動態升級與降級**（第 4 節）：同一個問題兩次有效失敗就升一級，升級後的等級是下限，直到問題解決；最高一級仍失敗就停下來請使用者決定；解決後拿掉下限。hook 從使用者的話（「還是不行」「好了」）記錄失敗與成功，存在 `%TEMP%\claude-model-router\<session>.json`。
4. **hook 回傳**（第 7 節）：一行 JSON，含 `recommended_agent`、`recommended_model`、`recommended_effort`、四個分數、`scope`、`verification_cost`、`routing_reason`、失敗與升級的次數、主 session 目前的 effort（`$CLAUDE_EFFORT`；實測 `UserPromptSubmit` 的 hook 拿不到，目前不會出現）。agent 與背景工作的通知不處理（實測它們也會觸發 `UserPromptSubmit`）。
5. **驗證與實測**（第 8 節）：`check-model-routing.mjs`（不花額度，68 項）；`bench-model-routing.mjs verify`（委派五個 subagent，從 session 紀錄讀實際的 model 與 effort，不同就報降級）；`bench-model-routing.mjs bench`（同一組任務讓五個 agent 各跑一次）。後兩個要 `--yes` 才執行，數字只取 Claude Code 回報的值。

## 否決的替代方案

- **只用三個 agent，委派時用 `Agent` 工具的 `effort` 參數調強度**：每次呼叫都要記得傳，沒有傳就靜悄悄地用 agent 的預設；frontmatter 是宣告式的，檢查腳本與 `verify` 都能驗證。使用者也指定了五個 agent。
- **用 `CLAUDE_CODE_EFFORT_LEVEL` 或 settings 的 `effortLevel` 統一設定**：它是整個 session 一個值，而且 `CLAUDE_CODE_EFFORT_LEVEL` 會蓋過所有 subagent 的 effort，正好和目的相反。
- **`fast-worker` 寫死 `claude-haiku-5-5`，確保 effort 生效**：違反使用者「用別名」的要求；Claude Code 2.1.293 起別名已經是 Haiku 5.5。舊版的情況由 `verify` 報成降級。
- **hook 用語言模型分類**：要呼叫外部 API、每個需求多花時間與額度；使用者要求不做不必要的外部呼叫。
- **用 `max` 當最高一級**：使用者指定 `xhigh`；`max` 留給主 session 由使用者自己選。
- **hook 用 Python**：同 ADR-019，這台機器的 `python.exe` 只是 Store 捷徑。

## 後果與驗證

- `check-model-routing.mjs`：68 項通過，包括 17 個範例需求分到預期的 agent（每一級至少兩個）、建議的 model 與 effort 等於 frontmatter、兩次失敗升級、成功後降級、最高一級不再升、通知不處理、狀態資料夾寫不進去時照樣輸出。
- `verify`（Claude Code 2.1.294）：五個 subagent 的 model 與 effort 都照設定套用，沒有降級；整個 session US$0.86（Claude Code 回報）。
- `bench`（三個任務 × 五個 agent × 1 次，Claude Code 2.1.294）：15 次全對，實際的 model 與 effort 都和設定相同；費用合計 fast-worker US$0.03、developer 0.76、developer-deep 0.66、architect 1.15、architect-deep 1.12。這三題太簡單，正確率分不出高下；費用幾乎都是啟動時的快取寫入（每次 41K–74K tokens），output 只有 12–536 tokens。細節在 `MODEL_ROUTING.md` 第 9 節。
- 每個 subagent 啟動時載入專案文件（49K–70K tokens）：委派有固定成本，規則仍是「只在值得時委派」。
- 舊版 Claude Code（2.1.292 以前）的 `haiku` 是 Haiku 4.5，`fast-worker` 的 effort 不會生效；`verify` 會報降級。
- hook 只看使用者的話判斷失敗與成功；Claude 在一個回合裡自己的失敗要自己計數（`MODEL_ROUTING.md` 第 4 節）。
