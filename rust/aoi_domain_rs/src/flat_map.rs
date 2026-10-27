//! 鍵是 64 位元整數的雜湊表（開放定址、線性探測、容量是 2 的冪）：`src/domain/lattice/flat_map.h` 的對應。
//! 為什麼不用 `std::collections::HashMap`：預設的 SipHash 對這種小鍵太慢，而且這裡要能在配置失敗時回報錯誤
//! （`try_reserve`），不要直接結束行程。

use std::collections::TryReserveError;

/// 把兩個 32 位元的有號整數（格子座標）打包成一個鍵（`detail::packKey`）。
pub fn pack_key(a: i32, b: i32) -> u64 {
    ((a as u32 as u64) << 32) | (b as u32 as u64)
}

pub struct FlatMap<V: Copy + Default> {
    keys: Vec<u64>,
    values: Vec<V>,
    used: Vec<u8>,
    mask: usize,
    size: usize,
}

impl<V: Copy + Default> FlatMap<V> {
    pub fn new(expected: usize) -> Result<Self, TryReserveError> {
        let mut map = FlatMap {
            keys: Vec::new(),
            values: Vec::new(),
            used: Vec::new(),
            mask: 0,
            size: 0,
        };
        map.reset(expected)?;
        Ok(map)
    }

    fn reset(&mut self, expected: usize) -> Result<(), TryReserveError> {
        let mut capacity = 16usize;
        while capacity < expected.saturating_mul(2) {
            capacity *= 2;
        }
        let mut keys = Vec::new();
        keys.try_reserve_exact(capacity)?;
        keys.resize(capacity, 0);
        let mut values = Vec::new();
        values.try_reserve_exact(capacity)?;
        values.resize(capacity, V::default());
        let mut used = Vec::new();
        used.try_reserve_exact(capacity)?;
        used.resize(capacity, 0);
        self.keys = keys;
        self.values = values;
        self.used = used;
        self.mask = capacity - 1;
        self.size = 0;
        Ok(())
    }

    fn hash(mut k: u64) -> usize {
        k ^= k >> 33;
        k = k.wrapping_mul(0xFF51_AFD7_ED55_8CCD);
        k ^= k >> 33;
        k = k.wrapping_mul(0xC4CE_B9FE_1A85_EC53);
        k ^= k >> 33;
        k as usize
    }

    pub fn find(&self, key: u64) -> Option<&V> {
        let mut slot = Self::hash(key) & self.mask;
        while self.used[slot] != 0 {
            if self.keys[slot] == key {
                return Some(&self.values[slot]);
            }
            slot = (slot + 1) & self.mask;
        }
        None
    }

    /// 找不到就插入預設值（`operator[]`）。
    pub fn entry(&mut self, key: u64) -> Result<&mut V, TryReserveError> {
        if (self.size + 1) * 2 > self.keys.len() {
            self.grow()?;
        }
        let mut slot = Self::hash(key) & self.mask;
        while self.used[slot] != 0 {
            if self.keys[slot] == key {
                return Ok(&mut self.values[slot]);
            }
            slot = (slot + 1) & self.mask;
        }
        self.used[slot] = 1;
        self.keys[slot] = key;
        self.values[slot] = V::default();
        self.size += 1;
        Ok(&mut self.values[slot])
    }

    pub fn len(&self) -> usize {
        self.size
    }

    pub fn is_empty(&self) -> bool {
        self.size == 0
    }

    fn grow(&mut self) -> Result<(), TryReserveError> {
        let old_keys = std::mem::take(&mut self.keys);
        let old_values = std::mem::take(&mut self.values);
        let old_used = std::mem::take(&mut self.used);
        // 容量加倍（reset 取 ≥ 2 × expected 的 2 的冪）。
        if let Err(e) = self.reset(old_keys.len()) {
            self.keys = old_keys;
            self.values = old_values;
            self.used = old_used;
            return Err(e);
        }
        for s in 0..old_keys.len() {
            if old_used[s] != 0 {
                *self.entry(old_keys[s])? = old_values[s];
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn inserts_finds_and_grows() {
        let mut map: FlatMap<i32> = FlatMap::new(4).unwrap();
        for k in 0..1000 {
            *map.entry(pack_key(k, -k)).unwrap() = k + 1;
        }
        assert_eq!(map.len(), 1000);
        for k in 0..1000 {
            assert_eq!(map.find(pack_key(k, -k)), Some(&(k + 1)));
        }
        assert_eq!(map.find(pack_key(5, 5)), None);
    }

    #[test]
    fn pack_key_matches_cpp() {
        assert_eq!(pack_key(-1, 2), 0xFFFF_FFFF_0000_0002);
        assert_eq!(pack_key(1, -1), 0x0000_0001_FFFF_FFFF);
    }
}
