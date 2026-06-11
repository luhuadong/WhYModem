# Windows 构建与打包

本文档说明如何在 Windows 上构建 WhYModem，并使用 `windeployqt` 将可执行文件与依赖库打包发布。

## 环境要求

* Windows 10 或更高版本（64 位）
* Qt 5.15 LTS 或 Qt 6.x，需安装 **Qt SerialPort** 组件
* CMake 3.16+
* 编译工具链：**MinGW** 或 **MSVC** 二选一
* 推荐 IDE：Qt Creator

当前仓库已在以下环境验证打包流程：

* Qt 6.11.1 MinGW 64-bit
* Qt Creator 19.0.2
* Windows 10/11

## 使用 Qt Creator 构建

1. 安装 Qt 时勾选 **Qt SerialPort**。
2. 用 Qt Creator 打开项目根目录下的 `CMakeLists.txt`。
3. 选择 Kit，例如 `Desktop Qt 6.11.1 MinGW 64-bit` 或对应的 MSVC Kit。
4. 将构建配置切换为 **Release**。
5. 点击构建并运行，确认程序可正常启动。

Qt Creator 默认会把构建产物放到：

```text
build\Desktop_Qt_<版本>_<工具链>-Release\WhYModem.exe
```

例如 MinGW 环境：

```text
build\Desktop_Qt_6_11_1_MinGW_64_bit-Release\WhYModem.exe
```

## 使用命令行 CMake 构建

### MinGW 示例

在 Qt 附带的 MinGW 终端，或已将 Qt / MinGW 加入 `PATH` 的终端中执行：

```bat
cmake -S . -B build\mingw-release -G "MinGW Makefiles" ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64 ^
  -DCMAKE_BUILD_TYPE=Release
cmake --build build\mingw-release --parallel
```

### MSVC 示例

```bat
cmake -S . -B build\msvc-release -G Ninja ^
  -DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\msvc2019_64
cmake --build build\msvc-release --config Release
```

构建完成后，确认 `WhYModem.exe` 已生成。

## 打包原理

Windows 下发布 Qt 程序，通常采用**绿色包**方式：

1. 复制 Release 版 `WhYModem.exe`
2. 使用 Qt 自带工具 `windeployqt` 收集 Qt DLL 和插件
3. 视工具链补充编译器运行时
4. 打成 ZIP 分发给用户

WhYModem 依赖 **Qt Widgets** 和 **Qt SerialPort**，因此 `windeployqt` 需要带 `--serialport` 参数。

## 一键打包脚本

项目提供了两个入口，效果相同：

| 文件 | 说明 |
|------|------|
| `packaging/deploy-windows.ps1` | 主脚本，PowerShell 执行 |
| `packaging/deploy-windows.bat` | 批处理包装，双击或命令行均可调用 |

### 基本用法

在项目根目录执行：

```powershell
.\packaging\deploy-windows.ps1
```

或：

```bat
packaging\deploy-windows.bat
```

脚本会：

1. 自动读取 `CMakeLists.txt` 中的版本号（当前为 `0.1.0`）
2. 在 `build/` 下查找最新的 Release 版 `WhYModem.exe`
3. 自动定位对应工具链的 `windeployqt.exe`
4. 将部署结果输出到 `dist/WhYModem-<version>-win64-<toolchain>/`
5. 执行一次启动冒烟测试
6. 默认生成同名 ZIP 压缩包

### 常用参数

```powershell
# 指定 Release 构建目录
.\packaging\deploy-windows.ps1 -BuildDir ".\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release"

# 指定 Qt bin 目录
.\packaging\deploy-windows.ps1 -QtBin "C:\Qt\6.11.1\mingw_64\bin"

# 只部署目录，不生成 ZIP
.\packaging\deploy-windows.ps1 -NoZip

# 显式生成 ZIP
.\packaging\deploy-windows.ps1 -Zip
```

也可通过环境变量指定 Qt：

```powershell
$env:QT_BIN = "C:\Qt\6.11.1\mingw_64\bin"
.\packaging\deploy-windows.ps1
```

### 输出结果

打包成功后，可在 `dist/` 目录看到类似文件：

```text
dist/
  WhYModem-0.1.0-win64-mingw/
    WhYModem.exe
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Widgets.dll
    Qt6SerialPort.dll
    platforms/qwindows.dll
    ...
  WhYModem-0.1.0-win64-mingw.zip
```

将整个文件夹或 ZIP 发给用户即可，**无需安装 Qt**。

## 编译器运行时说明

### MinGW 版

`windeployqt` 通常会复制以下运行时 DLL：

* `libgcc_s_seh-1.dll`
* `libstdc++-6.dll`
* `libwinpthread-1.dll`

脚本会在 `C:\Qt\Tools\mingw*` 下尝试自动补齐这些文件。

### MSVC 版

MSVC 构建的程序，目标机器通常还需要安装对应版本的 [Visual C++ Redistributable](https://learn.microsoft.com/zh-cn/cpp/windows/latest-supported-vc-redist)。

建议在发布说明中注明所需运行库版本，或将运行库安装包一并提供。

## 发布前检查清单

1. 使用 **Release** 构建，不要发布 Debug 版本。
2. 架构保持一致：64 位构建只发给 64 位 Windows 用户。
3. 工具链保持一致：MinGW 版与 MSVC 版应分别打包，不可混用 DLL。
4. 在未安装 Qt 的目录解压后试运行，例如 `C:\Temp\`。
5. 确认主窗口能正常弹出，串口列表可扫描。
6. 打包前关闭正在运行的 `WhYModem.exe`，否则 ZIP 可能因文件占用而失败。

## 串口与驱动

WhYModem 通过 `QSerialPort` 访问串口。USB 转串口芯片（如 CH340、CP2102、FTDI）需要用户自行安装对应驱动，这与 Qt 打包无关。

## 常见问题

### 提示找不到 Release 版 exe

先确认已在 Qt Creator 中完成 **Release** 构建，或检查 `build/` 下是否存在 `WhYModem.exe`。

也可手动指定构建目录：

```powershell
.\packaging\deploy-windows.ps1 -BuildDir ".\build\Desktop_Qt_6_11_1_MinGW_64_bit-Release"
```

### 提示找不到 windeployqt

手动指定 Qt 的 `bin` 目录：

```powershell
.\packaging\deploy-windows.ps1 -QtBin "C:\Qt\6.11.1\mingw_64\bin"
```

### 双击 exe 提示缺少 `Qt6Core.dll` 或 `qwindows.dll`

说明依赖未收集完整。请重新执行部署脚本，不要只复制单个 exe。

### `dxcompiler.dll` 警告

`windeployqt` 可能提示找不到 `dxcompiler.dll`。对 WhYModem 这类普通 Widgets 程序通常可以忽略。

### ZIP 打包失败，提示文件被占用

先关闭所有 `WhYModem.exe` 实例，再重新执行脚本。

## 推荐发布流程

```text
Qt Creator Release 构建
        ↓
packaging\deploy-windows.bat
        ↓
在 dist/ 中检查部署目录
        ↓
在未安装 Qt 的机器上试跑
        ↓
发布 WhYModem-<version>-win64-<toolchain>.zip
```

## 相关文件

* `packaging/deploy-windows.ps1`：Windows 部署主脚本
* `packaging/deploy-windows.bat`：批处理入口
* `CMakeLists.txt`：项目版本号与构建配置
* `README.md`：项目总览与 Linux 打包说明
