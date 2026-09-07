.PHONY: all test run clean
RISCV_GNU ?= riscv64-unknown-elf
CC = $(RISCV_GNU)-gcc
LD = $(RISCV_GNU)-ld
OBJCOPY = $(RISCV_GNU)-objcopy
CFLAGS = -march=rv64gc_zicbom -mcmodel=medany -ffreestanding -nostdlib -g -Wall -Iinc
QEMU ?= qemu-system-riscv64
TARGET = kernel

BUILD_DIR = build
SRCS_C = $(wildcard *.c) $(wildcard src/*.c)
SRCS_S = $(wildcard *.S)
OBJS = $(patsubst %.c, build/%.o, $(SRCS_C))
OBJS += $(patsubst %.S, build/%.o, $(SRCS_S))

all: clean $(OBJS)
	$(LD) -T link.ld --defsym KERNEL_START=0x00200000 -o $(TARGET).elf $(OBJS)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

test: CFLAGS += -DDEBUG
test: clean $(OBJS)
	$(LD) -T link.ld --defsym KERNEL_START=0x80200000 -o $(TARGET).elf $(OBJS)
	$(OBJCOPY) -O binary $(TARGET).elf $(TARGET).bin

run: test
	$(QEMU) -machine virt -m 1G -kernel $(TARGET).bin -serial stdio -initrd initramfs.cpio -device ramfb -vnc :0

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D) 
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D) #
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET).elf $(TARGET).bin