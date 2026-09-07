# Operating System Capstone (OSC) Labs

這個 repository 收錄國立陽明交通大學「作業系統總整與實作」課程的 Lab。專案以 RISC-V bare-metal programming 為主，從一個只能啟動的程式開始，逐步加入硬體驅動、記憶體管理、中斷、行程、虛擬記憶體與檔案系統，最後形成一個可執行 user program 的簡易 OS kernel。

專案可在 QEMU RISC-V `virt` machine 上測試，部分 Lab 也包含 Orange Pi RV2 的建置與 UART 傳輸流程。Kernel 以 freestanding C 與 RISC-V assembly 撰寫，不依賴一般作業系統提供的 runtime。

## Lab 0 — Minimal Kernel Image

建立最小的 RISC-V bare-metal 程式，確認交叉編譯、連結與 kernel image 的基本流程。

- 撰寫 `_start` assembly entry point，讓 CPU 進入等待中斷的迴圈。
- 使用 linker script 將 `.text`、`.rodata`、`.data` 與 `.bss` 放到指定的記憶體位置。

## Lab 1 — Kernel Boot and UART

完成 kernel 的基本啟動環境，並透過 UART 提供第一個互動式 shell。

- 在 assembly startup code 中設定 stack pointer、global pointer 並清除 BSS。
- 實作 polling-based UART driver，支援字元、字串與十六進位輸出。
- 封裝 OpenSBI calls，取得 SBI specification 與 implementation 資訊。
- 實作 `help`、`hello`、`info` 等基本 shell commands。
- 透過不同 linker address 支援 QEMU 與 Orange Pi RV2。

## Lab 2 — UART Bootloader, FDT and Initramfs

把 bootloader 與 kernel 分開，讓 kernel image 可以在系統啟動後經由 UART 載入。

- 實作 UART bootloader，搭配 host 端的 Python script 傳送 kernel binary，完成載入後跳轉執行。
- 解析 Flattened Device Tree（FDT），動態取得 UART base address 與 initrd 位置。
- 解析 `newc` 格式的 CPIO archive，將 initramfs 當作初期檔案來源。
- 在 shell 中加入 `ls` 與 `cat`，列出及讀取 initramfs 內的檔案。

## Lab 3 — Physical Memory Allocator

建立 kernel 的實體記憶體配置機制，讓後續子系統可以動態取得與釋放記憶體。

- 從 device tree 取得可用實體記憶體範圍。
- 實作 startup allocator，配置 allocator metadata 所需空間。
- 保留 kernel image、DTB、initramfs 與 `/reserved-memory` 描述的區域，避免被錯誤覆寫。
- 實作 buddy system，以不同 order 管理連續 page frames，並在釋放時合併 buddy blocks。
- 實作 slab allocator，處理小型 kernel objects 的配置與回收。

## Lab 4 — Exceptions and Interrupts

建立 RISC-V trap handling 架構，讓 kernel 能處理同步例外與非同步硬體中斷。

- 在 exception entry 保存與還原 registers，並依照 `scause` 分派 trap。
- 處理 timer interrupt、UART external interrupt 與 user-mode `ecall`。
- 實作 PLIC 的初始化、interrupt claim 與 completion 流程。
- 將 UART 改為 interrupt-driven I/O，使用 RX/TX ring buffers 暫存資料。
- 實作 timer queue 與具有 priority 的 deferred tasks；高優先權工作可以搶占低優先權工作。
- 從 initramfs 載入程式，透過 `sret` 進入 RISC-V user mode 執行。

## Lab 5 — Multitasking and System Calls

加入 process abstraction 與 scheduler，讓多個 kernel threads 和 user processes 能共享 CPU。

- 定義 task control block，保存 CPU context、kernel/user stacks、PID 與 process state。
- 實作 run queue、round-robin scheduling、context switch、idle thread 與 zombie cleanup。
- 支援 user process 的建立、執行、結束與等待流程。
- 建立 system call dispatch，包含 UART I/O、`getpid`、`exec`、`fork`、`waitpid`、`exit`、`usleep` 等操作。
- 實作 signal registration、delivery、`kill` 與 `sigreturn`。
- 支援 QEMU ramfb 初始化與 user program 的 framebuffer 顯示 system call。

## Lab 6 — Virtual Memory and Paging

啟用 RISC-V Sv39 MMU，為 kernel 與每個 user process 建立虛擬位址空間。

- 建立 three-level page tables，映射 kernel、RAM、UART、PLIC 與其他 MMIO regions。
- 將 kernel 移至 higher-half virtual address，啟用 MMU 後移除 early identity mapping。
- 為每個 process 維護獨立的 user page table，並在 context switch 時切換 `satp`。
- 實作 page fault handler，按需配置 user code、stack 與 `mmap` regions。
- 實作 `mmap` system call 與 VMA 管理，依照 read/write/execute 權限建立 mappings。
- 在 `fork` 中使用 copy-on-write，共享 read-only pages；寫入時才複製 page，並以 reference count 管理回收。

## Lab 7 — Virtual File System

在既有 process 與 virtual memory 架構上加入 VFS，提供一致的檔案與裝置存取介面。

- 定義 `filesystem`、`mount`、`vnode`、`file`、file operations 與 vnode operations。
- 實作 absolute/relative pathname lookup、current working directory、mount point traversal 與 filesystem registration。
- 實作 tmpfs，支援記憶體中的目錄與一般檔案建立、讀寫及 seek。
- 實作 ramfs，將 initramfs/CPIO 的內容掛載到 VFS namespace。
- 實作 devfs，透過 `/dev/uart` 與 `/dev/framebuffer` 存取 UART 和 framebuffer devices。
- 為每個 process 加入 file descriptor table，並支援 `open`、`close`、`read`、`write`、`mkdir`、`mount`、`chdir`、`lseek64` 與 `ioctl` system calls。

## Build and Run

需要 RISC-V GNU cross toolchain（預設 prefix 為 `riscv64-unknown-elf`）及 `qemu-system-riscv64`。大部分 Lab 可在各自目錄使用：

```bash
make run    # Build and run on QEMU
make test   # Build the QEMU kernel image
make        # Build the default target（部分 Lab 使用 make all 或 make build）
make clean  # Remove generated files
```

Lab 2 的 bootloader 與 kernel 各有自己的 `Makefile`，需要分別建置；`transmit_kernel.py` 用來把 kernel image 傳送到實體板或 QEMU 提供的 UART pseudo-terminal。各 Lab 的實際 targets 與部署方式請參考對應目錄中的 `README.md` 和 `Makefile`。
