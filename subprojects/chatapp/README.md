# chatapp (socket + libhackode-dev example)

This example shows how to use `libhackode-dev` in a small TCP chat app.

- `chat_server.c`: listens on TCP `9099`, decrypts incoming ciphertext, prints plaintext, then replies with encrypted `ACK: <message>`.
- `chat_client.c`: connects to server, encrypts user input with libhackode, sends ciphertext, then decrypts server reply.

Both sides use the same dictionary via `-D`.

## Build with pkg-config (system installed -dev)

```bash
cc chat_server.c $(pkg-config --cflags --libs hackode) -o chat-server
cc chat_client.c $(pkg-config --cflags --libs hackode) -o chat-client
```

## Run

Terminal A:

```bash
./chat-server -D /usr/share/hackode/hackode.map -p 9099
```

Terminal B:

```bash
./chat-client -D /usr/share/hackode/hackode.map -h 127.0.0.1 -p 9099
```

Type plaintext in client, server receives/decrypts it, then sends encrypted ACK back.

## Build as Meson subproject sample

If you copy this directory into another project, this `meson.build` shows typical integration:

```meson
hackode_dep = dependency('hackode', required: true)
executable('chat-server', 'chat_server.c', dependencies: [hackode_dep])
executable('chat-client', 'chat_client.c', dependencies: [hackode_dep])
```
