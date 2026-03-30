import struct
import sys
import time

path = sys.argv[1]

def wait_for_ack(ser, expected_ack='G'):
    print(f"Waiting for ACK '{expected_ack}' from bootloader...")
    while True:
        char = ser.read(1).decode('ascii', errors='ignore')
        if char == expected_ack:
            print("ACK received!")
            break
    time.sleep(0.01) # 避免 CPU 虛耗

with open('kernel/kernel.bin', 'rb') as f:
    kernel_data = f.read()

header = struct.pack('<II', 0x544F4F42, len(kernel_data))

with open(path, 'r+b', buffering=0) as tty:
    tty.write(header)
    chunk_size = 512
    for i in range(0, len(kernel_data), chunk_size):
        chunk = kernel_data[i : i + chunk_size]
        tty.write(chunk)
        tty.flush()
        
        # 如果還沒傳完，就等 Bootloader 說 OK
        if i + chunk_size < len(kernel_data):
            wait_for_ack(tty, 'G')

    print("Kernel sent successfully via Handshaking!")