## SDL2 Frontend — ARM64/ROCKNIX Porting Notes

This fork adds an SDL2-based frontend targeting **ARM64 Linux handheld devices** (e.g. ROCKNIX), with keyboard-only input and no Qt dependency. Below is a summary of the porting work built on top of the base SDL2 CLI frontend.

### Commits

| # | Commit | Description |
|---|--------|-------------|
| 1 | `aab8df20a` — **OpenGL ES backend for aarch64 Linux** | Switch from desktop OpenGL + GLX/X11 to OpenGL ES + EGL/Wayland on ARM64. Conditionally link `GLESv2` instead of `libGL`. |
| 2 | `03d68b925` — **Fix display on ARM64 Wayland** | Create `gl_context_sdl2` wrapper using SDL's own GL context, avoiding EGL conflicts. Add missing screen redraw callback and gamepad crash guard. |
| 3 | `883f79b28` — **App launcher UI** | Interactive app launcher rendered with SDL_ttf. Arrow-key navigation, Enter to launch. Auto-return to launcher when an app exits. Default keyboard bindings for Symbian keys. ROCKNIX cross-compilation toolchain. |
| 4 | `ccc026842` — **Grid layout + app icon thumbnails** | Upgrade launcher to grid/tile layout. Load real icons from MIF (SVGB/NVG→SVG via lunasvg), MBM, or `get_icon()` bitmap. Colored placeholder icons as fallback. |
| 5 | `862b6c332` — **`--install` exits after completion** | SIS package installation via CLI now exits cleanly instead of continuing into the emulator. |
| 6 | `9915e804a` — **Persistent graphics driver across app launches** | Keep the SDL window and graphics driver alive between app transitions (hide/show), preserving GPU texture handles and fixing black-screen on re-entry. |

### Key Bindings

| Key | Symbian Function |
|-----|-----------------|
| Arrow Keys | D-pad (Up/Down/Left/Right) |
| Enter | OK / Select |
| F1 / F2 | Left / Right Softkey |
| F3 / F4 | Green (Call) / Red (End) |
| Backspace | Backspace |
| 0–9 | Number keys |
| Escape | End key |

### Build (cross-compile for aarch64 ROCKNIX)

```bash
cmake -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-rocknix.cmake \
  -DENABLE_UNEXPECTED_EXCEPTION_HANDLER=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-aarch64 --target eka2l1_sdl2 -j$(nproc)
```

### Usage

```bash
# Install device firmware
./eka2l1_sdl2 --installdevice firmware.rpkg rom.rom

# Install a SIS game (exits after install)
./eka2l1_sdl2 --install Game.sis

# Launch (shows app launcher, select with arrow keys + Enter)
CUBEB_BACKEND=alsa ./eka2l1_sdl2
```

---

## SDL2 前端 — ARM64/ROCKNIX 移植说明

本分支为 **ARM64 Linux 掌机设备**（如 ROCKNIX）添加了基于 SDL2 的前端，仅需键盘输入，无 Qt 依赖。以下是在基础 SDL2 CLI 前端之上所做的移植工作总结。

### 提交记录

| # | Commit | 说明 |
|---|--------|------|
| 1 | `aab8df20a` — **aarch64 OpenGL ES 图形后端** | 在 ARM64 上从桌面 OpenGL + GLX/X11 切换到 OpenGL ES + EGL/Wayland，条件链接 `GLESv2`。 |
| 2 | `03d68b925` — **修复 ARM64 Wayland 显示** | 创建 `gl_context_sdl2` 封装使用 SDL 自身 GL 上下文，避免 EGL 冲突。补充缺失的屏幕重绘回调，修复手柄崩溃。 |
| 3 | `883f79b28` — **应用启动器 UI** | 使用 SDL_ttf 渲染的交互式应用启动器。方向键导航，Enter 启动。游戏退出后自动返回启动器。默认 Symbian 键盘映射。ROCKNIX 交叉编译工具链。 |
| 4 | `ccc026842` — **网格布局 + 应用图标缩略图** | 启动器升级为网格平铺布局。从 MIF（SVGB/NVG→SVG，通过 lunasvg 渲染）、MBM 或 `get_icon()` 加载真实图标。无图标时生成彩色占位图标。 |
| 5 | `862b6c332` — **`--install` 安装后退出** | CLI 安装 SIS 包后干净退出，不再继续启动模拟器。 |
| 6 | `9915e804a` — **跨应用启动保持 Graphics Driver** | 在应用切换间保持 SDL 窗口和图形驱动存活（隐藏/显示），保留 GPU 纹理句柄，修复再次进入游戏时黑屏问题。 |

