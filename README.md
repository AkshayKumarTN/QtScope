# QtScope

Multi-source oscilloscope application with interactive terminal commands and dual view modes (GUI/Terminal). Built with Qt6 and modern C++.

## Features

- 🎤 **Audio Input** - Real-time microphone/line-in visualization
- 🔊 **Signal Generator** - Configurable sine wave generator (1-20000 Hz)
- 📡 **FTDI USB Device** - External device support
- 💻 **Interactive Terminal** - Runtime control via commands
- 🖥️ **Dual View Mode** - GUI (Qt Widgets) or ASCII Terminal
- ⚡ **Real-time Processing** - Multi-threaded architecture with 60 FPS rendering
- ✨ **Bonus Feature** - Message waveform (text-to-signal converter)

## Interactive Terminal Commands

Run in interactive mode:
```bash
SimpleScope.exe --interactive --source=gen
```

Available commands:
```
scope start          # Start acquisition
scope stop           # Stop acquisition
sampleTime=1ms       # Set sample time
sampleFor=10s        # Sample for duration then auto-stop
status               # Show current status
message=<text>       # Set message (for message source)
help                 # Show all commands
exit                 # Exit application
```

## Build Instructions

### Requirements
- Qt 6.10.1 or higher
- C++17 compiler
- CMake 3.16+
- MinGW 64-bit (Windows) / GCC (Linux)

### Windows Build (Qt MinGW)

**Step 1:** Open Qt command prompt
- Start Menu → Qt → Qt 6.10.1 → **Qt 6.10.1 for Desktop (MinGW 64-bit)**

**Step 2:** Navigate and build
```bash
cd "D:\Stevens\Course\SSW 565 Software Architecture\ScopeUI"

# Clean previous build (optional)
rmdir /s /q build

# Create and configure
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" ..

# Build
mingw32-make -j8
```

### Quick Build (If Qt in PATH)
```bash
mkdir build && cd build
cmake -G "MinGW Makefiles" ..
mingw32-make -j8
```

## Usage Examples

### Audio Input (Microphone)
```bash
# Basic audio visualization
SimpleScope.exe --source=audio --start

# With custom timebase
SimpleScope.exe --source=audio --start --timebase=3
```

### Signal Generator (Sine Wave)
```bash
# Default 440 Hz
SimpleScope.exe --source=gen --start

# Custom frequency (1000 Hz)
SimpleScope.exe --source=gen --start --gen-freq=1000

# Lower frequency (100 Hz)
SimpleScope.exe --source=gen --start --gen-freq=100
```

### Message Waveform (Bonus Feature)
```bash
# Basic message
SimpleScope.exe --source=msg --msg="HELLO" --start

# Short message with clear steps
SimpleScope.exe --source=msg --msg="ABC" --start

# Numbers (different ASCII range)
SimpleScope.exe --source=msg --msg="12345" --start
```

### Interactive Terminal Mode ⭐
```bash
# Launch interactive mode
SimpleScope.exe --interactive --source=gen

# Example session:
> scope start
[OK] Acquisition started

> sampleTime=50ms
[OK] Sample time set to 50 ms

> sampleFor=10s
[OK] Sample duration set to 10000 ms (10 s)

> status
[Status] Running: YES
[Status] Sample Rate: 48000 Hz

> scope stop
[OK] Acquisition stopped
```

### Terminal View Mode (ASCII Art)
```bash
# Audio in terminal
SimpleScope.exe --view=terminal --source=audio --start

# Generator in terminal
SimpleScope.exe --view=terminal --source=gen --start --gen-freq=1000
```

**Note:** Interactive stdin uses QSocketNotifier (POSIX). On Windows, terminal view prints frames but interactive commands work best with `--interactive` flag.

## Command-Line Options

### Timebase Settings (--timebase)
```bash
--timebase=0    # 5 ms/div (fast sweep)
--timebase=1    # 10 ms/div
--timebase=2    # 20 ms/div
--timebase=3    # 50 ms/div (default)
--timebase=4    # 100 ms/div
--timebase=5    # 200 ms/div (slow sweep)
```

**Examples:**
```bash
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=0
SimpleScope.exe --source=audio --start --timebase=3
```

### Vertical Scale (--scale)
```bash
--scale=0    # 0.2 units/div
--scale=1    # 0.5 units/div
--scale=2    # 1.0 units/div (default)
--scale=3    # 2.0 units/div
--scale=4    # 5.0 units/div
```

### All Options
```
--source=<type>      Source: audio | gen | ftdi | msg
--start              Start acquisition immediately
--interactive        Interactive terminal command mode
--view=<mode>        View: gui | terminal (default: gui)
--timebase=<idx>     Timebase index (0-5)
--scale=<idx>        Vertical scale index (0-4)
--gen-freq=<hz>      Generator frequency in Hz (1-20000)
--msg=<text>         Message text (for message source)
--help               Show help
--version            Show version
```

## Architecture

### Design Patterns
- **Strategy Pattern** - Runtime source selection via abstract interface
- **Observer Pattern** - Signal/slot mechanism for real-time data flow
- **Interface Segregation** - Clean separation via `scpDataSource` and `scpView`

### Core Interfaces

**Data Source Interface:**
```cpp
class scpDataSource : public QObject {
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isActive() const = 0;
    virtual int sampleRate() const = 0;
    virtual int copyRecentSamples(int count, QVector<float>& out) = 0;
signals:
    void samplesReady(const float* data, int count);
};
```

