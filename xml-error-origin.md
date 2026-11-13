# libexpat 错误位置报告与原始输入边界的对应关系分析

仓库：`/srv/fb2/fangzhengmei/0012-libexpat-6-9312f322`（expat 2.10.0 开发快照，提交 `477c766`）。
分析对象：`expat/lib/xmlparse.c`、`expat/lib/xmltok.c`、`expat/lib/xmltok_impl.c`、`expat/lib/expat.h`。
文中所有"实测"结果均来自针对本仓库静态构建（`XML_CONTEXT_BYTES=1024`）的可复核 C 程序，样本输入以十六进制字节给出，可原样重放。

## 1. 位置信息的产生机制

expat 对调用方暴露五类位置接口（`expat/lib/xmlparse.c:2847-2986`）：

- `XML_GetCurrentByteIndex64`：`m_parseEndByteIndex - (m_parseEndPtr - m_eventPtr)`。`m_parseEndByteIndex` 是累计喂入的**原始字节**总数（`xmlparse.c:2406`、`2578`），因此字节偏移以**原始编码的字节**为单位，跨缓冲区累计，与转码后的 `XML_Char` 无关。
- `XML_GetCurrentLineNumber64` / `XML_GetCurrentColumnNumber64`：惰性计算。`XmlUpdatePosition`（`xmltok_impl.c:1778`）从 `m_positionPtr` 逐字符扫描到 `m_eventPtr`，**列号按字符（码点）计数而非字节**：UTF-8 的 2/3/4 字节序列只算 1 列（`LEAD_CASE` 宏），UTF-16 的代理对（`BT_LEAD4`，4 字节）也只算 1 列。行号内部从 0 计、返回时 +1；列号从 0 计。
- `XML_GetCurrentByteCount64`：`m_eventEndPtr - m_eventPtr`，对 `XML_TOK_INVALID` 类错误为 0（两个指针都指向非法字节）。
- `XML_GetInputContext64`：返回**当前缓冲区**指针及事件在缓冲区内的偏移，仅在处理器回调内部（`m_handlerCallDepth > 0`，`xmlparse.c:1209`）可用，且需 `XML_CONTEXT_BYTES > 0`。

关键前提：`m_eventPtr` 指向哪个缓冲区，决定了位置的可信度。`doContent` 每轮循环末尾执行 `*eventPP = s = next`，使事件指针始终落在当前 token 起点；`XML_TOK_INVALID` 时改为 `*eventPP = next`，而 `next` 由分词器精确指向非法字节（`xmltok_impl.c:48-67` 的 `INVALID_CASES` 宏：`*nextTokPtr = ptr; return XML_TOK_INVALID`）。

## 2. UTF-8 / UTF-16 截断与非法序列的可复核样本

### 2.1 UTF-8

| 样本（hex） | 含义 | 错误码 | byteIndex | line:col |
|---|---|---|---|---|
| `3c646f63 3e e4b8`（final） | `<doc>` + 截断的 3 字节字符（仅 2 字节） | 6 `XML_ERROR_PARTIAL_CHAR` | 5 | 1:5 |
| 同上，非 final | 缓冲区边界的半截字符 | 无错误（`XML_STATUS_OK`，等待续传） | 5 | 1:5 |
| `3c646f63 3e e428 3c2f646f633e` | `<doc>` + 非法续字节 `0x28` | 4 `XML_ERROR_INVALID_TOKEN` | 5 | 1:5 |
| `3c646f63 3e 61 ff 623c2f646f633e` | 孤立非法字节 `0xff` | 4 `XML_ERROR_INVALID_TOKEN` | 6 | 1:6 |
| `3c646f63 3e 0a0a e428`（final） | 第 3 行起点的 `e4 28`，但缓冲区在序列完成前结束 | 6 `XML_ERROR_PARTIAL_CHAR` | 7 | 3:0 |
| `efbbbf 3c646f63 3e e428 78` | 带 UTF-8 BOM 的非法序列 | 4 `XML_ERROR_INVALID_TOKEN` | 8 | 1:6 |