### 按键映射

| 按键 | Symbian 功能 |
|------|-------------|
| 方向键 | 上/下/左/右 |
| Enter | 确认/选择 |
| F1 / F2 | 左/右软键 |
| F3 / F4 | 绿色拨号键/红色挂断键 |
| Backspace | 退格 |
| 0–9 | 数字键 |
| Escape | 挂断键 |

### 编译（aarch64 ROCKNIX 交叉编译）

```bash
cmake -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-rocknix.cmake \
  -DENABLE_UNEXPECTED_EXCEPTION_HANDLER=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-aarch64 --target eka2l1_sdl2 -j$(nproc)
```

### 使用方法

```bash
# 安装设备固件
./eka2l1_sdl2 --installdevice firmware.rpkg rom.rom

# 安装 SIS 游戏（安装后自动退出）
./eka2l1_sdl2 --install Game.sis

# 启动（显示应用启动器，方向键选择 + Enter 进入）
CUBEB_BACKEND=alsa ./eka2l1_sdl2
```

---

<div class="header">
  <p align="center">
     <img src="https://i.imgur.com/FasrbKV.png" width="256">
     <!-- Margin not working for some reasons! Tried to fix it by searching but not working! Feel free to submit a patch! -->
     &nbsp;
     <!-- Old link: https://femto.pw/rasu.gif -->
     <img src="https://raw.githubusercontent.com/EKA2L1/eka2l1.github.io/main/assets/main/logo.gif">
  </p>

  <p align="center">
    <a href="https://github.com/EKA2L1/EKA2L1/actions?query=branch%3Amaster"><img src="https://github.com/eka2l1/eka2l1/workflows/C/C++%20CI/badge.svg"></a>
    <a href="https://crowdin.com/project/eka2l1"><img src="https://badges.crowdin.net/eka2l1/localized.svg"></a>
  </p>

  <h3 align="center">Symbian OS/N-Gage emulator, written in C++ 17.</h3>
</div>

---

The emulator *emulates* Symbian OS/N-Gage's kernel, and *reimplement* most of its critical app servers and libraries. 

### Download Builds/Artifacts:

- Builds/Artifacts for Windows, OSX, Linux and Android are provided through Github Actions. Click on the [***Releases***](https://github.com/EKA2L1/EKA2L1/releases/tag/continous) section to get the newest stable build.

    - **Note:** There's no official maintainer for OSX and Linux versions of the emulator. Please report to the developers through issues if versions for these OSes are not working.

### Compatibility:
- At the moment the emulator supports:
    - Almost all official N-Gage/N-Gage 2.0 official libraries
    - Most of Symbian's game libraries from S60v1 to Symbian Belle
    - A limited subsets of Symbian applications.

- Compatibility for the games and software that can (and can't) run on the emulator can be verified [**here**](https://github.com/EKA2L1/Compatibility-List)

### Links

For more information, discussion and support, please visit these links:

- [**Homepage**](https://12l1.com/)
- [**Emulator Wiki**](https://eka2l1.miraheze.org/wiki/Main_Page)
- [**Discord server**](https://discord.gg/5Bm5SJ9)

### Screenshots

Calculator (5530)                                               |  High Seize                                                   |          Dirk Dagger
:--------------------------------------------------------------:|:-------------------------------------------------------------:|:-----------------------------------------------:
![calculator](screenshots/0.0.8/screenshot_008_calculator.jpg)  | ![highseize](screenshots/0.0.8/screenshot_008_highseize.jpg)  | ![dirkdagger1](screenshots/0.0.8/screenshot_008_dirkdagger1.jpg)

The Big Roll in Paradise                                 | Mega Monster       
:-------------------------------------------------------:|:-----------------------------------------------------------------:
![BigRoll](screenshots/0.0.8/screenshot_008_bigroll.jpg) | ![MegaMonster](screenshots/0.0.8/screenshot_008_megamonster.jpg)

### Donations

From 2022, developing the emulator has shifted to become a part-time hobby and sometimes not actively maintained in months, since the compatibility for most popular games and Symbian operating systems have been satisfied.

Still, if you feel like our work has benefited you much and you want to support or give us some cheers, feel free to donate to two developers that maintain the PC/Android version by visiting the **Sponsor this project** section of the Github page.

Visit this [link](https://12l1.com/quickstart/donation/) for more information.

  -------------
 *GIFs are provided by [**Stranno**](https://www.youtube.com/user/9esferas1)!*
 
 *Logo is designed and drawn by dmolina007 and Frenesi!*
