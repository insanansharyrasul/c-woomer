# C-Woomer

This is repository is vibe coded by seeing [woomer](https://github.com/coffeeispower/woomer) because it has a problem by not running correctly on my laptop and only show a quarter screen of it so i made it in C.

## Features

- Take screenshots and zoom/pan through them
- Spotlight effect with customizable radius
- Wayland support using `grim` for screenshots
- Smooth camera controls with momentum
- Mouse wheel zoom and Ctrl+Shift for spotlight radius adjustment

## Dependencies

- **raylib** - Graphics library
- **grim** - Screenshot tool for Wayland (install via your package manager)

## Building

### Prerequisites

Make sure you have the following installed:
- CMake (3.15 or higher)
- A C compiler (GCC or Clang)
- raylib development libraries (optional - will be downloaded automatically if not found)
- grim (for Wayland screenshot functionality)

### Ubuntu/Debian
```bash
sudo apt update
sudo apt install cmake build-essential libraylib-dev grim
```

### Arch Linux
```bash
sudo pacman -S cmake gcc raylib grim
```

### Fedora
```bash
sudo dnf install cmake gcc raylib-devel grim
```

### Build Instructions

1. Clone the repository:
```bash
git clone <your-repo-url>
cd c-woomer
```

2. Create a build directory:
```bash
mkdir build
cd build
```

3. Configure with CMake:
```bash
cmake ..
```

4. Build the project:
```bash
make -j$(nproc)
```

5. Run the application:
```bash
./woomer
```

### Alternative: One-liner build
```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc) && ./woomer
```

## Usage

- **Left Mouse Button**: Pan around the screenshot
- **Mouse Wheel**: Zoom in/out
- **Ctrl**: Enable spotlight mode
- **Ctrl + Shift + Mouse Wheel**: Adjust spotlight radius
- **Right Mouse Button** or **Escape**: Exit application

## Notes

- This application requires a Wayland compositor and the `grim` tool for taking screenshots
- The application creates a temporary screenshot at `/tmp/woomer_screenshot.png`
- Make sure you're running on a Wayland session for the screenshot functionality to work
- Please change the `screenWidth` and `screenHeight` value manually, since raylib can't fetch the monitor size.

