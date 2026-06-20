# 软件实现说明与使用注意事项

本文档记录 WhYModem 的关键代码实现细节、协议兼容性取舍和测试注意事项，便于后续维护、排查传输问题和使用 `socat`/`lrzsz` 做模拟测试。

## 1. 总体数据流

WhYModem 将界面、传输适配和协议状态机分层实现：

```text
Widget
  -> FileTransmitter / FileReceiver
    -> ITransferProtocol
      -> XmodemProtocol / YmodemProtocol / ZmodemProtocol
    -> QFile
    -> QSerialPort
```

- `Widget` 负责 UI 状态、串口选择、文件选择、进度显示和接收窗口显示。
- `FileTransmitter` 负责发送文件、打开串口、响应协议状态机的数据请求。
- `FileReceiver` 负责接收数据、创建保存文件、写入文件和上报接收状态。
- `ITransferProtocol` 是统一协议接口，屏蔽 XMODEM/YMODEM/ZMODEM 的状态机差异。
- 协议层只通过回调读写串口和读写文件，不直接依赖 UI。

## 2. 传输开始前的状态复位

每次点击“发送”或“接收”开始新一轮传输时，`FileTransmitter` 和 `FileReceiver` 都会先停止读写定时器、关闭串口、关闭文件句柄，并重新创建协议对象。这样可以确保上一轮取消、超时或失败后不会留下协议阶段、文件读写位置、串口缓存或定时器状态，避免出现第一次失败后第二次“假成功”或复用 EOF 文件位置的问题。

超时和中止路径也会关闭对应文件句柄，确保下一轮传输可以重新从文件开头开始。

## 3. 串口与虚拟串口

串口列表来自 `QSerialPortInfo::availablePorts()`。Linux 下该接口通常能发现 `/dev/ttyUSB*`、`/dev/ttyACM*`、`/dev/ttyS*` 等真实串口，但不一定枚举 `socat` 创建的 `/dev/pts/*` 伪终端。

因此 WhYModem 的端口下拉框允许手动输入。使用虚拟串口测试时，推荐创建固定名字：

```bash
socat -d -d \
  pty,raw,echo=0,link=/tmp/whymodem_gui \
  pty,raw,echo=0,link=/tmp/whymodem_cli
```

WhYModem 端口输入：

```text
/tmp/whymodem_gui
```

命令行工具使用：

```text
/tmp/whymodem_cli
```

`refreshSerialPorts()` 会保留用户手动输入的端口路径，避免刷新列表后丢失 `/tmp/whymodem_gui`。

## 4. 接收窗口与传输串口

接收窗口显示的是串口收到的数据副本，不会与文件传输线程竞争读取同一份数据。

传输开始前，`Widget` 会关闭用于普通串口监视的 `serialPort`，然后由 `FileTransmitter` 或 `FileReceiver` 打开同一个端口执行传输。传输过程中的原始数据通过 `rawDataReceived` 信号送回 UI 显示。

因此：

- 接收窗口打开不会导致两个线程同时读取串口。
- 接收窗口中的 `<15>`、`<1A>` 等内容只是协议字节或文件内容的显示结果。
- 文本模式下不可打印字节会显示为 `<XX>`；十六进制模式下会显示原始字节值。

## 5. XMODEM 实现细节

### 5.1 协议限制

XMODEM 是最简单的协议，不传文件名，也不传真实文件大小。接收端只能按数据块接收：

- classic XMODEM 使用 128 字节数据块，包头为 `SOH`。
- XMODEM-1K 使用 1024 字节数据块，包头为 `STX`。
- 传输结束由发送端发送 `EOT`，接收端返回 `ACK`。

由于没有文件大小字段，最后一包不足整包时，发送端通常使用 `0x1A` 填充到整包长度。

### 5.2 接收端尾包处理

WhYModem 的 XMODEM 接收端采用“延迟写最后一包”的策略：

1. 收到一个 XMODEM 数据包后，先不立即写入当前包。
2. 如果之前已有暂存包，则先把上一包写入文件。
3. 将当前包保存为 `pendingXmodemBlock`。
4. 收到 `EOT` 并确认传输完成时，对 `pendingXmodemBlock` 从尾部裁掉连续的 `0x1A` padding。
5. 将裁剪后的最后一包写入文件并关闭文件。

这样可以避免 `sx` 等发送端在最后一包补齐的 `0x1A` 被写入最终文件，导致接收文件比原文件更大。

需要注意：这是 XMODEM 场景下的兼容性取舍。如果原始二进制文件本身确实以一个或多个 `0x1A` 字节结尾，标准 XMODEM 无法区分这些字节是文件内容还是 padding。对于必须严格保留尾部 `0x1A` 的文件，建议使用 YMODEM 或 ZMODEM。

### 5.3 接收文件名和保存路径

XMODEM 不携带文件名。WhYModem 的“保存路径”输入框在 XMODEM 下优先表示完整输出文件路径；用户可以通过浏览按钮选择或输入例如 `/home/user/Downloads/test.log`、`C:\\Users\\user\\Downloads\\firmware.bin` 这样的保存文件名。

如果保存路径指向一个已存在目录，或没有指定完整文件名，WhYModem 接收 XMODEM 时会自动生成文件名：

```text
xmodem-yyyyMMdd-hhmmss.bin
```

`.bin` 后缀只是默认保存名，不会改变文件内容。如果接收的是日志文本，可以在 XMODEM 接收前直接把保存路径指定为 `.log` 文件，或接收后手动改成 `.log` 后缀再查看。

