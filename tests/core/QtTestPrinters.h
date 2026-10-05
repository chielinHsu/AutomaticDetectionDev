#pragma once

#include <QString>
#include <QtTest>

#include "application/interaction/InteractionStateMachine.h"

// QCOMPARE 要把值印成文字，而 Qt Test 用 ADL 找 toString()：aoi::toString(InteractionState) 回 std::string_view，
// 和 Qt Test 預期的 const char* 不合，編譯不過（ErrorCode 也一樣，所以其他測試對它用 QVERIFY(a == b)）。
// 狀態的比較常用，所以做一個會印出狀態名稱的巨集：失敗訊息是「expected Idle, actual Preview」，不是 1 與 4。
namespace aoi::test {

inline QString stateName(InteractionState s) {
    const std::string_view name = toString(s);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

}  // namespace aoi::test

#define QCOMPARE_STATE(actual, expected)                                                                         \
    do {                                                                                                         \
        const aoi::InteractionState actual_ = (actual);                                                          \
        const aoi::InteractionState expected_ = (expected);                                                      \
        if (actual_ != expected_) {                                                                              \
            QFAIL(qPrintable(QStringLiteral("state: expected %1, actual %2")                                     \
                                 .arg(aoi::test::stateName(expected_), aoi::test::stateName(actual_))));         \
        }                                                                                                        \
    } while (false)
