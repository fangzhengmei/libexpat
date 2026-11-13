# 外部实体回调中的子解析器：重入、挂起/恢复与资源归属边界分析

分析对象：本仓库 `expat/`（libexpat 2.10.0 开发线，提交 477c766），核心文件
`expat/lib/xmlparse.c`、`expat/lib/expat.h`、`expat/doc/reference.html`。
本文不预设存在漏洞，只划分库支持的状态边界与调用方必须遵守的顺序，
并对每一边界给出最小调用序列与源码证据。

## 1. 对象模型与所有权事实

`XML_ExternalEntityParserCreate`（xmlparse.c:1718）创建的子解析器：

- 拷贝父解析器的全部处理器与 `userData`（xmlparse.c:1826-1860），
  并记录 `parser->m_parentParser = oldParser`（xmlparse.c:1861）。
- 一般外部实体（`context != NULL`）：子解析器持有独立 DTD，
  由 `dtdCopy` + `setContext` 填充（xmlparse.c:1876-1880）。
- 外部参数实体（`context == NULL`，`XML_DTD` 构建）：子解析器与根解析器
  共享 DTD，`m_isParamEntity = XML_TRUE`（xmlparse.c:1882）。
  源码注释明确说明此处不得调用 `setContext`，否则 prefix->binding
  指针会指向随子解析器销毁的内存，留下悬空指针
  （xmlparse.c:1876-1881 注释）。
- 释放侧的对称保护：`XML_ParserFree` 仅在 `!m_isParamEntity` 时才
  `dtdDestroy`（xmlparse.c:1965），共享 DTD 由根解析器拥有。
- 反放大记账（`XML_GE == 1`）的实体统计挂在根解析器上，
  子解析器每次实体展开都通过 `getRootParserOf` 沿 `m_parentParser`
  链向上走（xmlparse.c:9788-9800）。这是"父必须比子活得久"的
  具体机制，而不只是文档措辞。

文档契约：`reference.html`（`XML_ExternalEntityParserCreate` 与
`XML_ParserFree` 条目）明确要求"先释放子解析器，再释放父解析器"。

## 2. 库支持的重入与恢复顺序

### 2.1 支持的重入

处理器回调中对另一个解析器实例调用解析类 API 是支持的。
防护函数 `isCalledFromInsideHandler`（xmlparse.c:1209）只检查
本实例的 `m_handlerCallDepth`（`beforeHandler`/`afterHandler`，
xmlparse.c:1197-1206），因此在父解析器的 externalEntityRefHandler 里
对子解析器调用 `XML_Parse`/`XML_ParseBuffer`/`XML_ParserFree` 均合法。
同一回调中对本解析器调用这些函数则被拒绝：

- `XML_Parse`/`XML_Parse64`：xmlparse.c:2340-2341、2351-2352，返回
  `XML_STATUS_ERROR`；
- `XML_ResumeParser`：xmlparse.c:2794，返回 `XML_STATUS_ERROR`
  （回归测试 handlers.c:2035-2048 验证了该防护）；
- `XML_ParserFree`：xmlparse.c:1905，静默返回（见 3.2）；
- `XML_ParserReset`：xmlparse.c:1628 拒绝；且子解析器永远不可 reset
  （`m_parentParser` 检查，xmlparse.c:1631-1632）。

### 2.2 支持的挂起/恢复顺序

`XML_StopParser`（xmlparse.c:2755）只作用于当前实例。
expat.h:828-836 的注释明确：若要整体停下，回调实现必须自行对父解析器
（递归）调用 `XML_StopParser`。`XML_ResumeParser` 的文档注释
（expat.h:845-851）规定恢复顺序：先恢复嵌套最深的子解析器，
待子解析器结束后才恢复其父解析器，父解析器不会自动恢复。

最小受支持序列（异步模式）：

```
ExternalEntityRefHandler(parent, context, ...):
    child = XML_ExternalEntityParserCreate(parent, context, NULL)   # 回调内
    XML_StopParser(parent, XML_TRUE)        # 挂起父；当前实例，合法
    保存 child；return 1                    # 返回非 0，父进入挂起
# 之后（回调外）：
XML_Parse(child, 实体数据, len, 1)          # 子解析器独立推进
XML_ParserFree(child)                       # 子先于父释放
XML_ResumeParser(parent)                    # 最后恢复父
```