YMODEM 和 ZMODEM 会携带文件名，因此“保存路径”在这两个协议下表示保存目录。路径处理使用 Qt 的 `QDir` 和 `QFileInfo`，避免手写路径分隔符，以保持 Linux 和 Windows 兼容。

### 5.4 发送端 EOT 收尾

XMODEM 发送完最后一个数据包并收到 ACK 后，发送端会发送 `EOT` 表示传输结束。WhYModem 进入收尾阶段后不会以 10 ms 周期连续刷 `EOT`；它会等待接收端 `ACK`，只有收到 `NAK` 或等待达到较长周期时才重发 `EOT`。这样可以避免 `rx` 等接收端在处理结束阶段时把重复的 `0x04` 误当成新的 sector header。

### 5.5 发送端分包节奏

XMODEM 使用 128 字节小包时，不能套用 YMODEM 为部分 Bootloader 预留的包间延迟。WhYModem 的发送端只对 YMODEM 数据包执行 `firstDataDelayMs` 和 `interPacketDelayMs` 延迟；XMODEM 数据包不做额外 sleep，避免 `rx` 因传输过慢进入超时重试状态。

### 5.6 与 lrzsz 对传

命令行发送，WhYModem 接收：

```bash
sx test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
```

接收后可以直接比较：

```bash
diff test.bin ~/Downloads/xmodem-*.bin
```

如果使用其他 XMODEM 接收工具且没有裁剪 padding，直接 `diff` 可能会因为尾部 `0x1A` 不同而失败。可以先比较原文件长度范围内的内容：

```bash
cmp -n "$(stat -c %s test.bin)" test.bin received_xmodem.bin
```

## 6. YMODEM 实现细节

YMODEM 在 block0 中携带文件名和文件大小，因此接收端可以按真实文件大小写入，最后一包的填充不会进入最终文件。

发送流程为：

1. 等待接收端发送 `C`。
2. 发送 block0，包含文件名和大小。
3. 等待 `ACK` 和下一次 `C`。
4. 发送数据包。
5. 发送 `EOT`。
6. 等待接收端 `ACK` 后继续等待 `C`。
7. 收到 `C` 后发送最终空 block。
8. 等待最终 `ACK`，标记传输完成。

这里需要特别注意第 6 步和第 7 步。`lrzsz rb` 的常见收尾顺序是：

```text
EOT -> ACK -> C -> empty block -> ACK
```

发送端不能在收到 `ACK` 时立即发送最终空 block，否则可能把随后到来的 `C` 留到下一阶段并误判为重传请求，最终导致 GUI 端等待超时。

接收端在收到 `EOT` 后会回复 `ACK` 和 `C`，随后等待发送端发出批量传输结束的空文件名 block。常见实现会用 `SOH` 发送 128 字节空 block，但 `lrzsz sb` 在部分场景下可能发送 `STX` 形式的 1024 字节空 block。WhYModem 接收端同时接受这两种结束 block，只要块号为 `0`、反码为 `0xFF`、文件名首字节为 `0x00` 且 CRC 正确，就回复 `ACK` 并标记接收完成。

## 7. ZMODEM 实现细节

ZMODEM 通过 `ZmodemProtocol` 适配第三方 qzmodem 实现。ZMODEM 支持文件名、文件大小、批量文件和更复杂的自动握手流程。

使用 `lrzsz` 测试时要注意启动顺序：

- WhYModem 发送，命令行接收：先让 WhYModem 准备发送，再运行 `rz`。
- 命令行发送，WhYModem 接收：先让 WhYModem 进入接收，再运行 `sz file`。

ZMODEM 可能主动发送启动序列，如果两端启动顺序不对，容易出现一边已经发出握手而另一边还没开始读串口的情况。

## 8. lrzsz 测试命令

安装工具：

```bash
sudo apt install lrzsz socat
```

常用命令：

| 协议 | 命令行发送 | 命令行接收 |
| ---- | ---------- | ---------- |
| XMODEM | `sx file` | `rx received.bin` |
| YMODEM | `sb file` | `rb` |
| ZMODEM | `sz file` | `rz` |

串口重定向必须同时连接标准输入和标准输出：

```bash
sx test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
rx received.bin < /tmp/whymodem_cli > /tmp/whymodem_cli

sb test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
rb < /tmp/whymodem_cli > /tmp/whymodem_cli

sz test.bin < /tmp/whymodem_cli > /tmp/whymodem_cli
rz < /tmp/whymodem_cli > /tmp/whymodem_cli
```

不要写成：

```bash
rz /tmp/whymodem_cli
```

`lrzsz` 这类工具通过标准输入/输出交互，串口设备需要用 shell 重定向连接。

## 9. 文件对比建议

YMODEM 和 ZMODEM 会携带文件大小，成功接收后通常可以直接使用：

```bash
diff source.bin received.bin
sha256sum source.bin received.bin
```

XMODEM 不携带文件大小。WhYModem 接收端会裁剪尾部 `0x1A` padding，因此常规文件通常也可以直接 `diff`。如果对端接收工具不会裁剪 padding，则需要按源文件长度比较：

```bash
cmp -n "$(stat -c %s source.bin)" source.bin received.bin
```

若文件必须保留尾部真实的 `0x1A` 字节，请使用 YMODEM 或 ZMODEM 进行验证。
