//! 配置失敗不結束行程（ADR-031 第 4 點）：大小和輸入有關的配置一律用 `try_reserve`，失敗時往上回報
//! `Fail::NoMemory`（FFI 回 `AOI_RS_NO_MEMORY`，C++ 的包裝丟 `std::bad_alloc`）。
//! 自訂的分群策略（C++ 的回呼）失敗時是 `Fail::Callback`：C++ 的包裝重新丟出回呼記下的例外。

use std::collections::TryReserveError;

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Fail {
    /// 配置失敗。
    NoMemory,
    /// 回呼（C++ 的分群策略）回報失敗（狀態碼）。
    Callback(i32),
}

impl From<TryReserveError> for Fail {
    fn from(_: TryReserveError) -> Self {
        Fail::NoMemory
    }
}

pub type Res<T> = Result<T, Fail>;

/// 容量至少 `n` 的空陣列。
pub fn vec_with_capacity<T>(n: usize) -> Res<Vec<T>> {
    let mut v = Vec::new();
    v.try_reserve_exact(n)?;
    Ok(v)
}

/// `n` 個 `value` 的陣列（`std::vector<T>(n, value)`）。
pub fn vec_filled<T: Clone>(n: usize, value: T) -> Res<Vec<T>> {
    let mut v = Vec::new();
    v.try_reserve_exact(n)?;
    v.resize(n, value);
    Ok(v)
}

/// 從切片複製（`std::vector<T>(begin, end)`）。
pub fn vec_from_slice<T: Clone>(s: &[T]) -> Res<Vec<T>> {
    let mut v = Vec::new();
    v.try_reserve_exact(s.len())?;
    v.extend_from_slice(s);
    Ok(v)
}

/// `push_back`，配置失敗時回報錯誤（`try_reserve` 的容量照常加倍，攤銷 O(1)）。
pub trait TryPush<T> {
    fn try_push(&mut self, value: T) -> Res<()>;
    fn try_insert(&mut self, index: usize, value: T) -> Res<()>;
}

impl<T> TryPush<T> for Vec<T> {
    fn try_push(&mut self, value: T) -> Res<()> {
        self.try_reserve(1)?;
        self.push(value);
        Ok(())
    }

    fn try_insert(&mut self, index: usize, value: T) -> Res<()> {
        self.try_reserve(1)?;
        self.insert(index, value);
        Ok(())
    }
}
