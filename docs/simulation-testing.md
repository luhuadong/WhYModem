# 模拟测试

本项目可以在纯 Linux 上闭环测试，不需要单片机。关键思路是用 `socat` 创建一对虚拟串口，再用 `lrzsz` 的 `sx/sb/sz/rx/rb/rz` 跟 WhYModem 对传。

协议实现细节、XMODEM 尾包 padding 处理和文件对比注意事项见 [软件实现说明与使用注意事项](implementation-notes.md)。



## 测试方案

有一套很经典的 Linux 命令行工具可以配合 WhYModem 测试：

```bash
sudo apt install lrzsz socat
```

其中：

|  协议  | 发送命令 | 接收命令 |
| :----: | :------: | :------: |
| XMODEM |   `sx`   |   `rx`   |
| YMODEM |   `sb`   |   `rb`   |
| ZMODEM |   `sz`   |   `rz`   |

`lrzsz` 提供的 `sz/sb/sx` 和 `rz/rb/rx` 就是 Linux 下常用的 XMODEM/YMODEM/ZMODEM 命令行工具。`sz` 的 man page 明确说明它可以使用 ZMODEM、YMODEM 或 XMODEM 协议发送文件。



## 自动化 CLI 回归

仓库提供 `WhYModemCli` 和 `tools/run_lrzsz_matrix.sh`，用于不启动 GUI 的协议回归测试。CLI 复用 WhYModem 的 `FileTransmitter`、`FileReceiver` 和协议状态机，因此测试结果能直接反映核心传输代码。

构建：

```bash
cmake -S . -B build
cmake --build build --target WhYModemCli
```

运行完整矩阵：

```bash
tools/run_lrzsz_matrix.sh --protocols xmodem,ymodem,zmodem --keep-work

# 包含负向测试：例如 ZMODEM 发送时对端 rz 未启动，必须失败而不是误报成功
tools/run_lrzsz_matrix.sh --protocols zmodem --include-negative --keep-work
```

脚本会为每个正常 case 重启一对 `socat` 虚拟串口，并测试 6 个方向：

```text
WhYModemCli send XMODEM -> rx
sx -> WhYModemCli receive XMODEM
WhYModemCli send YMODEM -> rb
sb -> WhYModemCli receive YMODEM
WhYModemCli send ZMODEM -> rz
sz -> WhYModemCli receive ZMODEM
```

失败时保留的工作目录里包含 `cli.log`、`*.raw` 和 lrzsz 日志，可用于分析控制字节和状态机阶段。

也可以直接使用 CLI 手动测试：

```bash
build/WhYModemCli send --protocol ymodem --port /tmp/whymodem_gui --file firmware.bin
build/WhYModemCli receive --protocol ymodem --port /tmp/whymodem_gui --output /tmp/recv
```



## 虚拟串口

用 `socat` 创建一对互通的虚拟串口：

```bash
socat -d -d pty,raw,echo=0 pty,raw,echo=0
```

它会输出类似：

```bash
N PTY is /dev/pts/5
N PTY is /dev/pts/6
N starting data transfer loop with FDs ...
```

这两个 `/dev/pts/5` 和 `/dev/pts/6` 就像一根“虚拟串口线”的两端。`socat` 创建虚拟串口对是 Linux 下常见的串口程序测试方法。

你可以这样连接：

```bash
WhYModem 打开：/dev/pts/5
命令行工具使用：/dev/pts/6
```

注意：`socat` 这个终端窗口不要关，关了虚拟串口就消失了。

切换 XMODEM/YMODEM/ZMODEM 协议、取消传输或遇到失败后，如果下一次测试表现随机，优先关闭 WhYModem 串口并重启这一对虚拟串口。`socat`/pty 可能保留上一轮已经送到另一端队列中的握手或取消字节，WhYModem 只能清理自己当前打开端的缓冲。

另外，`socat` 命令可以稍微增强为：

```bash
socat -d -d \
  pty,raw,echo=0,link=/tmp/whymodem_gui,wait-slave \
  pty,raw,echo=0,link=/tmp/whymodem_cli,wait-slave
```

`wait-slave` 不是必须，但能减少一端还没打开时另一端启动太快造成的偶发现象。不使用 `wait-slave` 时也可以固定端口名：

```bash
socat -d -d pty,raw,echo=0,link=/tmp/whymodem_gui pty,raw,echo=0,link=/tmp/whymodem_cli
```



## 测试方法

### 1. 测试 WhYModem 发送，Linux 命令行接收

#### XMODEM 接收

终端执行：

```bash
rx received_xmodem.bin < /dev/pts/6 > /dev/pts/6
```

然后 WhYModem 选择 `/dev/pts/5`，协议选择 XMODEM，发送文件。

XMODEM 本身不传文件名，所以命令行这边要手动指定接收文件名：

```bash
received_xmodem.bin
```

#### YMODEM 接收

```bash
rb < /dev/pts/6 > /dev/pts/6
```

然后 WhYModem 用 YMODEM 发送文件。

YMODEM 支持传文件名、文件大小，所以接收端一般不用手动指定文件名。

#### ZMODEM 接收

```bash
rz < /dev/pts/6 > /dev/pts/6
```

然后 WhYModem 用 ZMODEM 发送文件。

ZMODEM 也支持文件名、批量文件、自动恢复等能力，通常体验最好。

### 2. 测试 Linux 命令行发送，WhYModem 接收

#### XMODEM 发送

```bash
sx test.bin < /dev/pts/6 > /dev/pts/6
```

