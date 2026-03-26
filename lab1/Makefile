RISCV_GNU ?= riscv64-unknown-elf
CC = $(RISCV_GNU)-gcc
LD = $(RISCV_GNU)-ld
OBJCOPY = $(RISCV_GNU)-objcopy
CFLAGS = -mcmodel=medany -ffreestanding -nostdlib -g -Wall
QEMU ?= qemu-system-riscv64
TARGET = kernel

test: clean
	$(CC) $(CFLAGS) -c *.S *.c -DDEBUG
	$(LD) -T link.ld --defsym KERNEL_START=0x80200000 -o $(TARGET).elf *.o
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

build: clean
	$(CC) $(CFLAGS) -c *.S *.c
	$(LD) -T link.ld --defsym KERNEL_START=0x20000000 -o $(TARGET).elf *.o
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

run: test $(TARGET).bin
	$(QEMU) -M virt -m 256M -kernel $(TARGET).bin -display none -serial stdio

debug: test $(TARGET).bin
	qemu-system-riscv64 -M virt -kernel kernel.bin -display none -serial stdio -S -s

clean:
	rm -f $(TARGET) $(TARGET).elf *.o
