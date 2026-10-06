# S07 — OpenCV back end：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S07-opencv.md`](../../plan/S07-opencv.md)（含 S01、S03–S06 的提醒）、port 的契約（`ITemplateBuilder.h`、`IPatternSearcher.h`、`ILibraryMatcher.h`）、ADR-010、ADR-013、ADR-014。
> 本檔講「為什麼這樣做」；決策在 [ADR-016](../../adr/016-opencv-coarse-to-fine-search.md)（搜尋）與 [ADR-014 的 S07 後續](../../adr/014-image-ownership-and-copies.md)（有效位元數）；圖在 [`diagram.md`](diagram.md)；實測數字在 [`perf.md`](perf.md)。

<!--STE-->
## Purpose
This step builds the OpenCV back end. It builds the template and searches the image for similar parts.
It also builds the matcher for the sample library.
The user can now set the bit depth of the image.
<!--/STE-->

**繁中**：做出第一個真正看影像的 back end：範本建立器、搜尋器（流程 A），以及樣本庫比對器（流程 B 的 adapter，UI 先不接，使用者決定）。另外照使用者的決定，讓使用者可以指定影像的有效位元數（暗的 12 bit 影像會被推測錯）。

<!--STE-->
## Reason
The fake back end does not look at the image. So the program could not find real parts.
OpenCV is free, and it gives a speed and accuracy baseline for HALCON.
A dark 12-bit image looks like a 10-bit image. Then the screen is too bright, and the contrast check is wrong.
<!--/STE-->

**繁中**：S03–S06 的流程靠假 back end 跑通，它不看影像。OpenCV 免費，而且是 S08（HALCON）的速度與準確度基準。使用者的影像是 12 bit（16 bit 容器、值 0..4095，使用者 2026-10-06 回答）；暗的 12 bit 影像最大值 ≤ 1023，看最大值會推測成 10 bit：畫面亮 4 倍、use case 的對比檢查也放大 4 倍。

<!--STE-->
## Effect
The whole flow works on a real image, from the sample box to the inspection boxes.
On synthetic 5 MP boards, the search finds all parts. The position error is about 0.1 pixels.
The seed search, the window search and the template build are inside their budgets.
<!--/STE-->

**繁中**：整條流程第一次在真的影像上跑通。合成板子（5 MP、8 種情況、6 673 個元件）：召回率 100%、位置誤差 p95 約 0.1 像素；T1（範本）、T2a（5 MP 種子搜尋）、T4（200 個視窗）都在預算內（數字見 `perf.md`）。使用者的兩張真實影像（24.4 MP、12 bit）：整個 use case 0.11–0.21 秒；針腳找得到，但也看到兩個要使用者決定的問題（下面「真實影像」）。

<!--STE-->
## How the search works: coarse to fine
A full image has millions of positions. Each position needs a test at each angle.
So the program makes the image and the template smaller first.
At the smallest level, it tests all positions and all angles.
The best local scores become candidates.
At each larger level, it tests only near each candidate.
At the full size, it uses three scores to find the position between pixels.
<!--/STE-->

**繁中**：比對分數是正規化相關係數（`TM_CCOEFF_NORMED`，對亮度的倍數與偏移不敏感）。金字塔每層縮小一半，直到範本的短邊剩 8 像素（64×32 → 32×16 → 16×8）。頂層：每個角度一次 `matchTemplate`，每個位置取最好的角度，分數 ≥ `minScore − 0.2` 的局部最大值是候選。往下每一層：每個候選在 ± 2 像素、上一層的角度 ± 一步之內再比；最好的點在範圍邊上就往那邊移再比（爬坡）。第 0 層：x、y 各用三點拋物線內插到次像素，角度也用相鄰兩個角度的分數內插（兩側沒比過就補比）。準確度：合成板子的位置誤差 p95 約 0.1 像素；角度誤差約是第 0 層步進的 0.2 倍（48×28 的範本最大 0.42°，範本角落位移約 0.2 像素），來源是 NCC 對角度的曲線在真值兩側不對稱。最後用 domain 的 NMS 合併同一個元件、依分數排序。

<!--STE-->
## Angles
Each level has its own list of rotated templates. The step makes a corner move one pixel.
A large template needs small steps. A small template needs large steps.
A wide angle range needs more angles. So it takes more time.
<!--/STE-->