**View Interface:**
```cpp
class scpView {
    virtual void setSource(scpDataSource* src) = 0;
    virtual void setTotalTimeWindowSec(double sec) = 0;
    virtual void setVerticalScale(float units) = 0;
};
```

### Implementations

**Data Sources:**
- `scpAudioInputSource` - Microphone/line input via Qt Multimedia
- `scpSignalGeneratorSource` - Sine wave generator (software-based)
- `scpFtdiSource` - FTDI USB device interface
- `scpMessageWaveSource` - Text-to-ASCII waveform converter (bonus)

**Views:**
- `scpScopeView` - Qt Widgets graphical oscilloscope (60 FPS)
- `scpViewTerminal` - ASCII terminal oscilloscope (80x20 grid)

**Controllers:**
- `scpMainWindow` - GUI controller with interactive message input
- `scpTerminalController` - Interactive command-line controller

### Multi-threading
- Background threads for continuous sample generation
- `std::mutex` protection for shared data structures
- Qt queued signals for thread-safe cross-thread communication
- Non-blocking UI rendering at 60 FPS

## Project Structure
```
QtScope/
├── src/
│   ├── main.cpp
│   ├── scpDataSource.h                  # Abstract interface
│   ├── scpView.h                        # View interface
│   ├── scpAudioInputSource.cpp/h        # Microphone input
│   ├── scpSignalGeneratorSource.cpp/h   # Sine generator
│   ├── scpMessageWaveSource.cpp/h       # Message waveform
│   ├── scpFtdiSource.cpp/h              # FTDI device
│   ├── scpMainWindow.cpp/h              # GUI controller
│   ├── scpScopeView.cpp/h               # GUI view
│   ├── scpViewTerminal.cpp/h            # Terminal view
│   └── scpTerminalController.cpp/h      # Interactive CLI
├── CMakeLists.txt
├── README.md
└── .gitignore
```

## Demo Commands Quick Reference
```bash
# GUI with audio
SimpleScope.exe --source=audio --start

# GUI with generator (1kHz)
SimpleScope.exe --source=gen --start --gen-freq=1000

# Interactive terminal mode ⭐
SimpleScope.exe --interactive --source=gen

# Terminal ASCII view
SimpleScope.exe --view=terminal --source=audio --start

# Message waveform demo
SimpleScope.exe --source=msg --msg="HELLO" --start --timebase=2 --scale=4
```

## Technologies Used
- **Qt 6.10.1** - GUI framework, multimedia, and utilities
- **C++17** - Modern C++ features (std::thread, std::mutex, std::atomic)
- **CMake** - Cross-platform build system
- **MinGW 64-bit** - Compiler toolchain (Windows)
- **Qt Multimedia** - Audio input support

## Course Information
**Course:** SSW 565 Software Architecture  
**Institution:** Stevens Institute of Technology  
**Focus:** Design patterns, multi-threading, interface-based architecture

## License
Educational project - All rights reserved

---

**Key Features Summary:**
✅ Multi-source architecture (audio, generator, FTDI, message)  
✅ Interactive terminal commands (`scope start`, `sampleTime=1ms`, `sampleFor=10s`)  
✅ Dual view modes (GUI and ASCII terminal)  
✅ Real-time processing with multi-threading  
✅ Extensible design patterns  
✅ Command-line automation support





Notes:

Interactive stdin uses QSocketNotifier (POSIX). On Windows, terminal still prints frames but interactive commands are disabled in this minimal sample.
The terminal view uses a fixed 80x20 ASCII grid with an envelope renderer.
RUN Command

Open Qt → Qt 6.10.1 → Qt 6.10.1 for Desktop (MinGW 64-bit)

You'll recognize it because the prompt shows something like: C:\Qt\6.10.1\mingw_64>

cd D:\Stevens\Course\SSW 565 Software Architecture\ScopeUI rmdir /s /q build mkdir build cd build

cmake -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH="C:/Qt/6.10.1/mingw_64" ..

mingw32-make -j8

.\SimpleScope.exe --view=gui --source=audio --start

.\SimpleScope.exe --view=gui --source=gen --start -f 1000

cmake -G "MinGW Makefiles" .. mingw32-make -j8

SimpleScope.exe --source=msg --start

Message Waveform (Your Feature) bash# Basic message SimpleScope.exe --source=msg --msg="HELLO" --start
Short message with clear steps
SimpleScope.exe --source=msg --msg="ABC" --start

Message with numbers
SimpleScope.exe --source=msg --msg="12345" --start

Long message
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start

Audio Input (Microphone) bash# Start with microphone input SimpleScope.exe --source=audio --start

Audio with custom timebase
SimpleScope.exe --source=audio --start --timebase=3

Signal Generator (Sine Wave)

Default 440 Hz
SimpleScope.exe --source=gen --start

Custom frequency (1000 Hz)
SimpleScope.exe --source=gen --start --gen-freq=1000

Lower frequency (100 Hz)
SimpleScope.exe --source=gen --start --gen-freq=100

Timebase Options (--timebase) bash# Index 0: 5 ms/div (fast sweep) SimpleScope.exe --source=msg --msg="ABC" --start --timebase=0

Index 1: 10 ms/div
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=1

Index 2: 20 ms/div
SimpleScope.exe --source=msg --msg="ABC" --start --timebase=2

Index 3: 50 ms/div (default)
SimpleScope.exe --source=msg --msg="HELLO" --start --timebase=3

Index 4: 100 ms/div
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start --timebase=4

Index 5: 200 ms/div (slow sweep)
SimpleScope.exe --source=msg --msg="HELLO WORLD" --start --timebase=5