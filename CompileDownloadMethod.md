### 一、 编译方式 (Compilation)

Moteus 固件使用的是 **Bazel** 构建系统进行管理，并且需要交叉编译为 STM32G4 平台可执行的二进制文件。

1. **系统环境要求**
   官方推荐在 **x86-64 架构的 Ubuntu 20.04, 22.04, 或 24.04** 操作系统上进行编译。
2. **编译命令**
   在 moteus 源码根目录下，使用项目自带的 bazel 包装脚本进行编译。必须带有 `--config=target` 参数以应用交叉编译配置：
   ```bash
   ./tools/bazel build --config=target //fw:moteus
   ```
   *(编译成功后，生成的固件一般位于 `bazel-out/` 相关的目录下，如 `moteus.elf`)*

---

### 二、 下载/烧录方式 (Flashing)

Moteus 支持两种不同的固件下载方式，分别应对不同的使用场景：

#### 方式一：通过 CAN-FD 接口进行 OTA 升级（常规推荐）
这是最常用的升级方式。只要 Moteus 板载了有效的 Bootloader 且能正常在 CAN 总线上进行通信，就可以通过此方式刷写。

* **使用的工具**：Moteus 官方的 Python 工具 `moteus_tool`。
* **烧录命令**：
  ```bash
  python3 -m moteus.moteus_tool --target 1 --flash path/to/moteus.elf
  ```
  *(注：`--target 1` 为目标电机的 CAN ID，`path/to/moteus.elf` 替换为你编译出或下载的固件路径。切记不要选择名字里带 `bootloader` 的文件进行 CAN 升级。)*

#### 方式二：通过 SWD 硬件调试接口底层烧录（适用于救砖或全新空板）
如果板子是完全空白的，或者由于固件错误导致 CAN 通信失效（即“变砖”），就需要使用硬件调试接口（SWD）强刷底层固件和 Bootloader。

* **硬件要求**：需要一个 STM32 烧录器（例如 ST-Link 或 mjbots 官方的 STM32 Programmer），并通过排线连接到 Moteus 的 SWD 触点/接口上。
* **软件依赖**：需要系统中安装 `openocd` 和 `binutils-arm-none-eabi`。
  ```bash
  sudo apt install openocd binutils-arm-none-eabi
  ```
* **烧录命令**：
  - **选项 A：一键编译并烧录** (通过 Bazel 和 OpenOCD 自动完成)：
    ```bash
    ./tools/bazel build --config=target //fw:flash
    ```
  - **选项 B：通过 Python 脚本烧录** (如果你已经编译好了，可以直接调用 flash 脚本)：
    ```bash
    ./fw/flash.py
    ```
  - **选项 C：指定预编译好的固件进行烧录**：
    ```bash
    ./fw/flash.py path/to/firmware.elf path/to/bootloader.elf
    ```