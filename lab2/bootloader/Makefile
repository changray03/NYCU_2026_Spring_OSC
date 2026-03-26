RISCV_GNU ?= riscv64-unknown-elf
CC = $(RISCV_GNU)-gcc
LD = $(RISCV_GNU)-ld
OBJCOPY = $(RISCV_GNU)-objcopy
CFLAGS = -mcmodel=medany -ffreestanding -nostdlib -g -Wall -march=rv64gc_zifencei -mabi=lp64d
QEMU ?= qemu-system-riscv64
TARGET = bootloader

build: clean
	$(CC) $(CFLAGS) -c *.S *.c
	$(LD) -T link.ld --defsym KERNEL_START=0x00200000 -o $(TARGET).elf *.o
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

test: clean
	$(CC) $(CFLAGS) -c *.S *.c -DDEBUG
	$(LD) -T link.ld --defsym KERNEL_START=0x80200000 -o $(TARGET).elf *.o
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

run: test $(TARGET).bin
	$(QEMU) -machine virt -m 256M -kernel $(TARGET).bin -display none -serial pty -initrd initramfs.cpio
clean:
	rm -f $(TARGET).bin $(TARGET).elf *.o
