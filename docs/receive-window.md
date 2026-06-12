# 接收窗口设计说明

本文档记录主界面“接收窗口”的实现逻辑。相关代码位于 `src/ui/widget.cpp` 和 `src/ui/widget.h`。

## 设计目标

接收窗口需要同时覆盖两类典型场景：

* 慢速设备：例如单片机固件升级时每秒只打印 1 个字符，界面需要近实时显示。
* 高频设备：例如 IMU 持续输出数据，界面不能因为频繁插入文本而卡顿。

因此当前实现采用“有限行缓存 + 低延迟批量刷新”的策略，而不是每收到 1 个字节就立刻刷新 `QPlainTextEdit`。

## 数据流

串口数据进入 UI 的路径如下：

1. `readMonitorData()` 从监视串口读取原始数据。
2. `appendRawData()` 接收来自串口监视、文件发送、文件接收流程的原始数据。
3. `appendToRxLineCache()` 将数据写入接收缓存。
4. 未暂停时，数据先进入 `pendingRxRender` 待渲染队列。
5. `flushRxRender()` 按定时器或阈值批量调用 `renderRawData()` 追加到 `rxLog`。

文件发送/接收流程中的协议原始数据也会通过 `rawDataReceived` 信号进入同一套显示逻辑。

## 行缓存策略

接收历史按行缓存：

* `rxLines`：已经完成的行。
* `currentRxLine`：当前尚未遇到换行符的行。
* `MaxRxLogLines = 500`：最多保留 500 行历史。
* `MaxRxLineBytes = 16 * 1024`：单行最多 16 KiB。

当收到 `\n` 时，`currentRxLine` 会转入 `rxLines`。如果行数超过 500，最旧的行会被丢弃。

如果设备长期不输出换行，`currentRxLine` 也不会无限增长。超过 16 KiB 后会强制切成一行并继续接收后续数据。

界面层同时调用：

```cpp
rxLog->document()->setMaximumBlockCount(MaxRxLogLines);
```

这样 `QPlainTextEdit` 自身也会限制最多显示约 500 个 block，避免文档对象无限增长。

## 批量刷新策略

接收显示使用短周期批量刷新：

* `RxRenderIntervalMs = 30`：普通情况下最多等待 30 ms 刷新一次。
* `MaxPendingRxRenderBytes = 8 * 1024`：待渲染数据超过 8 KiB 时立即刷新。

这保证了：

* 每秒 1 个字符的慢速输出，最多约 30 ms 延迟，肉眼基本是实时显示。
* 高频输出时，多次串口数据会合并成一批文本插入，避免 UI 线程被大量 `insertText()` 调用拖慢。

`renderRawData()` 会先把一批 `QByteArray` 转换成一个 `QString`，再一次性插入 `QPlainTextEdit`。这比逐字节调用 `QTextCursor::insertText()` 更适合高频串口日志。

接收窗口禁用了撤销栈：

```cpp
rxLog->setUndoRedoEnabled(false);
```

这可以减少高频追加文本时的内存和文档维护成本。

## 暂停与继续

“暂停”按钮只暂停界面刷新，不暂停串口读取：

* 暂停时：
  * `rxPaused = true`
  * 停止 `rxRenderTimer`
  * 清空 `pendingRxRender`
  * 后续数据仍继续进入 `rxLines/currentRxLine`
* 继续时：
  * `rxPaused = false`
  * 调用 `renderRxCache()` 重绘最新 500 行缓存

这样暂停期间不会阻塞串口，也不会让 UI 在后台继续做无意义的渲染。

## 清空行为

“清空”按钮会清理：

* `rxLines`
* `currentRxLine`
* `pendingRxRender`
* `rxLog`

并停止刷新定时器。清空后接收窗口从空状态继续接收新数据。

## 十六进制显示

“十六进制显示”切换后会调用 `refreshRxLog()`，使用当前缓存重新渲染接收窗口。

普通文本模式中：

* `\n` 显示为换行。
* `\r` 被忽略。
* 可打印 ASCII 直接显示。
* 其他字节显示为 `<XX>`。

十六进制模式中：

* 每个字节显示为两位大写十六进制和一个空格。
* 遇到 `\n` 时额外换行，便于观察文本协议输出。

## 可调参数

以下参数集中定义在 `src/ui/widget.cpp` 顶部：

```cpp
const int MaxRxLogLines = 500;
const int MaxRxLineBytes = 16 * 1024;
const int MaxPendingRxRenderBytes = 8 * 1024;
const int RxRenderIntervalMs = 30;
```

调参建议：

* 如果希望保留更多历史，提高 `MaxRxLogLines`。
* 如果设备会输出很长的无换行二进制流，提高 `MaxRxLineBytes`，但不建议无限制。
* 如果高速数据仍然卡顿，降低 `MaxPendingRxRenderBytes` 不一定有效，优先适当增大 `RxRenderIntervalMs`。
* 如果慢速字符显示延迟感明显，可以降低 `RxRenderIntervalMs`，例如 16 ms。
