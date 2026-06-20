# Change log

## V1.0.0

- 新增设置窗口，支持配置 YMODEM 首包延迟和包间延迟
- 新增关于窗口和版本信息窗口，展示 Change Log、License 和第三方开源软件清单
- 支持手动输入串口设备路径，便于使用 `socat` 虚拟串口测试
- 增强 XMODEM 收发兼容性，支持自定义保存文件名和尾包 `0x1A` padding 裁剪
- 修复 YMODEM 与 `lrzsz` 对传时的接收结束块和发送收尾兼容问题
- 改进 ZMODEM 完成状态、进度显示、取消/超时处理和会话收尾行为
- 新增 `WhYModemCli` 和 `lrzsz` 矩阵测试脚本，覆盖 XMODEM/YMODEM/ZMODEM 收发回归
- 补充 `socat`/`lrzsz` 模拟测试和协议实现说明文档

## V0.2.0

- 重构协议/传输层架构，引入 `ITransferProtocol` 和 `ProtocolFactory` 分层架构
- 新增 Xmodem 协议状态机，支持 CRC/checksum 握手、重复包确认、连续 CAN 中止和 XMODEM-1K 数据包
- 集成 qzmodem 源码，实现可用的 Zmodem 收发适配层
- 文件收发层改为组合协议对象，不再继承 Ymodem
- 调整页面布局，将配置功能和操作功能分开
- 增加版本信息显示


## V0.1.0

- 支持 Ymodem 协议传输文件
- 支持串口参数配置
- 支持数据接收显示
