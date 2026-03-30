# Moteus 固件编译环境离线配置指南

本指南旨在帮助用户在网络受限的情况下，手动下载必要工具并配置 Moteus 固件的编译环境。

## 1. 准备工作

### 1.1 系统要求
*   **操作系统**: Windows 10/11 + WSL2 (推荐 Ubuntu 20.04 或 22.04)
*   **基础工具**: `git`, `python3`, `usbipd-win` (用于 Windows 端 USB 穿透)

### 1.2 手动下载工具清单
请在 Windows 浏览器中下载以下文件，并准备传输到 WSL 环境中（例如放在 `\\wsl.localhost\Ubuntu\home\username\Downloads`）。

| 工具名称 | 文件名 | 下载地址 | 说明 |
| :--- | :--- | :--- | :--- |
| **LLVM 10.0.0** | `clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz` | [点击下载](https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz) | 交叉编译核心工具链 |
| **Bazel 7.4.1** | `bazel-7.4.1-linux-x86_64` | [点击下载](https://github.com/bazelbuild/bazel/releases/download/7.4.1/bazel-7.4.1-linux-x86_64) | 构建系统二进制文件 |
| **mbed-os 5.13.4** | `mbed-os-5.13.4.tar.gz` | [点击下载](https://github.com/ARMmbed/mbed-os/archive/mbed-os-5.13.4.tar.gz) | 嵌入式操作系统内核 |
| **ARM GCC** | `arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz` | [点击下载](https://armkeil.blob.core.windows.net/developer/files/downloads/gnu/15.2.rel1/binrel/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi.tar.xz) | ARM 裸机编译器 |

---

## 2. 环境配置步骤

### 2.1 克隆代码仓库
在 WSL 终端中执行：
```bash
git clone --recursive https://github.com/mjbots/moteus.git
cd moteus
```

### 2.2 配置 Bazel (离线模式)
为了避免脚本自动下载 Bazel 失败，我们需要手动放置二进制文件。

1.  **创建缓存目录**:
    ```bash
    mkdir -p ~/.cache/bazel/7.4.1
    ```
2.  **放置文件**:
    将下载的 `bazel-7.4.1-linux-x86_64` 复制到上述目录。
3.  **赋予执行权限**:
    ```bash
    chmod +x ~/.cache/bazel/7.4.1/bazel-7.4.1-linux-x86_64
    ```
4.  **修改 `tools/bazel` 脚本** (可选，如果脚本尝试下载):
    确保 `tools/bazel` 能够找到该缓存文件。通常默认脚本会检查该路径。

### 2.3 配置依赖包 (Distdir)
Bazel 允许通过 `distdir` 机制使用本地下载的压缩包，从而避免编译时下载。

1.  **创建 distdir 目录**:
    在 `moteus` 工程根目录下创建：
    ```bash
    mkdir -p distdir
    ```
2.  **放置文件并重命名**:
    将下载的 `mbed-os` 和 `ARM GCC` 压缩包放入 `distdir` 目录。
    *   **重要**: 确保 `mbed-os` 的文件名为 `mbed-os-5.13.4.tar.gz` (如果下载下来是 `mbed-os-mbed-os-5.13.4.tar.gz`，请重命名)。
    *   `arm-gnu-toolchain` 文件名保持原样即可。

### 2.4 配置 LLVM 工具链
由于 LLVM 工具链配置较为特殊，建议通过修改 `WORKSPACE` 文件指定本地路径。

1.  **准备目录**:
    ```bash
    mkdir -p external
    ```
2.  **放置文件**:
    将 `clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz` 放入 `external` 目录。

3.  **计算 SHA256**:
    ```bash
    sha256sum external/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz
    # 记录输出的哈希值，例如: b25f592a0c00686f03e3b7db68ca6dc87418f681f4ead4df4745a01d9be63843
    ```

4.  **修改 `WORKSPACE` 文件**:
    使用编辑器打开 `WORKSPACE`，找到 `llvm_toolchain` 定义部分（约第40行），修改 `linux` 下的 `urls` 和 `sha256`：
    ```python
    llvm_toolchain(
        name = "llvm_toolchain",
        llvm_version = "10.0.0",
        urls = {
            "windows" : ["https://github.com/mjbots/bazel-toolchain/releases/download/0.5.6-mj20201011/LLVM-10.0.0-win64.tar.xz"],
            # 修改下面这行，指向本地文件路径 (请替换 /home/zhangfeng 为你的实际主目录)
            "linux": ["file:///home/zhangfeng/moteus/external/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz"],
        },
        sha256 = {
            "windows" : "2851441d3993c032f98124a05e2aeb43010b7a85f0f7441103d36ae8d00afc18",
            # 修改下面这行，填入你计算出的 SHA256
            "linux": "b25f592a0c00686f03e3b7db68ca6dc87418f681f4ead4df4745a01d9be63843",
        },
        strip_prefix = {
            "windows" : "LLVM",
            "linux": "clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04",
        }
    )
    ```

---

## 3. 编译固件

完成上述配置后，在 `moteus` 根目录下执行以下命令：

```bash
# --distdir 参数告诉 Bazel 优先在本地 distdir 目录查找依赖包
tools/bazel build --config=target //:target --distdir=$(pwd)/distdir
```

**编译成功标志**:
看到类似 `Build completed successfully, 439 actions` 的提示。

**产物位置**:
*   `bazel-bin/fw/moteus.bin`
*   `bazel-bin/fw/moteus.elf`

---

## 4. 烧录固件 (推荐 Windows 方式)

由于 WSL 配置 USB 穿透较为复杂，推荐将编译好的固件复制到 Windows 下进行烧录。

1.  **复制固件**:
    ```bash
    # 假设复制到 C 盘根目录
    cp bazel-bin/fw/moteus.elf /mnt/c/moteus.elf
    cp bazel-bin/fw/moteus.bin /mnt/c/moteus.bin
    ```
2.  **烧录工具**:
    *   使用 **STM32CubeProgrammer** 连接 ST-Link 进行烧录。
    *   或者使用 **moteus_tool** (Python) 通过 fdcanusb 进行烧录。