要点：

- **非法序列指向首字节还是肇事字节取决于分词器停在哪**。`INVALID_LEAD_CASE` 在 lead 字节完整但字符非法时把 `nextTokPtr` 设为 lead 字节本身（`xmltok_impl.c:48-56`），因此 `e4 28` 报在偏移 5（`e4` 处），而孤立的 `ff`（`BT_NONXML`/`BT_TRAIL` 分支）报在 `ff` 自身处。两种情况下 byteIndex 都精确落在原始输入的对应字节上。
- **截断与非法在缓冲区边界会互相伪装**：`e4 28` 若恰好是缓冲区最后两个字节且非 final，分词器返回 `XML_TOK_PARTIAL_CHAR` 而非 `XML_TOK_INVALID`——因为 `end - ptr < 3` 时先判 PARTIAL（`xmltok_impl.c:50-51`），根本没机会检查 `0x28` 是否合法。final 缓冲区下报 `XML_ERROR_PARTIAL_CHAR`（错误码 6），非 final 下完全不报错。调用方若只凭错误码判断"是否有恶意字节"，会被缓冲区切分方式影响。
- **BOM 计入列号**：位置扫描从原始缓冲区起点开始，BOM 被算作 1 列（上表最后一行 col=6 = BOM(1) + `<doc>`(5)），但字节偏移同样包含 BOM 的 3 字节，两者自洽。
- 分块续传可正常拼接：`<doc>\xe4`（非 final）+ `\xb8\xad</doc>`（final）解析成功，结束后 byteIndex=14、col=12（按字符计）。非法续字节在第二块到达时报在**全文**偏移 5 处，即跨块累计偏移与单块一次性输入完全一致。

### 2.2 UTF-16

| 样本 | 含义 | 错误码 | byteIndex | line:col |
|---|---|---|---|---|
| `fffe` + `<doc>`(LE) + `2d`（final） | 末尾 dangling 单字节 | 5 `XML_ERROR_UNCLOSED_TOKEN` | 12 | 1:6 |
| 同上，非 final | 同上 | 无错误 | 12 | 1:6 |
| `fffe` + `<doc>`(LE) + `00d8`（final） | 末尾孤立高代理项 | 6 `XML_ERROR_PARTIAL_CHAR` | 12 | 1:6 |
| `fffe` + `<doc>`(LE) + `00d8` + `3c00` | 高代理项后接非低代理项 | 4 `XML_ERROR_INVALID_TOKEN` | 12 | 1:6 |
| `fffe` + `<doc>`(LE) + `0100` + `3c00` | 非法控制字符 U+0001 | 4 `XML_ERROR_INVALID_TOKEN` | 12 | 1:6 |
| `<doc>`(BE，无 BOM) + `d800`（final） | BE 孤立高代理项 | 6 `XML_ERROR_PARTIAL_CHAR` | 10 | 1:5 |
| `fffe` + `<doc>\n\n`(LE) + `0100` | 第 3 行的非法控制字符 | 4 `XML_ERROR_INVALID_TOKEN` | 16 | 3:0 |

要点：

- **字节偏移永远以 2 字节码元为单位对齐到字符边界**（除 dangling 单字节情形），行列号按字符计：`<doc>` 5 个字符后 col=5（无 BOM）或 6（有 BOM，BOM 算 1 列）。
- **三种"截断"对应三种不同错误码**，调用方需要区分：奇数字节结尾（半个码元）→ `XML_ERROR_UNCLOSED_TOKEN`（分词器连一个完整码元都凑不齐，走 `XML_TOK_PARTIAL`）；孤立高代理项 → `XML_ERROR_PARTIAL_CHAR`；代理项配对错误 → `XML_ERROR_INVALID_TOKEN`。byteIndex 在三种情况下都指向问题起始字节，可精确定位。
- UTF-16 下 `MINBPC=2`，`HAS_CHAR` 需要 2 字节，因此单字节残留永远不会被误判为非法字符，只会是 unclosed/partial。

