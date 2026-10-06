#pragma once

#include <QString>
#include <QtTest>

#include <string_view>

#include "application/Error.h"
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

// ErrorCode 的比較（S06）：同樣的原因不能用 QCOMPARE；失敗訊息印出名稱（「expected NotFound, actual IoFailed」）與錯誤訊息。
#define QCOMPARE_CODE(actualCode, expectedCode)                                                                  \
    do {                                                                                                         \
        const aoi::ErrorCode actual_ = (actualCode);                                                             \
        const aoi::ErrorCode expected_ = (expectedCode);                                                         \
        if (actual_ != expected_) {                                                                              \
            const std::string_view a_ = aoi::toString(actual_);                                                  \
            const std::string_view e_ = aoi::toString(expected_);                                                \
            QFAIL(qPrintable(QStringLiteral("error code: expected %1, actual %2")                                \
                                 .arg(QString::fromUtf8(e_.data(), static_cast<qsizetype>(e_.size())),           \
                                      QString::fromUtf8(a_.data(), static_cast<qsizetype>(a_.size())))));        \
        }                                                                                                        \
    } while (false)