挂起期间父解析器的状态由 doContent 主循环底部的分派保存
（xmlparse.c:4195-4211）：`XML_SUSPENDED` 时记录 `*nextPtr` 并正常返回；
`XML_ResumeParser` 用保存的 `m_bufferPtr` 重新进入 `callProcessor`
（xmlparse.c:2802-2804）。若挂起点在内部实体展开中，恢复位置由
`internalEntityProcessor` 依据 `entity->processed` 偏移续传
（xmlparse.c:7345-7373）。

### 2.3 硬边界：外部参数实体不可挂起

`XML_StopParser(pe_parser, XML_TRUE)` 在 `m_isParamEntity` 为真时返回
`XML_ERROR_SUSPEND_PE`（xmlparse.c:2773-2778）。对共享 DTD 的参数实体
子解析器只能解析到底或中止（`resumable=XML_FALSE`），不能挂起。

## 3. 风险边界与最小触发序列

### 3.1 悬空引用：父先释放

最小序列：

```
child = XML_ExternalEntityParserCreate(parent, context, NULL);
XML_ParserFree(parent);        # 违反 reference.html 的顺序要求
XML_Parse(child, ...);         # 子解析器 m_parentParser 已悬空
```

失败机制：子解析器解析中任何实体记账（`XML_GE` 构建）都会经
`getRootParserOf`（xmlparse.c:9788-9791）解引用已释放的父解析器；
参数实体子解析器还会访问随父销毁的共享 DTD。库内没有任何防护，
这是纯粹的调用方契约。注意 `XML_ParserFree` 对子的 `m_parentParser`
不做空化处理，父解析器也不知道子的存在——归属关系是单向的。

### 3.2 静默无效的释放：回调内 `XML_ParserFree`

`XML_ParserFree(parser)` 在 `isCalledFromInsideHandler` 为真时直接返回
（xmlparse.c:1905），无错误码、无日志。最小风险序列：

```
ExternalEntityRefHandler(parent, ...):
    ...
    XML_ParserFree(parent);    # 静默 no-op，调用方误以为已释放
    return 1;
# 回调外再次 XML_ParserFree(parent)  -> 双重释放；
# 或不再释放 -> 泄漏
```

边界：回调内只能释放其他（已结束的子）解析器，不能释放正在执行
回调的实例本身；且调用方不能依赖 `XML_ParserFree` 的返回值判断成败
（它是 `void`）。

### 3.3 context 字符串的生命周期

传给 externalEntityRefHandler 的 `context` 由 `getContext` 构建在父解析器的
`m_tempPool` 中（xmlparse.c:8282-8289），回调返回后立即
`poolDiscard(&parser->m_tempPool)`（xmlparse.c:3874，唯一的 getContext 调用点在 xmlparse.c:3863）；

- 支持：`XML_ExternalEntityParserCreate(parent, context, enc)` 在回调内调用
  （`setContext` 在创建时即完成拷贝，xmlparse.c:1877-1879）。
- 风险：把 `context` 指针存起来、回调返回后再创建子解析器——指针已悬空。
  需要延迟创建时，调用方必须自行复制该字符串。expat.h:880-882 所说的
  "可在首次回调之后任意时刻调用"以调用方持有有效 context 副本为前提。

### 3.4 重复/额外汇报回调

`XML_StopParser` 的文档（expat.h:808-815）明确：停止后仍可能有少量
回调送达，否则事件会丢失——例如在 startElementHandler 中停止空元素时
对应的 endElementHandler 仍会来。调用方在 `XML_StopParser` 成功之后、
`XML_Parse` 返回之前必须容忍本实例继续收到回调，不能假设"停止即静默"。
恢复侧的对称风险是顺序性的而非库强制的：若在子解析器尚未完成时就
`XML_ResumeParser(parent)`（违反 expat.h:845-851 的顺序），父解析器会从
实体引用点之后继续产生事件，与子解析器后续事件在应用层交错——库不检测
这种交错，归属责任在调用方。

### 3.5 残留实体状态

- 外部实体：`entity->open` 在回调前后成对置位/复位
  （doContent：xmlparse.c:3862-3865；doProlog 参数实体：
  xmlparse.c:6526-6540；实体值：xmlparse.c:7767-7782），
  即使处理器返回 0 也先复位再返回
  `XML_ERROR_EXTERNAL_ENTITY_HANDLING`，无残留。
