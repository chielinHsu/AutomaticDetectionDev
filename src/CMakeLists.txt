# 依賴方向由這裡的 target_link_libraries 表達（docs/architecture.md）：
#   app ──► presentation ──► domain
# application、infrastructure 從 S03 起才有程式碼，S02 不建立空的 target。
add_subdirectory(domain)
add_subdirectory(presentation)
add_subdirectory(app)
