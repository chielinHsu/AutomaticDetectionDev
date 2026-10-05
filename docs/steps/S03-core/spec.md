# S03 — 系統核心：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S03-core.md`](../../plan/S03-core.md)、[ADR-006](../../adr/006-sample-library-and-hierarchy.md)、[`ux-spec.md`](../../ux/ux-spec.md) 第 12.1 節。本檔只講「為什麼這樣切」；實測數字在 [`perf.md`](perf.md)；決策在 [ADR-003](../../adr/003-test-framework.md)、[ADR-010](../../adr/010-coordinate-and-angle-convention.md)。

<!--STE-->
## Purpose
This step builds the core of the program without a screen.
It defines the ports, the use case and the job runner.
It adds a fake back end, a performance probe and a dependency check.
<!--/STE-->

**繁中**：做出沒有畫面的程式核心：port（介面）、use case、工作執行器；再加上假 back end、PerfProbe 與依賴檢查。

<!--STE-->
## Reason
The core must not know the screen or the vision library.
Then we can test it alone and swap its parts.
We also need proof that the layers stay apart.
<!--/STE-->

**繁中**：核心不認識畫面也不認識視覺函式庫，所以能單獨測試、可抽換；而且要有「分層真的守住了」的證據，不是靠自覺。

<!--STE-->
## Effect
We run the whole use case with a fake searcher in a test.
A new back end needs no change to the use case.
The dependency check fails when a layer breaks the rules.
<!--/STE-->

**繁中**：用假的 searcher 就能在測試裡跑完整個 use case；新增 back end 不必改 use case（`tst_backend_registry` 用一個全新的 back end 實證）；任何一層違反依賴規則，`check-deps` 就讓測試失敗（13 個 fixture 驗證它真的會失敗）。真正的搜尋要等 S07、S08。

<!--STE-->
## Ports
A port is a small interface with five methods or fewer.
The use case calls ports and never calls a library.
An adapter implements a port.
Only the composition root knows both sides.
<!--/STE-->

**繁中**：port 是窄介面（每個 ≤ 5 個方法）。use case 只呼叫 port，不呼叫任何函式庫；adapter 實作 port；只有組裝根（`app/`）同時認識兩邊。九個 port：`ITemplateBuilder`、`IPatternSearcher`、`IImageSource`、`IRecipeStore`、`IProgressSink`、`IPerfProbe`、`IExecutor`，以及 S01 加的 `ILibraryStore`、`ILibraryMatcher`。後面四個與 `IImageSource`、`IRecipeStore` 目前只有介面與契約（S06、S07 才有實作）。每個 port 的契約（可重入、不丟例外、取消怎麼生效、回傳的排序與範圍）寫在標頭檔裡，S09 把它變成對每個 back end 都跑的契約測試。

<!--STE-->
## One rule for errors
A port never throws an exception.
It returns a result that holds a value or an error.
A search with no hits is not an error.
It is a normal result with a reason.
<!--/STE-->

**繁中**：`Result<T>`（成功帶值、失敗帶 `Error`＋錯誤碼）；例外不穿過 port，邊界（`JobRunner`、adapter）把例外攔下變成 `Failed`／`Internal`。**「沒有結果」不是錯誤**：樣本太小、對比太低、只有一個、門檻太高，都是成功的結果加上 `NoMatchReason`（ux-spec 第 10 節那張表），畫面才能依原因給下一步建議。

<!--STE-->
## The latest job wins
The user can start a new search before the old one ends.
The job runner raises the cancel flag of the old job at once.
It does not wait for the old job to stop.
The screen drops a late result by its job number.
<!--/STE-->

**繁中**：`JobRunner` 的 latest-wins：新工作 `submit` 時舊工作的旗標立刻升起，不等它停下（預算 I4 量的是 UI 回到 Idle，不是 worker 停下）；舊工作的 `JobReport` 是 `Cancelled` 且 `latest ＝ false`，`usable()` 為假。排隊中就被取代的工作根本不執行。取消是協作式的：工作要自己在安全的地方看旗標；單次函式庫呼叫無法中斷，所以取消延遲的上限是「最長一次不可中斷的呼叫」。`~JobRunner` 先 `cancel()` 再等所有已提交的工作結束。

<!--STE-->
## Candidates and thresholds
The use case returns every candidate with its score.
The score floor is 0.30.
The screen applies the threshold.
A new threshold needs no new search.
<!--/STE-->

