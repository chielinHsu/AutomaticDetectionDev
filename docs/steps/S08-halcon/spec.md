# S08 — HALCON back end：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 設計來源：[`docs/plan/S08-halcon.md`](../../plan/S08-halcon.md)（含 S01、S03、S06、S07 的提醒）、port 的契約（`ITemplateBuilder.h`、`IPatternSearcher.h`、`ILibraryMatcher.h`）、ADR-010、ADR-014、ADR-016；HALCON 的運算子說明（`%HALCONROOT%\help\operators_en_US.ref` 與執行時的 `get_operator_info`）。
> 本檔講「為什麼這樣做」；決策在 [ADR-017](../../adr/017-halcon-shape-model-backend.md)（與 ADR-010、ADR-014 的「後續（S08）」）；圖在 [`diagram.md`](diagram.md)；實測數字在 [`perf.md`](perf.md)。

<!--STE-->
## Purpose
This step builds the HALCON back end. It uses the HALCON shape model to find similar parts.
It also checks the HALCON license. It tells the user when the license is not correct.
<!--/STE-->

**繁中**：做第二個 back end：HALCON 的 shape model（以邊緣的方向比對）。範本建立器、搜尋器（流程 A）與樣本庫比對器（流程 B 的 adapter，UI 仍不接）。另外照使用者的要求：檢查授權，分辨 runtime（RTL）與開發（SDK）兩種 dongle，授權不對時提醒。

<!--STE-->
## Reason
HALCON is an industry tool for this job. The shape model handles rotation and gives sub-pixel poses.
We must prove that it is better. We do not assume it.
The license has two dongles. HALCON runs with either dongle, and it gives no warning.
<!--/STE-->

**繁中**：「HALCON 比較準、比較快」是假設，要用和 S07 同一組情境的數字對照（S09 的 A/B 才下結論）。授權：這台機器有兩個授權檔，RTL 的 dongle 插著、SDK 的沒插；HALCON 會挑找得到 dongle 的那一個，照樣能跑、不報錯——所以「插錯 dongle」要程式自己看出來（使用者 2026-10-07 的要求）。

<!--STE-->
## Effect
The user can switch between OpenCV and HALCON with the back-end option.
A computer without HALCON still starts the program. The HALCON item shows the reason.
We have numbers that compare the two back ends on the same boards.
<!--/STE-->

**繁中**：`release-halcon` 的程式同時有 OpenCV 與 HALCON，用 `--backend` 切換（執行中切換還沒做）。沒有 HALCON 的電腦照樣啟動（`halconcpp.dll` 延遲載入），halcon 在選單上停用並說原因；`--backend=halcon` 在沒有授權時以結束碼 2 明確失敗。合成板子：召回率 100%；T2b（5 MP 種子搜尋）比同一次量的 OpenCV 快 2–3 倍，3 次中 2 次在預算內（機器整體變慢的那一次 p95 超過）；位置誤差 p95 在預算內但比 OpenCV 大約 4 倍（強雜訊時差最多，反光時 HALCON 比較好）。數字在 `perf.md`。

<!--STE-->
## The development files
The HALCON install on this computer has no headers and no libraries.
The user told us to copy them from another project.
The program loads the HALCON library only when it needs it.
<!--/STE-->

**繁中**：`%HALCONROOT%` 只有 runtime（DLL 與授權）。使用者指示從 CV_Algorithm 專案複製開發檔（那個專案也把它放在版控）：`third_party/halcon/`（標頭全部＋`halconcpp.lib`，版本 24.11.2，和 runtime 相同）。`cmake/FindHalconLocal.cmake` 找它；`halconcpp.dll` 延遲載入（`/DELAYLOAD`），第一次呼叫 HALCON 之前，`HalconRuntime` 在結構化例外的保護下載入 DLL、比對版本、綁定所有函式——沒有 HALCON 的電腦不會當掉。

<!--STE-->
## The license check
The first HALCON call starts the library and checks the license. This takes about 1.6 seconds.
So the program checks the license only when the user selects HALCON.
It reads the license files and the IDs of this computer.
It finds a development license file without its dongle. Then it shows a note.
<!--/STE-->

