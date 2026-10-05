# ADR-003 測試框架：統一用 Qt Test（涵蓋 domain 到 UI）

- 日期：2026-10-05
- 決定者：Claude Code (Claude Sonnet 5.5) 依 `docs/plan/S03-core.md` 第 9 項（預定的決策，計畫經使用者於 S00 核准）提出並實作；S02 已先用於 UI 測試，S03 擴及核心。使用者沒有針對這份 ADR 另外表示意見。
- 狀態：採用

## 背景

專案要測三種東西：純 C++ 的 domain 與 application（不 include Qt）、infrastructure 的 adapter、以及 Qt UI（要注入滑鼠、滾輪、鍵盤事件，要在 offscreen 平台跑）。`docs/perf-test-plan.md` 在 S00 就寫了「一個測試框架即可涵蓋 domain 到 UI」。

限制：

- `domain` 與 `application` 不能 include Qt（`docs/architecture.md`），但**測試檔案**不在五層之內（`tools/check-deps.mjs` 只掃 `src/`），所以測試可以用 Qt Test。
- 要經 CTest 執行，才能和 `check-deps` 一起當成「建置通過」的條件。
- Windows 上的 MSVC 2022，Qt 6.8.0。

## 決定

**所有測試用 Qt Test，一個測試類別一個執行檔，經 CTest 執行**（`cmake/AoiTargets.cmake` 的 `aoi_add_qt_test`）。

- 核心測試（`tests/core/`）用 `QTEST_APPLESS_MAIN`（不需要 `QApplication`）。被測的 target（`aoi_domain`、`aoi_application`、`aoi_backend_fake`、`aoi_perf`、`aoi_threading`）本身**不連 Qt**；只有測試執行檔連 `Qt6::Test`。
- UI 測試（`tests/ui/`）用 `QTEST_MAIN` 與 offscreen 平台。
- 參數化用 `_data()` 函式；沒有 mock 框架，需要「記錄呼叫」的地方手寫 spy 類別（例如 `CountingSearcher`）。
- 效能量測是獨立的執行檔（label `perf`），預設的 test preset 排除它們。它們自己用 `std::chrono::steady_clock` 量並算中位數與 p95、輸出 JSON；**沒有用 `QBENCHMARK`**，因為需要自己控制熱機次數、輸出機器指紋與 p95。

## 證據

事實（已觀察）：

- S02：5 個 UI 測試執行檔在 Release 與 Debug 都通過。
- S03：8 個核心測試執行檔加 `tst_cli`（共 155 項，含每個檔案的 `initTestCase`／`cleanupTestCase`）與 3 個 `check_deps*` 項目，Release 與 Debug 都通過；`aoi_application` 與 `aoi_domain` 的 CMake 依賴裡沒有 Qt（`src/application/CMakeLists.txt`）。
- 這台機器上 Qt Test **不把結果寫到 stdout**（重新導向後是空的），所以 `aoi_add_qt_test` 用 `-o <檔案>,txt`，`scripts/dev/test.ps1` 在失敗時印出 log（S02 發現）。
- Qt Test 的斷言巨集（`QVERIFY`、`QCOMPARE`）是為測試執行緒設計的：失敗時它們 `return` 並回報，在 worker 執行緒的 callback 裡呼叫沒有任何保證（Qt 文件沒有承諾它們可以跨執行緒用；這裡沒有實測會發生什麼）。S03 的跨執行緒測試因此一律「callback 只記錄到原子變數、測試執行緒再斷言」（`tst_backend_registry.cpp`、`tst_job_runner.cpp`）。

假設（未驗證）：

- `QBENCHMARK` 在這台機器的輸出行為沒有驗證過（本專案沒有用到它，所以沒有驗證）。計畫原本寫「也有 `QBENCHMARK`」，這一點只是「Qt Test 有這個功能」，不是「本專案用了它」。

## 否決的替代方案

- **GoogleTest／Catch2 做核心測試，Qt Test 做 UI 測試**：兩套框架、兩套 CTest 設定與輸出格式。核心測試不需要 Qt Test 以外的功能。多一個依賴要有理由，目前沒有。
- **只用 CTest 加自己寫的斷言**：UI 測試需要事件注入與事件迴圈輔助（`QTest::qWaitFor`、`QSignalSpy`），自己重寫成本高。

## 後果

- `tests/` 可以同時連各層，不受依賴規則限制；`check-deps` 不掃 `tests/`（也不掃 `AiInspTool/`）。
- 沒有 mock 框架：新的 port 要附一個「記錄呼叫」的 spy，放在 `tests/core/CoreTestSupport.h`。
- 跨執行緒的斷言要在測試執行緒做（callback 只記錄、不斷言）。
- 之後若需要 death test 或自動 mock，要新增一份 ADR 取代這一份。
