# ImRISC is a RISC-V RV32I(M)(Zicsr) Emulator that runs DOOM!

### Features
- M Multipy Extension (Optional), adds 'hardware' multplication
- Zicsr Extension (Optional?) adds extra CSR registers
- Disassembly with symbols & labels, register debug 
- Graphical Interface with PC debug, Cpu speed & VGA FrameBuffer
- MMIO (ROM, WAD-ROM, RAM, BRAM, VGA, UART, SPI)
- Multi core support* (*Experimental, needs A atomic extension not yet implemented...)

![image](https://github.com/user-attachments/assets/aa8c1f37-3487-48cf-ab98-e921c3c32856)


### Build Guide
In case of issues might be worth cheking the build guide for GLFW first at https://www.glfw.org/docs/3.3/compile.html

#### Linux (X11):
`sudo apt install build-essential`\
`sudo apt install xorg-dev`\
`git clone https://github.com/Paulo-D2000/imrisc.git`\
`cd imrisc`\
`mkdir build`\
`cd build`\
`cmake -DCMAKE_BUILD_TYPE=Release ..`\
`cmake --build . --target ImRisc-gui --`\
Then run `ImRisc-gui` with `.ImRisc-gui`and enjoy!

#### Linux (Wayland):
`sudo apt install build-essential`\
`sudo apt install libwayland-dev libxkbcommon-dev wayland-protocols extra-cmake-modules`\
`git clone https://github.com/Paulo-D2000/imrisc.git`\
`cd imrisc`\
`mkdir build`\
`cd build`\
`cmake -DCMAKE_BUILD_TYPE=Release ..`\
`cmake --build . --target ImRisc-gui --`\
Then run `ImRisc-gui` with `.ImRisc-gui`and enjoy!

#### Windows (General guide):
Dependencies:
GCC (MinGW-W64 > 12) (https://github.com/niXman/mingw-builds-binaries/releases)\
CMake (Cmake > 3.2) (https://cmake.org/download/)\
Ninja (https://github.com/ninja-build/ninja/releases) or Make (https://gnuwin32.sourceforge.net/packages/make.htm)\
(Optional) Git (https://git-scm.com/downloads)

If you have Git, run on cmd:\
`git clone https://github.com/Paulo-D2000/imrisc.git`\
`cd imrisc`

Or just download the repo as a zip and extract it to some place, then open cmd on that folder

Then run:\
`mkdir build`\
`cd build`\
`cmake -DCMAKE_BUILD_TYPE=Release ..`\
`cmake --build . --target ImRisc-gui --`\
Then run `ImRisc-gui` with `ImRisc-gui.exe`and enjoy!


### Small Usage Guide:
File formats:
- `.bin` actual binaries, made with objcopy -B binary of the .elf file
- `.sym` Symbols dump, made with obdjump -D of the .elf file
- `.WAD` DOOM WAD File

You can change the core count on [config.h](config.h), note this isn't working correctly right now!

You can load .bin files into RAM or ROM and add breakpoints at addresses, check the commented lines on [main.cpp](main.cpp#L170) to see how its done (TODO: Document This).

### TODO
- Update Documentation
- Add binary build guide & examples, linkerscript settings, uart addresses... etc...
- Add A atomic extension ? 
- Add a cmd (maybe even gui?) interface that allows user to load binaries, setup extensions, configure Number of cores, etc...
- Add a new framebuffer 'type' (RGB[A]) instead of 256 colors pallete

Thanks to [Lucas Teske](https://github.com/racerxdl) for his amazing [riscv-emulator](https://github.com/racerxdl/riscv-emulator) that was base for a lot of the code on this project! Also thanks to [smunaut](https://github.com/smunaut) for his [bootloader](https://github.com/smunaut/ice40-playground/tree/master/projects/riscv_doom) and the riscv port of DOOM [riscv_doom](https://github.com/smunaut/doom_riscv).
