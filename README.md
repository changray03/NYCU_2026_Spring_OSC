# 作業系統總整與實作 (OSC) - LAB
這裡是我在國立陽明交通大學學習「作業系統總整與實作」的 Lab。主要內容為 Bare-metal Programming，從頭建立一個簡易的 OS kernel。

## Lab 1
* UART Driver 開發
* OpenSBI interface
* Memory layout using linker script
* BSS clear, SP pointer setup using Assembly

## Lab 2
* 動態載入 kernel image by UART Bootloader
* Device tree parsing
* initrd.cpio parsing

## Lab3
* Memory management using Buddy System and Slab Allcoator
* 處理 Reserved Memory 避免修改到硬體資料
* 開發 Startup Allocator 以處理 buddy system 的 metadata 的空間分配
