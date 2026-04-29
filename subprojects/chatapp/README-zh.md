# chatapp（socket + libhackode-dev 示例）

此示例演示如何在一个小型 TCP 聊天程序中使用 `libhackode-dev`。

- `chat_server.c`：监听 TCP `9099`，解密收到的密文并打印明文，然后回复加密后的 `ACK: <message>`。
- `chat_client.c`：连接服务端，使用 libhackode 加密用户输入并发送，再解密服务端回复。

客户端与服务端需要使用同一份词典（通过 `-D` 指定）。

## 使用 pkg-config 编译（系统已安装 -dev）

```bash
cc chat_server.c $(pkg-config --cflags --libs hackode) -o chat-server
cc chat_client.c $(pkg-config --cflags --libs hackode) -o chat-client
```

## 运行

终端 A：

```bash
./chat-server -D /usr/share/hackode/hackode.map -p 9099
```

终端 B：

```bash
./chat-client -D /usr/share/hackode/hackode.map -h 127.0.0.1 -p 9099
```

在客户端输入明文后，服务端会解密显示，并返回加密 ACK。

## 作为 Meson 子项目示例

若你把该目录拷贝到其他工程，`meson.build` 展示了典型接入方式：

```meson
hackode_dep = dependency('hackode', required: true)
executable('chat-server', 'chat_server.c', dependencies: [hackode_dep])
executable('chat-client', 'chat_client.c', dependencies: [hackode_dep])
```
