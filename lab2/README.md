[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/-oo4r1dh)

# Makefile Operation
## Deploy on board
* Build target for board: 
    * **bootloader/**: `make`
    * **kernel/**: `make`

* Run python script: python3 transmit_kernel.py /dev/ttyUSB0

## Deploy on QEMU
* Build target for QEMU: 
    * **bootloader/**: `make run`
    * **kernel/**: `make test`

* Run python script: python3 transmit_kernel.py /dev/pts/X (X depends on the port that QEMU uses)