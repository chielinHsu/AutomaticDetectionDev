# 設計決策索引（ADR）

> 本專案一檔一決策：每個決策是 `docs/adr/NNN-slug.md`，本檔只是索引，新的放最上方。內容只寫在 ADR 檔，不要在這裡複製。
> 已記錄的決定，沒有新證據不推翻；要推翻就新增一筆，並在新舊兩筆都標明取代關係。

預定的 ADR（004、005）列在各步驟的 plan 檔（`docs/plan/Sxx-*.md`），寫成後再加進下表。S02 新增 007、008、009；S03 寫成預定的 003，並新增 010（座標與角度慣例）；S04 新增 011、012；S05 新增 013；S06 新增 014、015；S07 新增 016，並在 014、015 加了「後續（S07）」一節；S08 新增 017，並在 010、014 加了「後續（S08）」一節。2026-10-07 建置改 qmake，新增 018，並在 003、017 加了「後續」一節。2026-10-08 模型路由，新增 019，並在 000 加了「後續」；同一天加上推理強度，新增 020（部分取代 019）。同一天 S09 之前的 session B：back end 的提醒改成卡片與狀態列的標示（017 加「後續（2026-10-08）」），執行中切換 back end 新增 021。編號 004（S09）、005 仍預留。

| 編號 | 標題 | 狀態 | 日期 |
|---|---|---|---|
| [ADR-021](adr/021-runtime-backend-switching.md) | 執行中切換 back end：`SearchEngine`（back end ＋ use case，`shared_ptr` 共用，搜尋工作持有一份，舊的 adapter 等工作結束才釋放）、application 層的 `BackendSwitch`（一次換一個、不可用的不嘗試、失敗回到原本的、搜尋中先作廢、換好之後用新的重搜同一個區域）、在背景執行緒建立、失敗時訊息列的卡片、啟動時用記住的那一個（`QSettings` 的 INI；命令列指定的照舊嚴格） | 採用（要做的事與三個選擇由使用者決定；做法是 Claude Code 的提案） | 2026-10-08 |
| [ADR-020](adr/020-adaptive-model-effort-routing.md) | 模型與推理強度的路由（Claude Code 專用）：複雜度、風險、推理深度、不確定性 1–5 分加上判準，範圍與驗證成本；五個 subagent 用 frontmatter 的 `model` 與 `effort`（`fast-worker` haiku/low、`developer` sonnet/medium、`developer-deep` sonnet/high、`architect` opus/high、`architect-deep` opus/xhigh）；兩次失敗升一級、解決後降級；hook 回傳建議的 agent、model、effort 與分數；`bench-model-routing.mjs` 從 session 紀錄驗證實際的 model 與 effort | 採用（要求由使用者提出；判準、判斷順序、狀態、驗證與實測腳本是 Claude Code 的提案）；部分取代 ADR-019 | 2026-10-08 |
| [ADR-019](adr/019-model-routing.md) | 模型路由（Claude Code 專用）：根目錄的 `MODEL_ROUTING.md`（`CLAUDE.md` 匯入）、`.claude/agents/` 的 `fast-worker`（haiku，唯讀）、`developer`（sonnet）、`architect`（opus，唯讀）、`UserPromptSubmit` hook（Node，關鍵字分類，只注入提示、不切換主模型、失敗不擋）、`scripts/ai/check-model-routing.mjs` | 採用；判準與 subagent 的組合被 ADR-020 取代；「`haiku` 對應 Haiku 4.5」只對 Claude Code 2.1.292 成立 | 2026-10-08 |
| [ADR-018](adr/018-qmake-only-build.md) | 建置只用 qmake：`AiInspTool/AiInspTool.pro` 包含所有程式庫、程式與測試，移除 CMake；`.pro` 對照 `src/`、`tests/` 放在 `AiInspTool/` 底下、依賴表 `aoi_deps.pri`、OpenCV 與 HALCON 預設開（找不到就報錯）、執行檔 `AiInspTool.exe`（GUI 子系統）、測試的環境編進執行檔、測試清單在 `tests.pro`（`test.ps1` 照清單跑） | 採用（建置方式、執行檔名、預設編進 OpenCV 與 HALCON 由使用者決定）；取代 ADR-003 的「經 CTest 執行」 | 2026-10-07 |
| [ADR-017](adr/017-halcon-shape-model-backend.md) | HALCON back end：shape model（角度步進自己算、模型範圍兩端多 2 步、least_squares）、`halconcpp.dll` 延遲載入、授權的判斷與「runtime 授權、SDK 的 dongle 沒插」的提醒、位姿換算（`affine_trans_pixel` 的修正）、`interrupt_operator` 取消、流程 B 一次 `find_shape_models`；開發檔放 `third_party/halcon` | 採用（調整參數是起點；比 OpenCV 好不好由 S09 判斷）；建置設定改由 qmake（ADR-017 的後續、ADR-018） | 2026-10-07 |
| [ADR-016](adr/016-opencv-coarse-to-fine-search.md) | OpenCV 搜尋：由粗到細的 NCC（頂層全部角度、往下逐層精修、爬坡、拋物線內插）、小的分數圖自己算（積分影像）、OpenCV 與 JobRunner 共用執行緒數（核心數 − 1）；流程 B 每個類型建一次範本 | 採用（調整參數是起點） | 2026-10-06 |
| [ADR-015](adr/015-recipe-and-library-storage.md) | 配方與樣本庫的儲存：有版本的 JSON（嚴格讀取、逐欄位往返、遷移）、原子寫入、背景存檔（同一個檔案依序寫）；樣本庫一個檔案一層，照 TRI 分層（專案 → 使用者 → 唯讀的系統） | 採用（分層由使用者決定；S07 升到 schema v2：`board.significantBits`） | 2026-10-06 |
| [ADR-014](adr/014-image-ownership-and-copies.md) | 影像的所有權（像素交出去就不改、owner 讓它活著）、新增 `Bgr24`／`Gray16`、影像來源只交出 `Gray8`／`Gray16`／`Bgrx32`、複製的定義與計數、一個函式庫一個 adapter（新增 `image_qt`）、Qt 在前 OpenCV 在後、校正值不從檔案讀 | 採用；S07 後續：12 bit 是 16 bit 容器、值 0..4095（使用者回答），有效位元數可以覆寫（使用者決定做法）；S08 後續：HALCON 的轉換實作並驗證（緊密排列零複製），HALCON 的影像來源沒有做 | 2026-10-06 |
| [ADR-013](adr/013-lattice-acceptance-rules.md) | 晶格的接受規則：寧可沒有晶格，也不要錯的晶格（證據、補缺格的證據、二維成立的逐列判斷、連通、信心；不往外推） | 採用（門檻是起點，S07 用真實影像校準） | 2026-10-06 |
| [ADR-012](adr/012-dashed-frames-as-fill-rects.md) | 虛線與點線的框也畫成整數像素的填色矩形（取代 S02「虛線狀態仍用描邊」的做法；理由是 S04 量測推翻了「這兩種狀態的框很少」） | 採用 | 2026-10-06 |
| [ADR-011](adr/011-interaction-logic-in-application-layer.md) | 互動規則放在 application 層（沒有 Qt 的 `InspectionSession`），Presenter 只做 Qt 這一側 | 採用 | 2026-10-06 |
| [ADR-010](adr/010-coordinate-and-angle-convention.md) | 座標與角度慣例：影像像素、y 向下、角度為度、螢幕上逆時針為正（解決 ADR-006 未決 #5） | 採用（OpenCV 端 S07、HALCON 端 S08 實測並以測試鎖住；S09 的契約測試對兩邊跑同一組） | 2026-10-05 |
| [ADR-003](adr/003-test-framework.md) | 測試框架：統一用 Qt Test（涵蓋 domain 到 UI），效能量測自己用 `steady_clock`，不用 `QBENCHMARK` | 採用；「經 CTest 執行」由 ADR-018 取代（改由 `scripts/dev/test.ps1`） | 2026-10-05 |
| [ADR-009](adr/009-ui-toolkit-review-after-s02.md) | S02 實測後重審 UI 工具集：I1 在預算邊緣、真實視窗超標；選項 A 維持、B OpenGL 視埠、C QML、D 調預算 | **提案（待使用者決定）**；決定前 ADR-001 照舊有效 | 2026-10-05 |
| [ADR-008](adr/008-message-bar-fixed-height.md) | 訊息列固定高度（畫布大小不因訊息而變；放不下就在列內捲動） | 採用 | 2026-10-05 |
| [ADR-007](adr/007-copy-json-as-translation-source.md) | 文案：`copy.json` 直接當翻譯來源（`JsonTranslator`＋`qtTrId`，不產生 `.ts`／`.qm`；佔位字維持具名） | 採用 | 2026-10-05 |
| [ADR-006](adr/006-sample-library-and-hierarchy.md) | 樣本庫與 Board › Component › Box 階層（流程 A 教樣本、流程 B 區域掃描、元件類型編輯） | 採用（核心方向、單位 µm、庫與板子是複本關係已由使用者決定；其餘未決見 ADR） | 2026-10-05 |
| [ADR-002](adr/002-undo-model.md) | 復原模型：Command ＋ 三個範圍（提案、類型編輯、文件） | 採用 | 2026-10-05 |
| [ADR-001](adr/001-ui-toolkit.md) | UI 工具集：Qt Widgets + QGraphicsView | 採用 | 2026-10-05 |
| [ADR-000](adr/000-ai-agent-structure.md) | AI agent 共用結構（Codex 與 Claude Code） | 採用 | 2026-10-05 |
