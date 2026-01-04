# KDECodexBar

**KDECodexBar** is a native Linux/KDE port of **[CodexBar](https://github.com/steipete/CodexBar)** by **[Peter Steinberger (@steipete)](https://github.com/steipete)**.

It tracks your usage limits for various AI providers (Codex, Claude, Gemini, Antigravity) directly from your system tray.

## Features

*   **Real-time Usage Tracking**: Monitors API usage boundaries for:
    *   **Codex** (OpenAI)
    *   **Claude** (Anthropic)
    *   **Gemini** (Google)
    *   **Antigravity** (Cursor/IDE integration)
*   **System Tray Integration**:
    *   **Dynamic Icon**: Shows a visual pie-chart representation of your remaining quota for the selected provider.
    *   **Tooltip**: Displays detailed usage percentages and model breakdowns on hover.
    *   **Context Menu**: Quickly switch which provider's metrics are displayed in the icon.
*   **Antigravity Metrics**: Specifically customized order (Pro -> Claude -> Flash) for optimal visibility.
*   **Settings & Customization**:
    *   **Refresh Interval**: Configurable polling (Manual, 1 min, 3 min, 5 min, 15 min).
    *   **Autostart**: Easily toggle "Run at Startup" for seamless integration.

## Prerequisites

This application relies on **Qt 6** and **KDE Frameworks 6**. 

## Installation

### Arch Linux (AUR)
Install from the AUR package ([kdecodexbar](https://aur.archlinux.org/packages/kdecodexbar)):
```bash
yay -S kdecodexbar
```

### Arch Linux Dependencies
```bash
sudo pacman -S cmake extra-cmake-modules qt6-base qt6-tools kstatusnotifieritem kcoreaddons kconfig ki18n kwindowsystem
```

## Build Instructions

1.  **Clone the repository** (if you haven't already).

2.  **Create a build directory**:
    ```bash
    mkdir build
    cd build
    ```

3.  **Configure with CMake**:
    ```bash
    cmake ..
    ```

4.  **Build**:
    ```bash
    make -j$(nproc)
    ```

## Usage

### Running
Run the executable from the build directory:
```bash
./src/app/kdecodexbar
```

### Installation
To install system-wide (optional):
```bash
sudo make install
```
This will install the binary and the `.desktop` file, allowing you to launch it from your application launcher (search for "KDECodexBar").

### Configuration
1.  Right-click the tray icon.
2.  Select **Settings**.
3.  Adjust the **Refresh Interval** or toggle **Run at Startup**.
4.  Data is automatically refreshed based on the interval. You can also manually trigger a **Refresh All** from the context menu.

## License
MIT
