# SimpleScope (Qt) — GUI or Terminal View

This refactor introduces an abstract `scpView` interface with two implementations:
- `scpScopeView` — the original Qt Widgets graphical oscilloscope.
- `scpTerminalView` — an ASCII terminal oscilloscope that prints to stdout and accepts simple stdin commands (POSIX).

## Build
Same as before (Qt 6, CMake 3.21+):

```bash
mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/<platform> ..
cmake --build . --config Release
```

## Run
GUI (default):
```bash
./SimpleScope --view=gui --source=audio --start
```

Terminal:
```bash
./SimpleScope --view=terminal --source=gen --start -f 1000
```
On POSIX terminals you can type commands while it runs:
```
help
start
stop
timebase_ms 50
scale 1.0
freq 1000
quit
```

Notes:
- Interactive stdin uses `QSocketNotifier` (POSIX). On Windows, terminal still prints frames but interactive commands are disabled in this minimal sample.
- The terminal view uses a fixed 80x20 ASCII grid with an envelope renderer.

RUN Command 

Open 
Qt → Qt 6.10.1 → Qt 6.10.1 for Desktop (MinGW 64-bit)

You'll recognize it because the prompt shows something like:
C:\Qt\6.10.1\mingw_64>


cd D:\Stevens\Course\SSW 565 Software Architecture\ScopeUI
rmdir /s /q build
mkdir build
cd build

cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" ..

mingw32-make -j8


.\SimpleScope.exe --view=gui --source=audio --start

.\SimpleScope.exe --view=gui --source=gen --start -f 1000

----------------------------------------------------------------
cmake -G "MinGW Makefiles" ..
mingw32-make -j8

 
SimpleScope.exe --source=msg --start


-----------------------------------------------------------

1. Message Waveform (Your Feature)
bash# Basic message
SimpleScope.exe --source=msg --msg="HELLO" --start

# Short message with clear steps
SimpleScope.exe --source=msg --msg="ABC" --start

# Message with numbers
SimpleScope.exe --source=msg --msg="12345" --start

# Long message
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start

-----------------------------------------------------------

Audio Input (Microphone)
bash# Start with microphone input
SimpleScope.exe --source=audio --start

# Audio with custom timebase
SimpleScope.exe --source=audio --start --timebase=3



-----------------------------------------------------------
Signal Generator (Sine Wave)

# Default 440 Hz
SimpleScope.exe --source=gen --start

# Custom frequency (1000 Hz)
SimpleScope.exe --source=gen --start --gen-freq=1000

# Lower frequency (100 Hz)
SimpleScope.exe --source=gen --start --gen-freq=100



-----------------------------------------------------------

Timebase Options (--timebase)
bash# Index 0: 5 ms/div (fast sweep)
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=0

# Index 1: 10 ms/div
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=1

# Index 2: 20 ms/div
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=2

# Index 3: 50 ms/div (default)
SimpleScope.exe --source=msg --msg="HELLO" --start --timebase=3

# Index 4: 100 ms/div
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start --timebase=4

# Index 5: 200 ms/div (slow sweep)
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start --timebase=5