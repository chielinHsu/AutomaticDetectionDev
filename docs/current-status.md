# 目前狀態（進度表）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。原檔名 `STATUS.md`，2026-10-05 更名為本檔以符合共用結構（見 `docs/adr/000-ai-agent-structure.md`）。各步驟的完整內容在 `docs/steps/Sxx-*/` 與 `docs/changelog-ai.md`，本檔只留重點。

- 更新日期：2026-10-06
- 階段：S00–S06 完成；S07 待開始。S01–S06 都已用 `--no-ff` 合併到 `main` 並打 tag（`step-01-done`…`step-06-done`）；AI 共用結構、commit 格式規則與 S04 之後的〔適配寬度〕也已合併到 `main`（全部只在本機，**沒有 push**）。**S06 起用 Qt 6.12.0**（使用者 2026-10-06 更新；6.8.0 仍在機器上）。**等使用者決定的事**：（1）[ADR-009](adr/009-ui-toolkit-review-after-s02.md)：I1 在預算邊緣、真實視窗超標；（2）**U3 超出預算**（S04）；（3）T3 要不要涵蓋散佈的點（S05）；（4）**存檔／開啟配方的 UI 放進哪一步**（S06 新增，見待辦）；（5）S06 的指標（開圖、存檔）要不要有預算

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型（含樣本庫與 Board › Component › Box 階層） | ✅ 完成 | `step/01-ux-spec` | `step-01-done` |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ✅ 完成（⚠ I1 待使用者決定） | `step/02-ui-shell` | `step-02-done` |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ✅ 完成 | `step/03-core` | `step-03-done` |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ✅ 完成（⚠ U3 超標、待使用者決定） | `step/04-presenter` | `step-04-done` |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ✅ 完成 | `step/05-lattice` | `step-05-done` |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ✅ 完成 | `step/06-image-storage` | `step-06-done` |
| S07 | 底層 | OpenCV back end | ⏳ 下一個 | `step/07-opencv` | |
| S08 | 底層 | HALCON back end | ⬜ | `step/08-halcon` | |
| S09 | 底層 | 契約測試與 A/B 比較 | ⬜ | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## S01–S03（2026-10-05）

- **S01**：`docs/ux/ux-spec.md`、`tokens.json`、`copy.json`；ADR-001（Widgets + QGraphicsView）、ADR-002（復原模型）、ADR-006（樣本庫與 Board › Component › Box）。範圍擴大成兩種流程並存（教樣本、框任意區域與樣本庫比對）。使用者決定：UI 預算照原數字；庫與板子是**複本**（TRI 的 Merge）；庫存 **µm**＋影像校正值；接受／捨棄後 `Ctrl+Z` 回 Preview；`Esc` 捨棄不確認；訊息不蓋在影像上。只有文件與原型的驗證，沒有建置。
- **S02**：CMake＋presets（`/W4 /WX`）、影像畫布（零複製）、批次繪製層、四個工具、訊息列、主題與文案、`FakeScene`；ADR-007、008、009。**I1（5 000 框平移縮放）在預算邊緣**：offscreen 13 次 p95 14.1–20.7 ms（預算 16.7）、真實視窗 22.1 ms（ADR-009 待決定）。量測要釘核心、關電源節流（`MeasurementHygiene.h`）；整數像素的填色矩形比 cosmetic 描邊快約 4 倍。
- **S03**：domain 型別與座標慣例（ADR-010）、九個 port、`Result`／`CancelToken`、`JobRunner`（latest-wins）、use case 與 `classify`、`BackendRegistry` 與 `--backend`、假 back end、`PerfProbe`、`tools/check-deps.mjs`；ADR-003。取消的延遲上限是工作兩次看旗標之間的最長時間（p95 約 16–17 ms，Windows 計時器解析度）。

## S04、S05（2026-10-06）

