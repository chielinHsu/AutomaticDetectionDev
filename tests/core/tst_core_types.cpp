// 錯誤模型（Result、Error）與取消（CancelSource、CancelToken）。
#include <QtTest>
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <variant>

#include "application/CancelToken.h"
#include "application/Error.h"
#include "application/Result.h"

using namespace aoi;

class TstCoreTypes : public QObject {
    Q_OBJECT

private slots:
    void aSuccessCarriesItsValue() {
        const Result<int> r = 42;
        QVERIFY(r.ok());
        QVERIFY(static_cast<bool>(r));
        QCOMPARE(r.value(), 42);
    }

    void aFailureCarriesItsError() {
        const Result<int> r = makeError(ErrorCode::NotFound, "missing");
        QVERIFY(!r.ok());
        QVERIFY(!static_cast<bool>(r));
        QVERIFY(r.error().code == ErrorCode::NotFound);
        QCOMPARE(r.error().message, std::string("missing"));
    }

    // 這是文件裡寫明的少數例外：在失敗的 Result 取 value() 是呼叫端的程式錯誤，不是 port 的執行期失敗。
    void readingTheWrongAlternativeIsAProgrammerError() {
        const Result<int> failed = makeError(ErrorCode::Internal, "x");
        QVERIFY_THROWS_EXCEPTION(std::bad_variant_access, failed.value());
        const Result<int> fine = 1;
        QVERIFY_THROWS_EXCEPTION(std::bad_variant_access, fine.error());
    }

    void voidResultHasSuccessAndFailure() {
        const Result<void> ok = Result<void>::success();
        QVERIFY(ok.ok());
        const Result<void> bad = makeError(ErrorCode::IoFailed, "disk");
        QVERIFY(!bad.ok());
        QVERIFY(bad.error().code == ErrorCode::IoFailed);
        const Result<void> implicitOk;
        QVERIFY(implicitOk.ok());
    }

    void resultCanHoldMoveOnlyValues() {
        Result<std::unique_ptr<int>> r = std::make_unique<int>(7);
        QVERIFY(r.ok());
        std::unique_ptr<int> taken = std::move(r).value();
        QVERIFY(taken != nullptr);
        QCOMPARE(*taken, 7);
    }

    void everyErrorCodeHasAName() {
        for (ErrorCode code : {ErrorCode::Cancelled, ErrorCode::InvalidArgument, ErrorCode::BackendUnavailable,
                               ErrorCode::BackendMismatch, ErrorCode::BackendFailed, ErrorCode::IoFailed,
                               ErrorCode::FormatInvalid, ErrorCode::NotFound, ErrorCode::Internal}) {
            QVERIFY(toString(code) != "Unknown");
        }
    }

    void aDefaultTokenIsNeverCancelled() {
        const CancelToken token;
        QVERIFY(!token.cancelled());
    }

    void cancellingASourceRaisesEveryTokenCopy() {
        CancelSource source;
        const CancelToken a = source.token();
        const CancelToken b = a;  // 複本看同一個旗標
        QVERIFY(!a.cancelled() && !b.cancelled() && !source.cancelled());
        source.cancel();
        QVERIFY(a.cancelled() && b.cancelled() && source.cancelled());
        source.cancel();  // 冪等
        QVERIFY(a.cancelled());
    }

    void sourcesAreIndependent() {
        CancelSource first;
        CancelSource second;
        const CancelToken t1 = first.token();
        const CancelToken t2 = second.token();
        first.cancel();
        QVERIFY(t1.cancelled());
        QVERIFY(!t2.cancelled());
    }

    // 從別的執行緒取消，等待中的執行緒要看得到（原子旗標的可見性）。
    void cancellationIsVisibleAcrossThreads() {
        CancelSource source;
        const CancelToken token = source.token();
        std::atomic<bool> started{false};
        std::atomic<bool> sawCancel{false};
        std::thread worker([&] {
            started = true;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (!token.cancelled() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
            sawCancel = token.cancelled();
        });
        while (!started) std::this_thread::yield();
        source.cancel();
        worker.join();
        QVERIFY(sawCancel);
    }
};

QTEST_APPLESS_MAIN(TstCoreTypes)
#include "tst_core_types.moc"
