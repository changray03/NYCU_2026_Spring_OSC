.section ".text"
_start:
    wfi       # Wait For Interrupt (等待中斷)
    j _start  # 無限循環回到 _start