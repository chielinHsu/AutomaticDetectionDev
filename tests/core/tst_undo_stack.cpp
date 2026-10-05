// UndoStack（ADR-002）：執行、復原、重做、合併、深度上限、乾淨位置。
#include <QtTest>
#include <memory>
#include <string>
#include <vector>

#include "application/document/UndoStack.h"

using namespace aoi;

namespace {

// 目標：一個整數加一份記錄。Command 把數字加上 amount。
struct Counter {
    int value = 0;
    std::vector<std::string> log;
};

class AddCommand final : public Command<Counter> {
public:
    explicit AddCommand(int amount, bool mergeable = false, std::shared_ptr<int> payload = nullptr)
        : amount_(amount), mergeable_(mergeable), payload_(std::move(payload)) {}
    void redo(Counter& c) const override {
        c.value += amount_;
        c.log.push_back("redo " + std::to_string(amount_));
    }
    void undo(Counter& c) const override {
        c.value -= amount_;
        c.log.push_back("undo " + std::to_string(amount_));
    }
    CommandKind kind() const override { return CommandKind::MoveComponents; }
    std::size_t count() const override { return 1; }
    std::shared_ptr<const Command<Counter>> mergeWith(const Command<Counter>& next) const override {
        const auto* other = dynamic_cast<const AddCommand*>(&next);
        if (!mergeable_ || other == nullptr || !other->mergeable_) return nullptr;
        return std::make_shared<AddCommand>(amount_ + other->amount_, true);
    }
    int amount() const { return amount_; }

private:
    int amount_;
    bool mergeable_;
    std::shared_ptr<int> payload_;  // 用來看 Command 什麼時候被釋放
};

using Stack = UndoStack<Counter>;
std::shared_ptr<const Command<Counter>> add(int n, bool mergeable = false, std::shared_ptr<int> payload = nullptr) {
    return std::make_shared<AddCommand>(n, mergeable, std::move(payload));
}

}  // namespace

class TstUndoStack : public QObject {
    Q_OBJECT

private slots:
    void pushRunsTheCommandOnceAndUndoRedoRoundTrip() {
        Counter c;
        Stack s;
        s.push(c, add(5));
        QCOMPARE(c.value, 5);
        QCOMPARE(c.log, (std::vector<std::string>{"redo 5"}));  // push 只執行一次
        QVERIFY(s.canUndo());
        QVERIFY(!s.canRedo());

        QVERIFY(s.undo(c));
        QCOMPARE(c.value, 0);
        QVERIFY(!s.canUndo());
        QVERIFY(s.canRedo());
        QVERIFY(s.redo(c));
        QCOMPARE(c.value, 5);
        QVERIFY(!s.redo(c));  // 沒有可重做的：回 false，不動
        QCOMPARE(c.value, 5);
    }

    void undoOnAnEmptyStackDoesNothing() {
        Counter c;
        Stack s;
        QVERIFY(!s.undo(c));
        QVERIFY(!s.redo(c));
        QVERIFY(c.log.empty());
        QVERIFY(s.nextUndo() == nullptr);
        QVERIFY(s.nextRedo() == nullptr);
    }

