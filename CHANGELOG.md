# Change log

## V0.2.0

- 重构协议/传输层架构，引入 `ITransferProtocol` 和 `ProtocolFactory`
- 新增 Xmodem 协议状态机，支持 CRC/checksum 握手、重复包确认、连续 CAN 中止和 XMODEM-1K 数据包
- 集成 qzmodem 源码，实现可用的 Zmodem 收发适配层
- 文件收发层改为组合协议对象，不再继承 Ymodem


## V0.1.0

- 支持 Ymodem 协议传输文件
- 支持串口参数配置
- 支持数据接收显示
