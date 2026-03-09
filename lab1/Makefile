RISCV_GNU ?= riscv64-unknown-elf
CC = $(RISCV_GNU)-gcc
LD = $(RISCV_GNU)-ld
OBJCOPY = $(RISCV_GNU)-objcopy
CFLAGS = -mcmodel=medany -ffreestanding -nostdlib -g -Wall
QEMU ?= qemu-system-riscv64
TARGET = kernel

build: clean
	$(CC) $(CFLAGS) -c *.S *.c -g
	$(LD) -T link.ld -o $(TARGET).elf *.o
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET)

run: build $(TARGET)
	qemu-system-riscv64 -M virt -kernel kernel -display none -S -s

clean:
	rm -f $(TARGET) $(TARGET).elf *.o