**繁中**：旋轉的範本在建範本時就做好（每層 × 每個角度），步進讓範本的角落移動 1 像素：64×32 在原尺寸 1.6°、頂層 16×8 是 6.4°，所以 ±5° 在頂層只要 3 個角度。旋轉的範本不是把裁好的矩形轉一轉（四個角會空），而是先裁一塊含周圍影像的方形、縮小、轉過再切。角度範圍越大越慢：±180° 的種子搜尋約是 ±5° 的 4 倍（`perf.md` 的 A-*）。角度的正負號照 ADR-010（螢幕上逆時針為正），用傾斜 ±3° 的合成板子鎖住。

<!--STE-->
## Small score maps
Near a candidate, the program tests only a few dozen positions.
For this small work, the OpenCV function has a large fixed cost.
So the program calculates these small scores itself. It uses sums of the image to make this fast.
<!--/STE-->

**繁中**：第一版每一層都用 `cv::matchTemplate`，5 MP 的種子搜尋 193 ms（預算 150），其中 224 ms 的工作（多執行緒的總和）是精修：分數圖只有 5×5，`matchTemplate` 仍走 DFT，固定成本主宰。改成自己算（`OpenCvCorrelation`：8 bit 整數點積，視窗的 ΣI、ΣI² 用積分影像、同一塊比好幾個角度時共用，範本的 ΣT、ΣT² 建範本時算好），定義和 OpenCV 相同（測試比對差 < 10⁻⁴），T2a 降到 64 ms、T4 從 87 降到 30 ms（開發中各量 1 次），找到的元件完全相同；最終 3 次的 T2a p95 是 67–72 ms、T4 是 31–33 ms。大的分數圖（頂層的整張影像）仍用 `matchTemplate`。

<!--STE-->
## Window search
The lattice predicts the missing parts. The program then searches small windows at these places.
For each window, it converts and searches only the pixels near the window.
<!--/STE-->

**繁中**：視窗清單（use case 的驗證階段）的每個視窗是一塊獨立的區域：視窗外加範本的一半，只轉換、只掃這些像素；每個視窗最多精修 4 個頂層候選（流程 B 的大區域依面積放寬）。匹配的中心一定在某個視窗內（契約）。

<!--STE-->
## Threads
OpenCV runs its loops on its own threads. The job runner also has threads.
Both use the number of cores minus one. One core stays free for the screen.
Only one search runs at a time. So the two groups do not fill every core together.
<!--/STE-->

**繁中**：頂層的橫帶、精修的候選、視窗清單的視窗用 `cv::parallel_for_`。組裝根在建立 OpenCV back end 時 `cv::setNumThreads(核心數 − 1)`，和 JobRunner 的 worker 數用同一個函式（`app::workerThreads()`）。latest-wins 時只有一個搜尋在跑，被取代的舊搜尋在下一個檢查點就停。實測 5 MP 的種子搜尋（中位數，3 次的範圍）：1 個執行緒 126–133 ms、4 個 65–83 ms、15 個 50–57 ms。

<!--STE-->
## Gray values and the bit depth
The search compares 8-bit gray values. A 16-bit image gets a scale from its bit depth.
The program guesses the bit depth from the largest value. A dark 12-bit image gives a wrong guess.
So the user can set the bit depth in the parameter panel.
The board keeps this setting, and the next image uses it too.
The program refuses a setting that is smaller than the values in the image.
<!--/STE-->

**繁中**：搜尋器只比 8 bit 灰階：`Gray8` 零複製，`Gray16` 用 `v × 255 ／（2^位元數 − 1）` 換算、彩色轉灰階（各算一次複製）。位元數來自 `ImageView::significantBits`。**使用者 2026-10-06 決定**：設定存在板子裡（`Board::significantBits`，配方 schema v2）、換影像沿用（相機的設定）、UI 是參數面板的下拉選單「影像位元數」（自動（偵測：N bit）／8／10／12／14／16 bit；不是 16 bit 灰階就停用）；改了立刻重畫，已顯示的提案不重新搜尋。規則是 domain 的純函式 `decideSignificantBits`：0 ＝ 自動；**比推測的小就不收**（影像裡有值會被截掉，多半是選錯），用吐司說明。畫布、use case 的對比檢查、OpenCV 搜尋器都讀 view 的欄位，所以 Presenter 在開圖與改設定時套到 view 上就夠了（零複製）。

