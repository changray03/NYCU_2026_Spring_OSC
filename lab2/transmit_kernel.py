import struct
import sys

path = sys.argv[1]

with open('kernel/kernel.bin', 'rb') as f:
    kernel_data = f.read()

header = struct.pack('<II', 0x544F4F42, len(kernel_data))

with open(path, 'wb', buffering=0) as tty:
    tty.write(header)
    tty.write(kernel_data)