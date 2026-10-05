# infrastructure 層：實作 application 的 port。每個 adapter 一個 target，彼此不依賴。
# backend_opencv（S07）、backend_halcon（S08）、storage（S06）之後加在這裡，各自只連自己的函式庫。
add_subdirectory(backend_fake)
add_subdirectory(perf)
add_subdirectory(threading)