**繁中**：use case 一次回傳**所有**候選與分數（≥ 搜尋下限 0.30），分數門檻由 `classify()`（純函式）套用：拖門檻滑桿只需重新分類，不碰任何 port（預算 U3 的前提；`changingTheThresholdNeverSearchesAgain` 驗證）。「降低門檻到 {suggested}」的建議值取到小數第二位、向下取整，保證最高分的候選過得了新門檻（`aSuggestedThresholdMakesTheBestCandidatePass` 驗證）。分兩階段：種子搜尋用較高的下限（0.70），晶格預測出的缺格再用小視窗以較低的下限（0.30）驗證；視窗裡找不到的是 Inferred（沒有分數，不偽造）。

<!--STE-->
## A fake back end
The fake back end builds a grid around the sample.
A hash of the seed and the cell decides each cell.
The result is the same on every run.
It has no vision code.
<!--/STE-->

**繁中**：假 back end 以使用者框的樣本為中心排一個合成網格（間距是樣本大小的倍數，不管影像多大都貼著樣本）。每個格子是缺件、低分還是正常，由 `(seed, i, j)` 的雜湊決定，與走訪順序、呼叫次數、執行緒無關，所以完全可重現。它不看影像內容；`--backend=fake`（預設）在 S04 之前讓整個 UI 流程有東西可跑。測試的標準答案來自同一份產生器（`makeFakeGrid`），所以「世界是什麼樣子」是已知的，被測的是 use case 怎麼把它找出來。

<!--STE-->
## The dependency check
A script reads every include in the source folder.
It compares each include with a table of rules.
A wrong include fails the test.
A fixture with a wrong include proves that the check works.
<!--/STE-->

**繁中**：`tools/check-deps.mjs` 讀 `src/` 的每個 `#include`，對照 `docs/architecture.md` 的依賴表（規則表在腳本裡，兩邊要一致）。13 個 fixture（`tests/core/fixtures/deps/`）各自宣告「應該報哪幾行」，腳本的 `--self-test` 比對集合完全相同：該報的沒報、不該報的卻報了，都算失敗；另有一個 `WILL_FAIL` 的 CTest 項目證明「違規的樹 → 結束碼非 0」。限制：只看 `#include` 文字（CMake 的連結關係是另一道防線）；裸 Qt 標頭靠白名單分 Core 與 Widgets。

<!--STE-->
## The coordinate rule
The origin is the top left corner of the image.
The x axis points right and the y axis points down.
A positive angle turns counter clockwise on the screen.
The unit of angle is the degree.
<!--/STE-->

**繁中**：影像像素、原點在左上角像素的左上角、x 向右 y 向下、角度為度、螢幕上逆時針為正、正規化到 (−180, 180]；樣本庫存 µm，影像帶 `umPerPixel`。寫在 `src/domain/geometry.h`，理由與否決的做法見 [ADR-010](../../adr/010-coordinate-and-angle-convention.md)。**OpenCV 與 HALCON 端的轉換是文件上的假設，沒有驗證**：S07、S08 實測，S09 用旋轉過的合成影像鎖住。

<!--STE-->
## What this step does not do
This step does not search a real image.
It does not read a file, build a library or draw a box.
Steps S05 to S08 add the real parts.
<!--/STE-->

**繁中**：本步驟不做真實搜尋、不讀檔、不做樣本庫儲存、不畫框。晶格擬合是**簡化的暫代版**（`SimpleGrid`，S05 取代）；流程 B（區域掃描）只有 port 與型別，沒有 use case；畫面還沒有接上核心（S04）。

## 元件與檔案

| 層 | 內容 |
|---|---|
| `domain`（只有標頭） | `geometry.h`（座標慣例、`Rect`、`Pose2D`、`RectObj`）、`image_view.h`（加了 `umPerPixel`）、`match.h`（`Match`、`Candidate`、`InspectionObject`、`ComponentCandidate`）、`object_state.h`（`ObjectState`、`ObjectOrigin`、`NoMatchReason`、`NoMatchInfo`）、`search_params.h`（`SearchParams`、`SearchRegion`）、`component.h`（`Board`、`ComponentInstance`、`ComponentType`、`BoxGroup`）、`recipe.h` |
| `application` | `Error.h`、`Result.h`、`CancelToken.h`；`ports/` 九個介面；`jobs/JobRunner`；`usecases/GenerateObjectsFromSample`、`ProposalView`（`classify`）、`SimpleGrid`（暫代）；`backend/BackendRegistry` |
| `infrastructure` | `backend_fake/`（`FakeGrid`、`FakeBackend`）；`perf/PerfProbe`；`threading/ThreadPoolExecutor`、`InlineExecutor` |
| `app` | `CompositionRoot`（`makeBackendRegistry`、`composeCore`）、`main.cpp` 的 `--backend` |
| 工具與測試 | `tools/check-deps.mjs`；`tests/core/`（8 個單元測試執行檔、`tst_cli`、`perf_core`、13 個依賴檢查 fixture） |