    void aNewPushDiscardsTheRedoTail() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        s.push(c, add(10));
        s.undo(c);
        QVERIFY(s.canRedo());
        s.push(c, add(100));
        QVERIFY(!s.canRedo());
        QCOMPARE(c.value, 101);
        QCOMPARE(s.depth(), std::size_t{2});
    }

    void nextUndoAndNextRedoPointAtTheRightCommands() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        s.push(c, add(2));
        QCOMPARE(static_cast<const AddCommand*>(s.nextUndo())->amount(), 2);
        s.undo(c);
        QCOMPARE(static_cast<const AddCommand*>(s.nextUndo())->amount(), 1);
        QCOMPARE(static_cast<const AddCommand*>(s.nextRedo())->amount(), 2);
    }

    void mergeFoldsConsecutiveCommandsIntoOneStepWhenAllowed() {
        Counter c;
        Stack s;
        s.push(c, add(1, true), true);
        s.push(c, add(1, true), true);
        s.push(c, add(1, true), true);
        QCOMPARE(c.value, 3);
        QCOMPARE(s.depth(), std::size_t{1});  // 三次微調，一步
        s.undo(c);
        QCOMPARE(c.value, 0);                 // 一次復原全部回去
        s.redo(c);
        QCOMPARE(c.value, 3);
    }

    void mergeIsNotTriedUnlessTheCallerAllowsIt() {
        Counter c;
        Stack s;
        s.push(c, add(1, true));
        s.push(c, add(1, true));  // allowMerge 預設 false：一次拖曳放開後的下一次拖曳是另一步
        QCOMPARE(s.depth(), std::size_t{2});
    }

    void mergeIsRefusedByTheCommandAndThenItIsASeparateStep() {
        Counter c;
        Stack s;
        s.push(c, add(1, true), true);
        s.push(c, add(2, false), true);  // 新的不是可合併的類型
        QCOMPARE(s.depth(), std::size_t{2});
        QCOMPARE(c.value, 3);
    }

    void mergeNeverMutatesTheCommandAlreadyOnTheStack() {
        // Command 不可變：舊的 Command 被其他堆疊的複本共用時（提案堆疊隨接受／捨棄一起被帶走），合併不能改到它。
        Counter c;
        Stack s;
        auto first = add(1, true);
        s.push(c, first, true);
        Stack copy = s;                    // 複本共用同一個 Command
        s.push(c, add(5, true), true);     // 在原本的堆疊上合併
        QCOMPARE(static_cast<const AddCommand*>(first.get())->amount(), 1);

        Counter other;
        other.value = 1;
        copy.undo(other);                  // 複本的第一步還是 +1
        QCOMPARE(other.value, 0);
    }

    void aCopyOfTheStackIsIndependent() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        Stack copy = s;
        s.push(c, add(2));
        QCOMPARE(s.depth(), std::size_t{2});
        QCOMPARE(copy.depth(), std::size_t{1});
        QVERIFY(!copy.canRedo());
    }

    void depthIsCappedAndTheOldestStepsAreReleased() {
        Counter c;
        Stack s(3);
        std::vector<std::weak_ptr<int>> payloads;
        for (int i = 1; i <= 5; ++i) {
            auto p = std::make_shared<int>(i);
            payloads.push_back(p);
            s.push(c, add(i, false, p));
        }
        QCOMPARE(s.depth(), std::size_t{3});
        QVERIFY(payloads[0].expired());   // 第 1、2 步被丟掉，連它們帶的資料一起釋放
        QVERIFY(payloads[1].expired());
        QVERIFY(!payloads[2].expired());
        QVERIFY(!payloads[4].expired());
        QCOMPARE(c.value, 15);
        s.undo(c);
        s.undo(c);
        s.undo(c);
        QVERIFY(!s.canUndo());
        QCOMPARE(c.value, 3);             // 前兩步（1+2）已經回不去了
    }

    void theDefaultDepthIs200() {
        QCOMPARE(Stack::kDefaultMaxDepth, std::size_t{200});
        Counter c;
        Stack s;
        for (int i = 0; i < 250; ++i) s.push(c, add(1));
        QCOMPARE(s.depth(), std::size_t{200});
    }

    void dropRedoKeepsTheUndoablePart() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        s.push(c, add(2));
        s.undo(c);
        QCOMPARE(s.redoDepth(), std::size_t{1});
        s.dropRedo();
        QVERIFY(!s.canRedo());
        QVERIFY(s.canUndo());
        QCOMPARE(s.depth(), std::size_t{1});
    }

    void clearEmptiesEverythingAndTheStackIsCleanAgain() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        QVERIFY(!s.isClean());
        s.clear();
        QVERIFY(!s.canUndo());
        QVERIFY(!s.canRedo());
        QVERIFY(s.isClean());
    }

    // ---- 乾淨位置（「有沒有未儲存的修改」）----
    void aNewStackIsCleanAndAnEditMakesItDirty() {
        Counter c;
        Stack s;
        QVERIFY(s.isClean());
        s.push(c, add(1));
        QVERIFY(!s.isClean());
    }

    void markCleanFollowsTheStackPosition() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        s.markClean();
        QVERIFY(s.isClean());
        s.push(c, add(2));
        QVERIFY(!s.isClean());
        s.undo(c);
        QVERIFY(s.isClean());          // 回到儲存時的位置就是乾淨的
        s.undo(c);
        QVERIFY(!s.isClean());
        s.redo(c);
        QVERIFY(s.isClean());
    }

    void pushingAwayTheCleanPositionMakesItUnreachable() {
        Counter c;
        Stack s;
        s.push(c, add(1));
        s.push(c, add(2));
        s.markClean();                 // 位置 2
        s.undo(c);
        s.push(c, add(3));             // 位置 2 的內容被換掉了
        QVERIFY(!s.isClean());
        s.undo(c);
        s.redo(c);
        QVERIFY(!s.isClean());         // 回到位置 2，但那已經不是儲存的內容
    }

    void mergingIntoTheSavedStepMakesItDirty() {
        Counter c;
        Stack s;
        s.push(c, add(1, true), true);
        s.markClean();
        s.push(c, add(1, true), true);  // 併進已儲存的那一步
        QVERIFY(!s.isClean());
        s.undo(c);
        QVERIFY(!s.isClean());          // 位置 0 也不是儲存的內容（儲存的是 +1，現在 undo 到 0）
    }

    void theSavedStateStaysReachableWhileItIsTheBottomOfTheStack() {
        Counter c;
        Stack s(2);
        s.push(c, add(1));
        s.markClean();                  // 儲存的內容：value ＝ 1
        s.push(c, add(2));
        s.push(c, add(3));              // 超過深度：丟掉最舊的 +1，它的效果成為堆疊底部的基底
        QVERIFY(!s.isClean());
        s.undo(c);
        s.undo(c);
        QCOMPARE(c.value, 1);
        QVERIFY(s.isClean());           // 堆疊底部就是儲存時的內容
    }

    void droppingMoreThanTheSavedStepMakesItUnreachable() {
        Counter c;
        Stack s(2);
        s.push(c, add(1));
        s.markClean();
        s.push(c, add(2));
        s.push(c, add(3));
        s.push(c, add(4));              // 再丟一步：儲存時的內容已經不在堆疊上任何位置
        s.undo(c);
        s.undo(c);
        QVERIFY(!s.isClean());
    }
};

QTEST_APPLESS_MAIN(TstUndoStack)
#include "tst_undo_stack.moc"