<!--STE-->
## The sample library matcher
The region scan compares a region with the reference image of each part type.
The program builds a template from each reference and searches the region.
Matches at the same place become one part with a score for each type.
This cost grows with the number of types.
<!--/STE-->

**繁中**：`OpenCvLibraryMatcher`（`ILibraryMatcher`）：每個候選類型用參考影像整張建範本（校正值不同時先縮放成板子的比例），在區域裡找；所有類型的匹配依分數合併成元件（交集 ÷ 較小的框 > 0.5 是同一個元件），每個元件附每個類型的分數（由高到低）。唯一的預篩選是「參考影像比區域大就不比」；ADR-006 §7.2 的「依外形預篩選」沒有做。成本隨類型數 N 線性成長（約 9 ms／類型，T7）。**UI 沒有接**（使用者決定：程式裡還沒有帶參考影像的類型來源，等樣本庫 UI 那一步）。

<!--STE-->
## Real images
The user gave two real 12-bit images of a board with many pins.
The search finds the pins. Some edges of the pads also get scores from 0.70 to 0.85.
In one image, the lattice predicts parts in two empty slots.
These are open questions for the user.
<!--/STE-->

**繁中**：使用者提供的兩張去識別化影像（24.4 MP、12 bit、高角度光 HA 與低角度光 LA；**不入版控**，`perf_opencv_real` 用環境變數 `AOI_REAL_IMAGE_DIR` 讀）。以一根針腳當樣本：（1）HA 的種子分數分兩群：≥ 0.85 的幾乎都在針腳上，0.70–0.85 的多半是焊墊的邊緣（相似但不是）；use case 的種子門檻 0.70 與預設的分數門檻 0.70 會把它們當成找到的，晶格也因此變差（一維、信心 0.28）。（2）LA 的晶格是二維的，但在兩個空槽裡補了約 120 個假缺格：這是 S05 的已知失敗 #11（非矩形陣列），第一次在真實板子上確認。兩件事都在 use case／晶格（application、domain），S07 沒有改，列為**等使用者決定**（`docs/current-status.md`）。

## 已知的限制

- 強度相關法對光照變化、反光、縮放敏感：同一個零件在 HA 與 LA 下長得不一樣，換光源要重新教樣本；縮放沒有搜尋（流程 B 用校正值換算比例）。
- 相似但不同的東西可以有不低的分數（真實影像 0.70–0.85）：門檻要人確認。
- 範本有一部分在影像外的元件找不到（`matchTemplate` 只看完整的位置）。
- 角度範圍越大越慢；大的範本 ＋ ±180° 時，每層的角度數受 `maxAnglesPerLevel`（720）與記憶體（64 MB）限制。
- 搜尋的角度範圍要被範本的角度範圍涵蓋（否則 `InvalidArgument`，不默默只搜一部分）；use case 用同一組參數建範本與搜尋，所以不會碰到。
- 流程 B 不快取範本、只有尺寸的預篩選；小的類型可能在大元件的一部分上得到高分（強度相關法不看大小）。
- 角度的誤差約是第 0 層角度步進的 0.2 倍（小範本的步進大：48×28 是 2°，最大誤差 0.42°）；要更準就要更細的步進（更多範本、精修更慢）。
- `OpenCvSearchOptions` 的預設值是合成板子與兩張真實影像定的起點。

## 驗證

- `tst_opencv_backend`（Qt Test 的 Totals 23 項，含 ±5°、±30° 的角度掃描）、`tst_opencv_end_to_end`（12 項：8 種合成板子、有效位元數的兩個情況）、`tst_opencv_library_matcher`（8 項）；Totals 都含 initTestCase／cleanupTestCase 與資料列；`tst_image_view`、`tst_document`、`tst_params`、`tst_presenter`、`tst_recipe_store` 加了有效位元數與 schema v2 的測試；`tst_backend_registry`、`tst_cli` 依建置選項檢查 opencv 是否可用。
- 四種組態（Release、Release＋OpenCV、Debug、Debug＋OpenCV）的 CTest、`check-deps`、`ste-lint`、效能 3 次、變異測試：結果在 `perf.md` 與 `docs/changelog-ai.md`。
