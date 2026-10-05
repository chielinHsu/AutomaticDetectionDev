#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vector>
#endif

// 效能量測的衛生：讓結果不隨 Windows 把執行緒排到哪一種核心而跳動。
//
// 為什麼：本機是 Intel 混合架構（i7-1360P：4 個 P 核、8 個 E 核）。實測同一份程式、同一台機器，
// 背景啟動的量測行程在不同次執行會差約 2 倍（PNG 解碼 125 ms 與 265 ms 兩種結果，縮放 0.5 的框層 9.5 與 21.7 ms），
// 推測是被排到 E 核或被電源節流（沒有證據證明是哪一個，所以三件事都做）。
// 使用者實際操作時視窗在前景，Windows 會把它排到 P 核；量測也應該在 P 核上。
//
// 做三件事：行程優先權設為 HIGH；關閉電源節流（EcoQoS）；把呼叫的執行緒釘在效率等級最高的核心上。
// 回傳做了哪些事，要寫進機器指紋。
namespace aoi::test {

inline QString applyMeasurementHygiene() {
#ifdef Q_OS_WIN
    QStringList notes;
    if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) notes << QStringLiteral("process priority HIGH");

    PROCESS_POWER_THROTTLING_STATE throttling{};
    throttling.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    throttling.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    throttling.StateMask = 0;  // 0 ＝ 明確關閉節流
    if (SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &throttling, sizeof(throttling)))
        notes << QStringLiteral("power throttling off");

    ULONG length = 0;
    GetSystemCpuSetInformation(nullptr, 0, &length, GetCurrentProcess(), 0);
    if (length > 0) {
        std::vector<char> buffer(length);
        auto* first = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(buffer.data());
        if (GetSystemCpuSetInformation(first, length, &length, GetCurrentProcess(), 0)) {
            BYTE bestClass = 0;
            int bestCpu = -1;
            int distinctClasses = 0;
            BYTE seenClasses[8] = {};
            for (ULONG offset = 0; offset < length;) {
                auto* info = reinterpret_cast<SYSTEM_CPU_SET_INFORMATION*>(buffer.data() + offset);
                if (info->Type == CpuSetInformation && info->CpuSet.Group == 0) {
                    const BYTE cls = info->CpuSet.EfficiencyClass;
                    bool seen = false;
                    for (int i = 0; i < distinctClasses; ++i) seen = seen || seenClasses[i] == cls;
                    if (!seen && distinctClasses < 8) seenClasses[distinctClasses++] = cls;
                    if (bestCpu < 0 || cls > bestClass) {
                        bestClass = cls;
                        bestCpu = info->CpuSet.LogicalProcessorIndex;
                    }
                }
                offset += info->Size;
            }
            // 環境變數 AOI_PERF_CPU 可以指定要釘在哪個邏輯 CPU（量測對核心的敏感度用）；沒設就用效率等級最高的第一個。
            const QByteArray forced = qgetenv("AOI_PERF_CPU");
            if (!forced.isEmpty()) bestCpu = forced.toInt();
            if (bestCpu >= 0 && bestCpu < 64 &&
                SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(1) << bestCpu) != 0) {
                notes << QStringLiteral("thread pinned to logical CPU %1 (efficiency class %2 of %3 classes)")
                             .arg(bestCpu).arg(bestClass).arg(distinctClasses);
            }
        }
    }
    return notes.join(QStringLiteral("; "));
#else
    return QStringLiteral("not applied (not Windows)");
#endif
}

}  // namespace aoi::test