结论（第 1 问）：对单解析器、单字节流，byteIndex 可以**字节级精确**地指向原始输入中的截断点或非法字节；行列号与字节偏移自洽（列按码点计、BOM 计 1 列）。可信的前提是调用方用 `*64` 接口且理解 PARTIAL_CHAR 与 INVALID_TOKEN 在缓冲区边界的互换性。

## 3. 外部实体嵌套与父子解析器的错误上下文传播

### 3.1 传播链实测

样本文档（父）：

```xml
<!DOCTYPE d [
  <!ENTITY e1 SYSTEM "level1">
  <!ENTITY e2 SYSTEM "level2">
]>
<d>&e1;</d>
```

`level1` 实体内容为 `<a>\n\n&e2;</a>`（第 3 行引用 e2）；`level2` 实体内容为 `<ok>\xe4\x28</ok>`（偏移 4 处非法 UTF-8）。`XML_ExternalEntityRefHandler` 中用 `XML_ExternalEntityParserCreate` 建子解析器并喂入对应字节流，实测输出：

```
[grandchild] err=4 (not well-formed (invalid token)) byteIndex=4  line=1 col=4
[child]      err=21 (error in processing external entity reference) byteIndex=5 line=3 col=0
[parent]     err=21 (error in processing external entity reference) byteIndex=82 line=5 col=3
```

### 3.2 机制分析

- 每个经 `XML_ExternalEntityParserCreate`（`xmlparse.c:1718`）创建的子解析器是**独立的位置坐标系**：`parserCreate` 把 `m_parseEndByteIndex` 置 0、`m_position` 清零（`xmlparse.c:1533`、`1549`），子解析器的 byteIndex/line/col 全部相对于**该实体自己的字节流**起点，与父文档的偏移无任何换算关系。
- 子解析器失败时，错误码**不会**上传到父解析器。父解析器在 `XML_ExternalEntityRefHandler` 返回 0 后统一报 `XML_ERROR_EXTERNAL_ENTITY_HANDLING`（21）（`xmlparse.c:3873`、`5846`、`5896`、`6536`），其 `m_eventPtr` 停在**实体引用 `&e1;` 的位置**。多级嵌套时每一级都只知道自己流内的引用点：child 报 line 3 col 0（`&e2;` 在 level1 流中的位置），parent 报 line 5 col 3（`&e1;` 在父文档中的位置）。
- 真正的恶意字节位置（grandchild 的 byteIndex=4）**只存在于最内层子解析器对象中**，且只在该对象存活、且在 `XML_ParserFree(child)` 之前可读。`XML_GetInputContext64` 更进一步：只能在**该解析器自己的回调**执行期间返回其缓冲区（实测在 handler 外查询返回 `NULL`；在子解析器默认回调内查询返回子缓冲区、偏移相对于子流）。`XML_Parse` 路径下库会临时伪造 `m_buffer` 使该接口在回调内可用（`xmlparse.c:2411-2420`）。
- "缓冲区在错误处终止"的情形（实体字节流最后一个字节即非法/半截字符）：子解析器按第 2 节的规则在**自己的坐标系**内精确报出（实测 `<a>text\xff` 报 byteIndex=7 指向 `ff`；若结尾是半截 UTF-8 且 isFinal 则报 `XML_ERROR_PARTIAL_CHAR` 指向该字符首字节；若 isFinal=FALSE 则不报错、等待续传）。父解析器对此一无所知，只看到 handler 返回 0。

### 3.3 调用方定位恶意字节的可信范围

