import struct
import sys
import time

path = sys.argv[1]

with open('kernel.bin', 'rb') as f:
    kernel_data = f.read()

header = struct.pack('<II', 0x544F4F42, len(kernel_data))

with open(path, 'r+b', buffering=0) as tty:
    tty.write(header)
    tty.write(kernel_data)

print("Kernel sent successfully!")