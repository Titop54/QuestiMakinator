This is a simple version:

- Allows to get text for ftb quests mod more easily, with a lot more effects, using quests methods (aka, using &)
- Thats for now

To build, you have 2 main ways:

- Windows: Using msys2 mingw64 (https://www.msys2.org/) or WSL (https://learn.microsoft.com/en-us/windows/wsl/install), either way works, then follow linux instructions

- Linux: Download your prefer C++ compiler (most distros should have g++ or gcc) and cmake, then run ./setup.sh && ./launch.sh
         Optionally, you can pass --windows or --linux on launch.sh to compile to Windows or Linux (needs the correct compiler, on linux you can use mingw64)

Programs needed: Python3, cmake, a c++ compiler and a terminal compatible with bash scripts

# License
All Right Reserved on the code made, unless otherwise stated
setup.sh calls licenses.py which makes a .csv with all licenses notices
