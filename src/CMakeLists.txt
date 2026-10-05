# 依賴方向由這裡的 target_link_libraries 表達（docs/architecture.md）：
#   app ──► presentation ──► (S04 起) application ──► domain
#   app ──► infrastructure ──► application ──► domain
# 違規的 #include 由 tools/check-deps.mjs 擋下（掛在 CTest）。
add_subdirectory(domain)
add_subdirectory(application)
add_subdirectory(infrastructure)
add_subdirectory(presentation)
add_subdirectory(app)
