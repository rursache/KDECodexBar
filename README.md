# CodexBar for Linux (KDE Plasma)

This is a native port of CodexBar for KDE Plasma on Arch Linux.
It requires Qt 6 and KDE Frameworks 6.

## Prerequisites

Ensure you have the following packages installed (Arch Linux):

```bash
sudo pacman -S cmake extra-cmake-modules qt6-base qt6-tools kstatusnotifieritem kcoreaddons kconfig ki18n kwindowsystem
```

## Build Instructions

1. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

2. Configure with CMake:
   ```bash
   cmake ..
   ```

3. Build:
   ```bash
   make -j$(nproc)
   ```

## Running

Run the executable from the build directory:

```bash
./src/app/codexbar
```

## Development Status

- **Phase 0 & 1 Complete**: Project structure and core data model.
- **Pending**: Tray UI implementation and specific provider strategies.