- 内部实体展开中被 `XML_StopParser(p, XML_FALSE)` 中止：
  `entity->open` 保持为真、`m_openInternalEntities` 非空
  （processEntity 置位 xmlparse.c:7303，正常关闭在
  internalEntityProcessor xmlparse.c:7394-7403）。但中止后解析状态为
  `XML_FINISHED`，`XML_Parse` 只会返回 `XML_ERROR_FINISHED`
  （xmlparse.c:2363-2365），残留状态不可达；`XML_ParserReset`
  会把打开实体链表并入空闲表并 `dtdReset` 清掉实体定义
  （xmlparse.c:1647-1662），`XML_ParserFree` 直接释放这些链表
  （xmlparse.c:1927-1953）。残留被封闭在实例生命周期内，不外溢。

## 4. 失败如何传播至父解析器，后续解析是否安全

子解析器的失败不会自动传播。库只提供一条显式通道：
externalEntityRefHandler 返回 0，父解析器在调用点把它映射为
`XML_ERROR_EXTERNAL_ENTITY_HANDLING`：

- 内容中的外部一般实体：xmlparse.c:3872-3873；
- 外部 DTD 子集/外部参数实体：xmlparse.c:5845-5846、5895-5896、
  6533-6536、7774-7779。

最小传播序列：

```
ExternalEntityRefHandler(parent, context, ...):
    child = XML_ExternalEntityParserCreate(parent, context, NULL);
    status = XML_Parse(child, text, len, 1);
    err = XML_GetErrorCode(child);     # 子解析器自己的具体错误
    XML_ParserFree(child);             # 无论成败都要释放
    return (status == XML_STATUS_OK) ? 1 : 0;   # 0 -> 父报错
```

父解析器侧：`XML_Parse`/`XML_ParseBuffer` 返回 `XML_STATUS_ERROR`，
`XML_GetErrorCode(parent)` 为 `XML_ERROR_EXTERNAL_ENTITY_HANDLING`，
`m_processor` 被置为 `errorProcessor`（xmlparse.c:2436-2439）。
此后对父解析器的再次 `XML_Parse` 只会经 `errorProcessor` 幂等地返回
同一错误（xmlparse.c:7409-7415），不会崩溃也不会继续消费输入；
要复用该实例只能 `XML_ParserReset`（仅限根解析器，
xmlparse.c:1631-1632）。子解析器侧的错误细节（行号、具体错误码）
在回调返回 0 之后就无法再从父解析器获得，调用方须在回调内自取。

对称地，子解析器内 `XML_StopParser(child, XML_FALSE)` 只让子解析器
自身以 `XML_ERROR_ABORTED` 结束（doContent 底部 xmlparse.c:4200-4201；
XML 声明处 externalEntityInitProcessor3 xmlparse.c:3671-3672），
父解析器是否失败仍取决于回调的返回值。

## 5. 边界一览

| 行为 | 库的支持状态 | 证据 |
| --- | --- | --- |
| 回调内创建并同步解析/释放子解析器 | 支持 | xmlparse.c:1718, 1905（防护按实例） |
| 回调内挂起父解析器，之后先完成子再恢复父 | 支持，顺序由文档规定 | expat.h:828-836, 845-851 |
| 挂起外部参数实体子解析器 | 拒绝，`XML_ERROR_SUSPEND_PE` | xmlparse.c:2773-2778 |
| 回调内对本解析器 `XML_Parse`/`XML_ResumeParser` | 拒绝，返回错误 | xmlparse.c:2351-2352, 2794 |
| 回调内对本解析器 `XML_ParserFree` | 静默 no-op | xmlparse.c:1905 |
| 子解析器 `XML_ParserReset` | 拒绝，返回 `XML_FALSE` | xmlparse.c:1631-1632 |
| 父先于子释放 | 未防护的悬空引用（调用方契约） | xmlparse.c:1861, 9788-9791；reference.html |
| 回调返回后使用 `context` 指针 | 悬空（tempPool 已丢弃） | xmlparse.c:8282-8289, 3874 |
| 子解析器失败经返回值 0 传播 | 支持，父报 `XML_ERROR_EXTERNAL_ENTITY_HANDLING` | xmlparse.c:3872-3873 等 |
| 失败后父解析器再次 `XML_Parse` | 安全，幂等返回原错误 | xmlparse.c:2436-2439, 7409-7415 |
| 中止后残留 `entity->open`/打开实体表 | 封闭在实例内，reset/free 时清理 | xmlparse.c:7303, 1647-1662, 1927-1953 |