## SOLID 對照

| 原則 | 這一步的具體做法 | 驗證 |
|---|---|---|
| **S** | `JobRunner` 只管排程與 latest-wins；`GenerateObjectsFromSample` 只管流程 A 的階段順序；`SimpleGrid` 只管晶格；`classify` 只管門檻 → 狀態；`PerfProbe` 只管記錄與輸出；`BackendRegistry` 只管註冊與建立。最大的檔案 232 行（`SimpleGrid.cpp`），沒有超過 300 行的 | 程式審查（行數）；每個類別一個測試檔 |
| **O** | 新增 back end ＝ 一組 adapter ＋ 在組裝根註冊一次；use case、domain、`BackendRegistry`、`JobRunner` 都不改 | `tst_backend_registry::aBrandNewBackendRunsTheWholeUseCaseUnchanged`：測試裡定義一個全新的 back end，註冊後經 `composeCore`、`JobRunner` 跑完整個 use case |
| **L** | `FakeSearcher` 遵守 `IPatternSearcher` 的契約（排序、範圍、視窗限制、取消、不丟例外、可重入）；測試裡的 `ScriptedSearcher` 是第二個實作，use case 一樣能用 | `tst_fake_backend`（S09 把這組斷言泛化成對每個 back end 都跑的契約測試） |
| **I** | 每個 port ≤ 5 個方法：`ILibraryStore` 4、`ITemplate` 3、其餘 1–2；`ProgressSink` 只有 `report` | 程式審查（方法數） |
| **D** | use case 只 include port；`aoi_application` 不連任何 adapter 的 target；adapter 只在 `app/CompositionRoot.cpp` 組裝 | `check-deps`（application 不得 include infrastructure）、CMake 連結關係、`tst_backend_registry` |

## 決策審查（engineering-workflow 第 3 節）