**繁中**：列 back end 時只看 PATH 上有沒有 `halconcpp.dll`（微秒級）；授權等建立 HALCON back end 時才查（第一次約 1.6 秒，之後回快取）。判斷是純函式 `judgeHalconLicense`：沒有程式庫、沒有有效授權、授權不含 shape matching → `BackendUnavailable`，原因寫出找到的授權檔與這台電腦的 ID；能用，但用的是 runtime 授權、而開發授權檔的 dongle 不在 → **提醒**：stderr 一行、訊息列的吐司、back end 選單的提示（文案在 `copy.json`：`ui.backend.halcon.sdkDongleMissing`）。只有 runtime 授權的電腦（部署用）不提醒。這台機器實測就是提醒的情況。沒有有效授權時（S08 驗證途中實測：HALCON 突然讀不到插著的 RTL dongle，錯誤 2300，重插後恢復），HALCON 查不到授權檔，程式改讀 `%HALCONROOT%\license\*.dat`，原因寫出每個授權檔需要哪個 dongle。

<!--STE-->
## How the shape model works
The shape model uses the edges of the sample, not the gray values.
At each point of an edge, it keeps the direction of the edge.
The score tells how well the edge directions agree. It does not change with brightness.
HALCON makes the image smaller first, like the OpenCV search. Then it refines each match.
<!--/STE-->

**繁中**：`create_shape_model` 從樣本的邊緣取點（每點記梯度方向）；`find_shape_model` 比的是「方向一致的程度」，所以亮度的倍數與偏移（甚至局部的對比變化，看 metric）不影響分數——這和 OpenCV 的 NCC（比灰階）不是同一把尺：同一個 `minScore` 在兩個 back end 的意義不同。也是由粗到細：金字塔頂層找候選、往下精修，最後最小平方調整到次像素。樣本整塊是平的時建不出模型（錯誤 8510），回傳 `LowContrast`（和 OpenCV 版一樣）。

<!--STE-->
## Angle steps
HALCON can choose the angle step. For small samples, its step is too large.
Then some parts get an angle at the end of the range.
So the program calculates the step. One step moves a corner of the template half a pixel.
The model also covers two more steps at each end of the range.
<!--/STE-->

**繁中**：48×28 的樣本，HALCON 'auto' 給 5.2°；說明寫範圍不是步進的整數倍時會調整步進，所以 ±5° 只剩 −5、0、5 三個角度，傾斜 3° 的元件有一半回報 5°。改成「角落每步移動 0.5 像素」（48×28 是 1.0°），並讓模型的角度範圍兩端各多 2 步（範圍剛好等於要求時，接近兩端的元件精修會被卡住）。搜尋仍只要求使用者的範圍，結果的角度夾回範圍內。實測見 `perf.md` 的 AS-*。

<!--STE-->
## Positions and angles
HALCON gives the row, the column and the angle in radians.
The program changes them to the convention of the project.
For a rotated match, HALCON moves the position a little. The program removes this change.
Tests with rotated boards keep these rules.
<!--/STE-->

**繁中**：ADR-010 的 HALCON 部分 S08 查了說明並實測：Row 向下、像素中心在整數、角度是弧度；角度的正方向和本專案相同（只換單位）。位置：說明寫 Row、Column 是配 `affine_trans_pixel` 調過的值，參考點真正的位置是 `x ＝ Column ＋ 0.5(sin ＋ cos)`、`y ＝ Row ＋ 0.5(cos − sin)`（角度 0 時就是 ＋0.5；30° 時差 0.37 像素）。測試：傾斜 ±3°、+4.5°、−5°..+5° 掃描、±30°（修正後平均誤差 0.018 像素，沒修正 0.303 像素）。

<!--STE-->
## Images
For an 8-bit image, HALCON reads the pixels of the program directly. There is no copy.
A 16-bit image gets a scale to 8 bits from its bit depth. This makes one copy.
The OpenCV search uses the same 8-bit values. So the comparison is fair.
<!--/STE-->

**繁中**：`gen_image1_extern` 要一列接一列的像素：`Gray8` 緊密排列時零複製，子影像要複製。`Gray16` 預設依有效位元數（`effectiveBits`，使用者可以覆寫）換成 8 bit（查表，5 MP 約 8–11 ms；第一版逐像素算要 213 ms，量到之後改的）；選項 `nativeGray16` 直接用 HALCON 的 `uint2`（零複製，實測一樣準、更快），留給 S09 決定預設。彩色轉灰階用和 OpenCV `cvtColor` 同一組係數。

<!--STE-->
## Windows and cancel
For the window list, the program searches once. The search area is all windows together.
A search can take a long time. HALCON can stop a running search.
A small thread watches the cancel flag. It stops the search when the user cancels.
<!--/STE-->

**繁中**：視窗清單是一次 `find_shape_model`：影像的區域（domain）＝ 所有視窗（各往外 2^(層數−1) 像素，說明建議）的聯集，結果再用視窗本身過濾（契約：中心在視窗內）。取消：`find_shape_model(s)` 支援 `interrupt_operator(…, 'cancel')`（`get_operator_info` 實查），`HalconCancelWatch` 每毫秒看一次旗標；`create_shape_model` 不能中斷，只在前後檢查。實測取消延遲見 `perf.md` 的 C-search。