- **S04**：互動規則在沒有 Qt 的 application 層（ADR-011）：`InteractionStateMachine`、`InspectionSession`、`ProposalSession`、`InspectionDocument`、`UndoStack`；`box_expander.h`；`QtThreadPoolExecutor`；`AutoRoiPresenter`、`ParamsPresenter`、物件樹、參數面板、`AppPresenters`；拖曳移動與方向鍵微調；ADR-012（虛線畫成填色矩形）。效能：U2、I3（餘裕只有 16–38%）、I4、I6 在預算內；**U3 超標：5 832 個元件 p95 46.4–60.7 ms（預算 16）**，成本是整個畫布的重繪，不是重新過濾。之後依使用者要求加了〔適配寬度〕（`W`，分支 `feat/fit-width`，沒有 tag）。
- **S05**：domain 第一次有 .cpp（`src/domain/lattice/`）：`LatticeEstimator`、`LatticePredictor`、`LatticeAccumulator`（O(1)）、`NonMaxSuppression`、`SpatialGrid`；ADR-013（寧可沒有晶格也不要錯的晶格）；`SimpleGrid` 刪除。1 029 種情境掃描、與 S00 示範頁交叉驗證 36 組一致。**T3（1 000 點）p95 0.65–1.19 ms，預算 2 ms，通過**；散佈的點 p95 3.1–4.2 ms 不在預算裡（待決定）。門檻要用真實影像校準（S07）；非矩形陣列沒有測。

## S06 的產出與發現（2026-10-06）

- **做了什麼**：`ImageView` 的所有權規則定案（ADR-014）：像素交出去就不改、owner 讓它活著、子影像共用 owner、做不到零複製就計數（`countImageCopy`）；`Bgr24`、`Gray16`、`significantBits`、`subView`、`ImageBuffer`。新 adapter `infrastructure/image_qt`（Qt Core＋Gui；`check-deps` 新增 `qt-gui` 類別）、`backend_opencv`（**OpenCV 4.9.0 第一次連結成功**，preset `release-opencv`／`debug-opencv`）。影像來源只交出 `Gray8`、`Gray16`、`Bgrx32`；組裝順序 **Qt 在前、OpenCV 在後**（計畫寫 Qt 當後備，改的理由是複製次數）。開圖改走 `IImageSource`（`ImageLoader`、`ImageViewAdapter` 刪除）。storage：JSON 的配方與樣本庫（ADR-015：格式標記、`schemaVersion`、逐版遷移、原子寫入、讀得嚴格、id 永不重複使用）；`RecipeSaver`（背景存檔）；`ImageSourceChain`；合成板子產生器（`tests/support/`）。細節：`docs/steps/S06-image-storage/`。
- **使用者在 S06 的決定**：（1）樣本庫照 **TRI 的分層**：專案 → 使用者 → 唯讀的系統（`LayeredLibraryStore`）；（2）**這一步不接存檔／開啟配方的 UI**，但它們是必要功能（見待辦）；（3）影像**也有 12 bit**：像素照原樣交給搜尋，畫面顯示前轉 8 bit，位元數看最大值推測；（4）**S06 就改用 Qt 6.12.0**。
- **驗證（都已實際執行，Qt 6.12.0）**：Release 45、Release＋OpenCV 47、Debug 45、Debug＋OpenCV 47 個 CTest 項目全過；qmake＋jom 建置可以執行；`check-deps` 0 違規、自我測試 15 個 fixture；植入 31 個缺陷抓到 30 個（M24 是等價變異）。Debug 抓到 Qt 6.8.0 `qUncompress` 的未定義行為（Release 碰巧通過）：參考影像改成不壓縮的 base64。
- **效能（Qt 6.12.0，Release，連跑 3 次的範圍；細節見 `docs/steps/S06-image-storage/perf.md`）**：S06 的指標沒有預算。Qt 解碼常見格式 0 次複製、OpenCV 讀彩色檔 1 次（計數，單元測試斷言）。5 MP 開圖（中位數）：BMP 28.8–32.9 ms、PNG 92.0–120 ms、JPEG 44.7–54.5 ms；25 MP 的 PNG 451–564 ms。5 000 個元件的配方：讀 29–39 ms、存 93.5–99.3 ms（另有 1 次 483 ms，原因不明）、背景存檔佔 UI 0.35–0.38 ms。有預算的指標在 6.12.0 重量：U4（p95 BMP 33.5–47.7、PNG 141–160、JPEG 51.6–63.9 ms，預算 200）與其他都在預算內，**U3 仍超標**（p95 19.5–29.4 ms，預算 16）。6.8.0 對 6.12.0 只比了一次，看不出 Qt 版本的差別。**預算數字沒有改。**
- **要留意的事**：（1）開圖仍在 UI 執行緒解碼（25 MP 的 PNG 約 0.5 秒停住）。（2）已知例外：16 階灰色表的 BMP（Qt 給 `Bgrx32`、OpenCV 給 `Gray8`）；灰階＋alpha 的 PNG 沒有驗證。（3）有效位元數是推測的，沒有覆寫的設定（配方或相機）。（4）分層樣本庫還沒有組裝進程式（各層的檔案放哪裡沒決定）；存一筆要整份重寫。（5）HALCON 的轉換與來源沒有做（沒有開發檔，S08）。
- **新增設計決定**：ADR-014（影像的所有權與複製）、ADR-015（配方與樣本庫的儲存）。`docs/plan/S07`–`S12` 開頭各加了「S06 完成後的提醒」。

