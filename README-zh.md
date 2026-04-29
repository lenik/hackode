# hackode: **黑客字典编码工具**

`hackode` 提供两部分能力：
- `enckode` / `deckode`：基于词典的可逆变换 CLI
- `libhackode`：可复用的 C 共享库（`libhackode.so`，含头文件和 pkg-config）

`deckode` 是 `enckode` 的符号链接别名，以 `deckode` 调用时默认进入解密模式。

## 内部机制（当前实现）

核心思路：将数据表示为 base-`N`（`N=词典长度`）的“数字序列”，再在 `Z_N` 上做可逆映射。

1) **词典作为字母表**
- 词典词条是 base-`N` 的符号
- `N > 10`
- 词条必须非空、不能包含空白字符，且不能是 `|`

2) **可逆映射**
- 选择 `p`，满足 `gcd(p, N)=1`
- 计算逆元 `q = p^{-1} mod N`
- 正向映射：`enc_div = (div * p) mod N`
- 逆向映射：`div = (enc_div * q) mod N`

3) **文本模式编码**
- 明文按 `chunk_size`（CLI 默认 4）分块
- 每块按大端解释为整数
- 输出是纯 token 流（默认空格分隔），不使用 `|` 分组标记
- 长度前缀 `len` 使用可变长度编码，基数为  
  `N_half = floor((N - 1)/2)`
  - 非最后 token：`2 * div + 1`（奇数）
  - 最后 token：`2 * div`（偶数）
- 解码时先读长度前缀（遇到首个偶数 token 结束），再按固定宽度读取后续 chunk token

4) **数字模式**
- 直接对无符号整数做 base-`N` 的 div 变换
- `enckode -n` 输出数字 div，`deckode -n` 还原整数

## 词典配置

```bash
enckode [OPTION]... [args]...
deckode [OPTION]... [args]...
```

通过 `-D/--dict` 指定词典来源：
- `.map`：直接 mmap 加载（启动更快，推荐生产）
- 文本词典：每行一个词；空行和 `#...` 注释行忽略

示例：

```bash
enckode -D /path/to/hackode.map "hello"
enckode -D /path/to/words.txt "hello"
```

文本词典路径会经过 `dict_load_auto()` 自动编译到临时 map 后再加载。

### 生成自定义 map

```bash
ninja -C /build hcdict-tool
/build/hcdict-tool ./my.dict ./my.map
enckode -D ./my.map "secret message"
```

## CLI 用法

```bash
enckode [OPTION]... [args]...
deckode [OPTION]... [args]...
```

若未提供 `args`，程序会把 stdin 作为一条完整消息读取。

### 选项

- `-D, --dict FILE` 指定词典（`.map` 或文本）
- `-e, --encrypt` 强制加密模式
- `-d, --decrypt` 强制解密模式
- `-n, --number` 数字模式
- `-a, --auto` 自动识别每个参数是数字或字符串
- `-s, --string` 强制按字符串处理（覆盖 `--auto`）
- `-c, --stdout` 输出到 stdout（保留选项，当前默认行为）
- `-v`, `--verbose`
- `-q`, `--quiet`
- `-h`, `--help`
- `--version`

### 示例

文本加解密：

```bash
enckode "hello world"
deckode "<cipher text>"
```

数字模式：

```bash
enckode -n 42 314159
deckode -n 7 3 11
```

管道模式：

```bash
echo "hello hackode" | enckode
echo "<cipher text>" | deckode
```

## 构建与测试

### 依赖

```bash
sudo apt install meson ninja-build gcc pkg-config check libbas-c-dev
```

### 配置与编译（使用绝对目录 `/build`）

```bash
meson setup /build
ninja -C /build
```

### 测试

```bash
meson test -C /build
```

当前测试目标：
- `tests/enckode_test.c`
- `tests/hc_cipher_unit.c`

## `libhackode-dev` 开发接入

安装后提供：
- 共享库：`libhackode.so`
- 头文件：`hackode/*.h`
- pkg-config：`hackode.pc`、`hackode-static.pc`

### 用 pkg-config 编译

```bash
cc demo.c $(pkg-config --cflags --libs hackode) -o demo
```

### Meson 中使用

```meson
hackode_dep = dependency('hackode', required: true)
executable('mytool', 'mytool.c', dependencies: [hackode_dep])
```

## i18n（gettext）

翻译源位于 `po/*.po`，编译后为 `po/<lang>/LC_MESSAGES/hackode.mo`。

同步词条：

```bash
msgupdate
```

快速测试语言：

```bash
LANGUAGE=ja /build/enckode -h
LANGUAGE=zh_CN /build/enckode -h
```

## 安装 / 符号链接辅助命令

```bash
meson install -C /build
```

本地调试（在已配置 prefix 下创建/清理链接）：

```bash
ninja -C /build install-symlinks
ninja -C /build uninstall-symlinks
```

链接内容包括：
- `bin/` 下的 `enckode` 与 `deckode`
- `man1/` 下的 `enckode.1` 与 `deckode.1`
- `bash-completion` 下的 `enckode` 与 `deckode`

## Debian 打包

```bash
dpkg-buildpackage -us -uc
```

## 许可证

Copyright (C) 2026 Lenik <hackode@bodz.net>

采用 **AGPL-3.0-or-later** 许可。
本项目明确反对 AI 剥削与 AI 霸权，反对无脑 MIT 式许可证和政治愚蠢的 BSD 式许可证。
完整文本及项目补充条款见 `LICENSE`。