<!--STE-->
## Threads
HALCON runs its operators on its own threads. The program sets the number of these threads.
It uses the same number as OpenCV and the job runner: the cores minus one.
<!--/STE-->

**繁中**：建立 back end 時 `set_system('thread_num', 核心數 − 1)`（`app::workerThreads()`，和 ADR-016 同一個理由）。`create_shape_model`、`find_shape_model(s)` 的 parallelization 都是 'no_restriction'；同一個模型可以被幾個 worker 同時用（測試驗證）。不預先產生模型（說明：預先產生的模型在不同大小的影像上平行搜尋可能當掉）。

<!--STE-->
## The sample library matcher
The region scan makes one shape model for each part type.
HALCON can search many models in one call. For many types, this call is faster than one call for each type.
The program puts matches at the same place into one part.
<!--/STE-->

**繁中**：`HalconLibraryMatcher`：每個候選類型一個模型，一次 `find_shape_models`；`MaxOverlap` 每個模型一個值（說明：這樣只有同一個模型的結果互相抑制，等於分開呼叫）。實測兩種做法給出一樣的元件；N ≥ 10 時一次呼叫快 1.1–2.0 倍（N ＝ 1 時反而慢一點），比 OpenCV 快 2.4–3.6 倍（N ＝ 1000）——ADR-006「每個條目一個 shape model，一次找多個」的預期成立。合併成元件的規則和 OpenCV 版相同（抄一份，S09 的契約測試鎖住）。UI 仍不接（使用者 S07 的決定）。

## 已知的限制

- **強雜訊時角度與位置比 OpenCV 不穩**：shape model 只看邊緣點。合成板子（雜訊 12 灰階）上最大角度誤差約 0.9°（48×28，角落位移 0.44 像素），雜訊 12＋模糊 2 的板子位置誤差最大 1.4 像素；同情況 OpenCV 好得多（`perf.md`）。
- **0° 附近的角度**：傾斜 ±0.5° 的元件有一部分回報接近 0°（最大誤差 0.52°），每種步進設定都一樣，是 HALCON 本身的行為。
- 範本有一部分在影像外的元件找不到（說明：模型要整個在影像內；`border_shape_models` 沒有開）。
- 建模型比 OpenCV 慢很多（T1 中位數：7–19 ms 對 0.2–0.5 ms），而且不能中斷。
- HALCON 的時間在幾次執行之間差得比 OpenCV 多（T2b 中位數 22–44 ms，同一次的 OpenCV 71–73 ms）；機器變慢的那一次 T2b 的 p95 超過預算（`perf.md`）。
- 真實影像 HA（高角度光）上，用一根針腳當樣本，HALCON 只找到少數針腳（分數 < 0.70）；OpenCV 找到大部分（也框到焊墊邊緣）。原因是推測（樣本裡周圍焊墊的輪廓），S09 比較。
- 25 MP 的 `Gray16` 一次搜尋額外用約 200 MB（OpenCV 約 70 MB）。
- 授權第一次檢查約 1.6 秒（在建立 back end 時，不在啟動時）；之後每次重新探測約 0.35 秒（`hostids` 要列舉 dongle），所以只探測一次。
- HALCON 的影像來源（`read_image`）沒有做：`release-halcon` 有 OpenCV 讀 TIFF；只有 HALCON 沒有 OpenCV 的建置讀不了 TIFF。是否要做，等使用者決定。
- `create_ncc_model` 的對照沒有做（plan 寫「可選」）。
- `HalconSearchOptions` 的預設值是合成板子與兩張真實影像定的起點。

## 驗證

- `tst_halcon_backend`（授權檔解析與判斷、位姿換算的公式、運算子可中斷、範本、角度正負號、角度掃描、±30° 的修正、三種格式一致、子影像、視窗契約、視窗與整張一致、取消、可重入、複製次數、有效位元數、色彩係數、工廠）、`tst_halcon_end_to_end`（8 種合成板子，和 OpenCV 版同一組）、`tst_halcon_library_matcher`（和 OpenCV 版同一組，加上兩種做法一致）；`tst_cli`（有授權時啟動並提醒、沒有授權時結束碼 2、沒有 DLL 時其他 back end 照常）、`tst_backend_registry`、`tst_shell`（選單停用與提示）。
- 組態、`check-deps`、`ste-lint`、效能的次數與結果：`perf.md` 與 `docs/changelog-ai.md`。
