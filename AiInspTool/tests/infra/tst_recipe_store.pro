# 配方（v1 範例檔在 tests/infra/fixtures/recipe-v1.json：手寫、不要改，之後的版本也要讀得進來）。

TARGET = tst_recipe_store
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_recipe_store.cpp