WhYModem 打开 `/dev/pts/5`，协议选择 XMODEM，点击接收。

#### YMODEM 发送

```bash
sb test.bin < /dev/pts/6 > /dev/pts/6
```

WhYModem 选择 YMODEM 接收。

#### ZMODEM 发送

```bash
sz test.bin < /dev/pts/6 > /dev/pts/6
```

WhYModem 选择 ZMODEM 接收。

### 3. 推荐方式：创建固定名字的虚拟串口

`/dev/pts/5`、`/dev/pts/6` 每次可能变化，而且 Qt 的串口枚举不一定会显示 `/dev/pts/*`。推荐创建固定名字的软链接，然后在 WhYModem 的串口输入框中手动输入 GUI 端口：

```bash
socat -d -d \
  pty,raw,echo=0,link=/tmp/whymodem_gui \
  pty,raw,echo=0,link=/tmp/whymodem_cli
```

然后：

```bash
WhYModem 使用：/tmp/whymodem_gui
命令行使用：/tmp/whymodem_cli
```

WhYModem 的串口下拉框支持手动输入。如果设备列表里看不到 `/tmp/whymodem_gui`，直接在串口输入框中输入这个路径即可。

测试命令就可以固定写成：

```bash
rz < /tmp/whymodem_cli > /tmp/whymodem_cli
sz test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli

rb < /tmp/whymodem_cli > /tmp/whymodem_cli
sb test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli

rx received.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
sx test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
```

### 4. 建议你做一个测试脚本

可以在 WhYModem 仓库里增加：

```bash
tools/
  create_virtual_serial.sh
  test_send_xmodem.sh
  test_recv_xmodem.sh
  test_send_ymodem.sh
  test_recv_ymodem.sh
  test_send_zmodem.sh
  test_recv_zmodem.sh
```

例如 `create_virtual_serial.sh`：

```bash
#!/usr/bin/env bash
set -e

GUI_PORT="/tmp/whymodem_gui"
CLI_PORT="/tmp/whymodem_cli"

rm -f "$GUI_PORT" "$CLI_PORT"

echo "Creating virtual serial pair..."
echo "WhYModem port : $GUI_PORT"
echo "CLI test port : $CLI_PORT"
echo
echo "Keep this terminal open while testing."
echo

socat -d -d \
  pty,raw,echo=0,link="$GUI_PORT" \
  pty,raw,echo=0,link="$CLI_PORT"
```

`test_cli_recv_ymodem.sh`：

```bash
#!/usr/bin/env bash
set -e

PORT="${1:-/tmp/whymodem_cli}"

echo "Receiving file by YMODEM on $PORT ..."
rb < "$PORT" > "$PORT"
```

`test_cli_send_ymodem.sh`：

```bash
#!/usr/bin/env bash
set -e

PORT="${1:-/tmp/whymodem_cli}"
FILE="${2:-test.bin}"

if [ ! -f "$FILE" ]; then
    echo "File not found: $FILE"
    exit 1
fi

echo "Sending $FILE by YMODEM on $PORT ..."
sb "$FILE" < "$PORT" > "$PORT"
```



### 5. 注意事项

第一，**XMODEM 不传文件名**，所以接收端要指定输出文件名；YMODEM/ZMODEM 可以携带文件名。

第二，`rx/rb/rz`、`sx/sb/sz` 都是和标准输入/输出交互的，所以测试串口时要用：

```bash
< "$PORT" > "$PORT"
```

不要只写：

```bash
rz /tmp/whymodem_cli
```

这不是它们的典型用法。

第三，虚拟串口的波特率意义不大。`socat` 的 pty 对传输速度不会真实模拟 UART 波特率，所以它适合测试协议流程、收发状态机、文件完整性，不适合测试真实串口速率、丢包、电平、硬件流控等问题。

第四，切换协议前建议重新打开串口；如果刚刚发生过失败、取消或超时，建议直接重启 `socat`。这条对虚拟串口尤其重要，因为旧的 `C`、`NAK`、`CAN`、`EOT` 等控制字节可能已经排在命令行工具那一端，导致下一次测试看起来像协议状态机随机异常。真实串口一般也建议在切换协议前让两端回到空闲状态。

第五，`lrzsz` 输出的 `Retry 0: NAK on sector` 是可恢复重传提示，不一定是失败。判断结果时以最终输出和文件校验为准：如果显示 `Transfer complete`，并且 `diff` 或 `sha256sum` 一致，就说明传输成功；如果连续 NAK 后出现 `Cancelled` 或 `Transfer incomplete`，才需要排查。

第六，ZMODEM 有时会自动发送启动序列，GUI 端如果状态机处理不当，可能会出现“谁先开始”的同步问题。建议你的测试矩阵里明确区分：

```bash
GUI Send  -> CLI Receive
CLI Send  -> GUI Receive
```

不要两边都点发送，也不要两边都等接收但没有启动信号。

最推荐你先跑这一组：

```bash
# 终端 1
socat -d -d pty,raw,echo=0,link=/tmp/whymodem_gui pty,raw,echo=0,link=/tmp/whymodem_cli

# 终端 2：等待接收
rb < /tmp/whymodem_cli > /tmp/whymodem_cli
```

然后 WhYModem 打开 `/tmp/whymodem_gui`，用 **YMODEM 发送一个小文件**。

如果 YMODEM 打通，再测 XMODEM 和 ZMODEM。YMODEM 是最适合作为第一条闭环测试链路的：比 XMODEM 信息完整，又比 ZMODEM 状态机简单。