| 面向 | Decision | Evidence | Verification |
|---|---|---|---|
| 架構與介面 | 見 SOLID。`ITemplate` 是抽象類別（不是 `void*`），搜尋器用 `dynamic_cast` 加 `backendId()` 拒絕別家的範本；`Backend` 是「一組配套的 adapter」，由工廠一次建出，所以範本與搜尋器能共用內部狀態 | `tst_fake_backend::aTemplateFromAnotherBackendIsRefused`（連 `backendId` 都冒用的外來範本也被擋下） | 同左；`tst_generate_objects::aTemplateFromAnotherBackendIsRefused` |
| 資料契約與生命週期 | `ImageView` 以共享擁有者跨 port；`SampleProposal` 是值型別快照（執行緒之間只傳它）；範本是 `shared_ptr<const ITemplate>`；`SampleRequest` 以值捕獲進工作，影像在工作結束前一定活著 | `JobRunner` 的 work 以值捕獲 request（`tst_backend_registry` 的端到端） | 端到端經 `JobRunner` 跑通；`ImageView` 欄位意義未改（只新增 `umPerPixel`，預設 0） |
| 所有權與資源釋放 | `CoreServices` 的成員宣告順序＝建立順序，解構反向：`JobRunner` 先停並等進行中的工作，之後才是 use case、back end、執行緒池、probe；`~JobRunner` 先 `cancel()` 再等 `pending ＝ 0` | `app/CompositionRoot.h` 的註解與成員順序 | `destroyingTheRunnerWaitsForPendingJobs`、`queuedJobsAlsoFinishBeforeTheRunnerIsGone`、`composingTheFakeBackendWorks` |
| 並行、順序與狀態 | latest-wins 時新舊工作會重疊，所以 **port 必須可重入**（契約寫在標頭）；use case 與假 back end 無狀態；`JobRunner` 的共享狀態（最新編號、取消來源、待處理數）由一把互斥鎖保護；`latest` 在 worker 上算，Presenter 轉回 UI 執行緒後要再檢查 | 契約寫在 `ITemplateBuilder.h`／`IPatternSearcher.h`；`JobRunner.h` 的註解 | `severalThreadsCanSearchWithTheSameObjects`、`concurrentSubmissionsAreAllAccountedFor`（4 執行緒 × 100 次 submit） |
| 可靠性與錯誤處理 | `Result<T>`；邊界攔例外（`JobRunner` 攔 work 與 done、`ThreadPoolExecutor` 攔逃出來的例外、adapter 攔函式庫例外）；無結果是成功加原因 | 見上方「One rule for errors」 | `anExceptionInTheWorkBecomesAFailedReport`、`anExceptionInDoneIsSwallowed`、`aThrowingTaskDoesNotKillTheWorker`、`sampleProblemsStopBeforeAnySearch` |
| 安全與信任邊界 | 本步驟唯一的外部輸入是命令列 `--backend` 與呼叫端給的 `PerfProbe::writeJson` 路徑；沒有檔案解析、沒有網路。`IRecipeStore`、`ILibraryStore`、`IImageSource` 的契約已寫明「損壞檔案回 `FormatInvalid`，不當機」 | `tst_cli`（未知與不可用的 id 結束碼 2）；`writeJsonToAMissingFolderIsAnIoError` | **未做**：真正的檔案解析在 S06，惡意檔案的模糊測試也在那裡 |
| 效能與容量 | 先寫簡單版、量測再下結論（`perf.md`）。`PerfProbe` 關閉時的成本在量測誤差內（見 perf.md）；暫代的 `SimpleGrid` 在約 4 900 點要 97–134 ms（3 次執行的中位數範圍；O(n × 800)），遠超 T3 的目標（1 000 點 ≤ 2 ms）：**這是 S05 的目標，這一步不優化暫代品** | `perf.md` 的 P1–P4 | `perf_core`（Release 才把「關閉 < 1 µs」當失敗條件） |
| 交付、相容性與可觀測性 | `--backend` 預設 `fake`；不可用的 back end 在啟動時明確失敗（stderr、結束碼 2，附可用清單），**不**退回別的 back end；`AiInspTool.pro` 同步納入新的層（qmake 命令列建置驗證過，沒有在 Qt Creator 載入過）；`PerfProbe` 可輸出 JSON | `tst_cli`；`qmake` ＋ `jom` 命令列建置 | 見 perf.md 與交接單 |
| 測試與驗證 | Qt Test（[ADR-003](../../adr/003-test-framework.md)）；變異測試確認關鍵測試會失敗（結果在 `perf.md` 同層的交接單與 changelog） | `tests/core/` | `scripts/dev/test.ps1`（Release 與 Debug） |

## 與計畫的差異（誠實紀錄）

- **新增 ADR-010**（座標與角度慣例）：計畫只列了 ADR-003；ADR-006 把角度正負號留給 S03 固定，所以多寫一份。
- **流程 B 只有 port 與型別**：`ILibraryStore`、`ILibraryMatcher`、`ComponentCandidate`、`NoMatchReason` 的後五項有了，沒有 use case、沒有假實作（S06、S07）。
- **晶格擬合是暫代版**（`SimpleGrid`）：最近 6 個鄰居找基底向量、單次最小平方法、只補索引範圍內的缺格；已知限制寫在標頭並由 `tst_grid` 的測試「看見」（間距差 20 倍會被當成一排）。S05 取代。
- **假 back end 不看影像**：使用者在任意影像上框樣本，它都排出貼著樣本的網格，並沒有「找到相似的元件」。S07 才有真的搜尋。`FakeSpec::forcedSampleProblem` 讓測試能模擬「框到多個元件」這種只有真的 back end 才看得出來的問題。
- **`PerfProbe` 只用標準庫**，沒有用 Qt Core（依賴表允許）；`ThreadPoolExecutor` 同理。
- **`aoi` 啟動時組裝核心但沒有用它**：`main.cpp` 呼叫 `composeCore` 只為了驗證 `--backend`，S04 的 Presenter 才會拿它去用。
- **`main.cpp` 的啟動錯誤改寫到 stderr**（原本是 `qCritical`）：Qt 在 Windows 上只有 stderr 是主控台時才寫到 stderr，被腳本或管線接走時使用者與測試都看不到訊息（實測）。
- **還沒有在 Qt Creator 裡載入 `AiInspTool.pro`**：只驗證了命令列 qmake ＋ jom 建置。