## 預算校準狀態

`perf/budgets.json` 頂層仍是 `status: "proposed"`（T 類搜尋預算要等 S07）。T3 標 `calibrated`（S05）。UI 類：U1a、U1b、U4、I2、I5 標 `calibrated`，I1 標 `calibrated-at-limit`；S04 起 U2、I3、I4、I6 也標 `calibrated`，**U3 標 `measured-over-limit`**。**S06 沒有改 `budgets.json`**：S06 的指標（載入、複製次數、配方存讀）沒有預算，只記在 `docs/steps/S06-image-storage/perf.md` 與 `perf/baseline/casparhsu1-storage.json`；U4 在 Qt 6.12.0 重量過，記在 `perf.md`。S02–S05 的基準線（`perf/baseline/casparhsu1*.json`）是 Qt 6.8.0 的數字，只在這台機器可比較。

## 未解決事項

- **存檔與開啟配方的 UI（必要功能，沒有排進任何一步）**：儲存層與背景存檔在 S06 做好了，畫面上的〔儲存〕仍停用，也沒有〔開啟配方〕；開啟配方還需要 schema v2（記住影像路徑）。需要使用者決定放進哪一步。
- **S04 沒有涵蓋的 UI**：`LibraryPresenter`、`TypeEditorPresenter`（樣本庫面板、元件類型編輯器、流程 C）沒有任何一步的 plan 檔涵蓋；流程 B（區域掃描）的結果固定是「樣本庫是空的」；接受提案時的元件類型是暫時的（`X1…`）；沒有校正值的影像暫以 1 µm ＝ 1 像素展開框；晶格的信心與「建議檢查」還沒有顯示。
- ADR-006 未決：實例覆寫、Panel 多連板、角度正負號、窗的演算法與參數、自由公式。庫的分層 S06 已照 TRI 決定，各層的檔案位置未決。新預算 T7（區域掃描隨樣本庫大小 N）等 S07 實測後再提。
- **HALCON 只有 runtime，沒有 `include`／`lib`（找不到 `HalconCpp.h`）：S08 前要補裝開發元件**。HALCON「每個條目一個 shape model」是預期做法，未驗證；ADR-010 的 OpenCV／HALCON 座標轉換未驗證（S07、S08）。
- 真實板子影像（去識別化 1–2 張）— S07 開工時請使用者提供，並問 12 bit 的影像怎麼存（16 bit 容器還是緊密打包的 TIFF）。
- 設定記憶（主題、字級、語言）、減少動態、高 DPI 的實際外觀：**沒有做**。`QBENCHMARK` 沒有用到也沒有驗證（ADR-003）。
- `AiInspTool.pro`：S06 納入 `image_qt` 與 `storage`（萬用字元）；Qt 6.12.0 的命令列 qmake ＋ jom 建置過、執行檔可以啟動；**沒有在 Qt Creator 裡實際載入過**；OpenCV、HALCON 還沒有納入（S07、S08）。
- AI 共用結構與 `ai-project-template/`（2026-10-05 套用並合併，見 `docs/adr/000-ai-agent-structure.md`）：stage 時一律明確列出路徑；Codex 端的流程沒有在本機端到端測過（該機器的 Codex CLI 無法執行 `codex exec`）。