1. **可信**：最内层失败子解析器的 `XML_GetCurrentByteIndex64` / 行 / 列，相对于**该实体字节流**起点，字节级精确。若调用方自己负责抓取该实体的字节（通常如此，字节本来就是调用方喂进去的），这个偏移可以直接索引到恶意字节。
2. **部分可信**：父链上每一级的位置只标识"哪一级实体引用触发了下游失败"，即引用点位置，**不是**恶意字节位置。把父解析器的 byteIndex 当作恶意字节偏移去切片父文档，得到的是 `&e1;` 而非攻击载荷。
3. **不可信/不可得**：
   - 子解析器被 `XML_ParserFree` 后，其错误上下文完全丢失；父解析器没有任何 API 能取回子错误码或子位置。
   - `XML_GetInputContext64` 只暴露**当前缓冲区**（最多保留 `XML_CONTEXT_BYTES`），已被消费的历史缓冲区的字节无法回溯；且在回调外调用返回 `NULL`。
   - 内部实体（`processEntity`/`internalEntityProcessor`，`xmlparse.c:7238+`）展开期才暴露的错误（如 `<!ENTITY e "<open>">` 的 `XML_ERROR_ASYNC_ENTITY`、展开时的非法 token），`m_eventPtr` 停在**主文档中的实体引用处**（实测 byteIndex 指向 `&e;` 的 `&`），而非实体替换文本内部——因为内部实体展开时 `eventPP` 被重定向到 `openInternalEntities->internalEventPtr`（`doContent` 开头），主事件指针不再前进。注意区分：实体**声明**中的非法字节在 DTD 解析期就被捕获，byteIndex 精确指向声明里的肇事字节（实测偏移 30/133 随文档填充同步移动），只有**展开期**错误才退化为引用点位置。

### 3.4 旧接口兼容边界

`expat.h:972-1005` 将 5 个位置接口的旧版本标记为 `XML_ATTR_DEPRECATED`（自 2.9.0 起），推荐全部换用 `*64` 版本。旧接口的边界是 **ABI 冻结的已知缺陷**（`xmlparse.c` 中逐条注释 "That's a bug and it only lives on because we cannot break ABI compatibility of public API"）：

| 旧接口 | 返回/参数类型 | 回绕边界 | 替代 |
|---|---|---|---|
| `XML_GetCurrentByteIndex` | `XML_Index`（long） | >2 GiB 输入在 32 位平台与 64 位 Windows（LLP64）上回绕，除非定义 `XML_LARGE_SIZE` | `XML_GetCurrentByteIndex64`（int64_t，事件无效时返回 -1） |
| `XML_GetCurrentLineNumber` / `XML_GetCurrentColumnNumber` | `XML_Size`（unsigned long） | >4 GiB 回绕 | `*_64`（uint64_t） |
| `XML_GetCurrentByteCount` | int | >2 GiB token 回绕 | `XML_GetCurrentByteCount64` |
| `XML_GetInputContext` | `int *offset, int *size` | 缓冲区偏移/尺寸 >2 GiB 回绕 | `XML_GetInputContext64` |

在本分析平台（64 位 Linux，LP64）上 `sizeof(XML_Index)==8`，旧接口不会回绕，问题仅在跨平台（32 位、Windows）或超大输入时显现。对"定位恶意字节"这一用途，实际约束是：只要输入可能超过 2 GiB 或需跨平台，旧接口返回的偏移就是不可信的；`*64` 接口在 2.9.0 之前的 expat 上不存在，这是调用方需要处理的版本兼容边界。

## 4. 复核方法

```sh
cd expat && cmake -B build -DEXPAT_BUILD_TESTS=OFF -DEXPAT_SHARED_LIBS=OFF
cmake --build build
# 测试程序按第 2、3 节的十六进制样本构造输入，
# 链接 build/libexpat.a，打印 XML_GetErrorCode /
# XML_GetCurrentByteIndex64 / XML_GetCurrentLineNumber64 /
# XML_GetCurrentColumnNumber64 / XML_GetInputContext64。
```

第 2 节表格中的每一行都对应一个不超过 16 字节的字面量输入，可直接用 `printf '\x..'` 重放；第 3 节的三级嵌套只需在 `XML_ExternalEntityRefHandler` 里按 `systemId` 分发到对应字面量字节流即可复现整条传播链。
