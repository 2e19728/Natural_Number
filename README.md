# A Library of Natural Number Computations
## Compile on Linux:
```
nasm -f elf64 asmLinux.asm -o asmLinux.o
g++ -std=c++20 test.cpp asmLinux.o -o test
```
## Compile on Windows:
```
nasm -f win64 asmWindows.asm -o asmWindows.o
g++ -std=c++20 test.cpp asmWindows.o -o test
```
