======================
Andes QEMU Quick Start
======================

| This document provides a default example for building and using Andes QEMU.
| For more details, please refer to the `QEMU's documentation <https://www.qemu.org/docs/master/index.html>`_.


Get the source
==============

Clone the Andes QEMU repository:

.. code-block:: bash

    git clone https://github.com/andestech/qemu.git


Build in Ubuntu
===============

Install the required dependencies:

.. code-block:: bash

    sudo apt-get install git libglib2.0-dev libfdt-dev libpixman-1-dev zlib1g-dev ninja-build

Navigate to the cloned qemu directory and create a build directory:

.. code-block:: bash

    cd qemu
    mkdir build
    cd build

Configure the build options:

.. code-block:: bash

    ../configure --target-list=riscv64-softmmu,riscv32-softmmu --disable-werror --disable-pa --disable-alsa --disable-gtk --disable-curses --disable-sdl --disable-vnc --disable-opengl --disable-libusb

Build QEMU:

.. code-block:: bash

    make -j`nproc`

| After successful building, you should have the binaries :code:`qemu-system-riscv64` and :code:`qemu-system-riscv32`.
| For more detailed information about building, refer to the `QEMU on Linux hosts <https://wiki.qemu.org/Hosts/Linux>`_.


QEMU Options
============

=================   ==================================================
   Option               Description
=================   ==================================================
-nographic          Direct the output to the terminal.
-M andes_ae350      Use Andes AE350 platform.
-cpu andes_ax45     Use Andes core AX45 for RV64.
-m 2G               Set system memory to 2G(maximum in AE350 platform).
-bios *elf_file*    Run the elf file.
=================   ==================================================

Run a demo
==========

| To run a simple printf/scanf demo, first compile the demo.
| Please refer to the **Andes BSP User Manual** for instructions.
|
| Then execute the following command:

.. code-block:: bash

    qemu-system-riscv64 -nographic \
                        -M andes_ae350 -cpu andes-ax45 \
                        -m 2G -bios demo-printf-V5/CmdMakefile/Demo.elf


Contact
=======

If you encounter any issues or problems, please contact nephih@andestech.com
for assistance.