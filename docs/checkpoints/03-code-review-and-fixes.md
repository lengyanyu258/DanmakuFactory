# 当前代码变更审查、修复与验收台账

- 记录日期：2026-10-03（Asia/Shanghai）。
- 审查基线：`e2d0cd95f46550b094c890781ea385656f8021e8`。
- 范围：相对该 HEAD 的已跟踪文件变更，以及当时的未跟踪 C 模块、Rust CLI、Slint GUI、多语言绑定、网页、WASM 接口、构建脚本、CI 和测试源码。既有实现中被当前转换链路暴露的问题也纳入；不能据此把每项问题都归因于本轮新增代码。
- 工作方式：三路并行文本审查与主代理整合；用户最初授权修复，随后要求统合文档；文档阶段暂停继续源码修复。
- 验证限制：本轮未编译、未构建、未运行测试、未启动 GUI、未进行浏览器或字幕渲染验证。新增测试只是源码。只读差异复查与 `git diff --check` 检查过部分修改，但不构成功能或 ABI 验证。
- 本文 ID `REV-001` 起为稳定 checkpoint；后续补充保留原 ID，新增项使用新 ID，避免重新编号。

## 严重等级、状态与责任

| 等级 | 判定标准 | 处理顺序 |
| --- | --- | --- |
| P0 | 已确认存在普遍、不可恢复的数据损坏，或必须立即停止交付的系统性风险。 | 先止损；本轮没有足够证据确认 P0。 |
| P1 | 内存越界、未初始化或失效指针、确定的数据覆盖，或关键入口无法正常运行。 | 优先修复；涉及发布链时先闭合依赖与装载。 |
| P2 | 可复现的结果错误、兼容性偏差、资源泄漏、输入验证遗漏，或主要功能交付未闭环。 | 在相关入口交付前完成。 |
| P3 | 模块职责、重复实现、可维护性、未承诺的扩展功能或后续架构改善。 | 明确方案与验收，再分批实现。 |

**“已改源码，未运行验证”**只说明源码中已实施对应修复。它不等于已通过编译、测试、跨平台验证或可发布。**“待处理”**表示源码冻结时仍有具体缺口。**“待验收”**表示实现存在，但没有当前产物和运行证据。

责任按模块划分：C 核心维护者负责 ABI、解析、调度和内存；CLI 维护者负责参数与兼容文档；Rust/build 维护者负责 Cargo/xmake 交界与依赖；GUI 维护者负责配置、交互和发行；Web/WASM 维护者负责浏览器加载、产物和平台差异。本文不把个人名称当作永久责任归属。

## 尚未完成的 checkpoints：先处理这些

| ID | 优先级 / 职责 | 当前缺口 | 下一步与验收 |
| --- | --- | --- | --- |
| REV-051 | P1 / Rust/build | `DF_XREPO_LIB_DIR` 的生产者已经输出路径列表，消费者仍把整串当单一路径。 | 在真实文件 [bindings/rust/build.rs](../../bindings/rust/build.rs) 使用 `env::split_paths`；逐项输出 search path，加入变更触发；在 Windows、Unix、静态与共享模式验证多目录输入。 |
| REV-052 | P1 / Rust/build | `DF_RUNTIME_DLL_DIRS` 已由脚本提供，但 Rust sys 构建脚本未消费；目前只扫描核心库目录。 | 消费多目录运行时 DLL，保留同文件跳过与重名策略；验证 CLI、GUI、tests、examples 在干净目录中能装载全部 DLL。 |
| REV-053 | P1 / C 消息动画 | 历史消息链中若干 `malloc` 后立即解引用；多个 `void` helper 无法向上返回失败。 | 统一可失败的分配与调用契约、链表所有权及错误出口；故障注入逐一验证无 NULL 解引用、泄漏或二次释放。 |
| REV-054 | P1 / C 几何与字号 | 配置字号仍只要求大于 0；估算宽度、字号映射、`textHei + line_spacing`、宽度加分辨率等仍可能整数溢出。 | 在公开输入、估算与排布三个边界联合约束；使用宽整数中间值后校验/钳制；补巨大字号、长 JSON 文本、INT 极值行距和分辨率测试。源码冻结前未落盘这些修复。 |
| REV-055 | P2 / CLI 兼容契约 | `-c` 零参数打印、最终配置打印、独立 `--save`、完整默认值保存、旧别名和双值坐标未完全实现文档契约。 | 对照 [CLI 参数说明](../DanmakuFactory%20CLI%20参数说明.md) 以既有文档和 API 为基准逐项恢复兼容；存在歧义时向用户确认，不能通过缩减文档关闭缺口；具体清单见后文。 |
| REV-065 | P2 / API 并发契约 | 公开头文件仍断言没有跨调用全局可变状态，而字体注册表为全局数据并要求初始化期同步。 | 限定转换 API 与注册阶段的线程保证，统一两个头文件契约；不能在没有证据时宣称并发转换已存在数据竞争。 |
| REV-066 | P2 / 原生正则失败策略 | 编译失败、匹配数据分配失败及非 NOMATCH 匹配错误仍可能静默降级/漏过滤并最终返回成功。 | 区分无效规则、正常不匹配与实际运行失败；明确兼容策略，分配/运行错误向调用者报告；故障注入验收，realloc 修复不覆盖此项。 |
| REV-056 | P2 / GUI 发行 | Slint 源码路径已修，构建、平台依赖、可执行文件和发布包未形成闭环。 | 闭合独立 GUI 构建任务与 release packaging，验证字体、核心库、第三方运行库和启动。 |
| REV-057 | P2 / 静态交付 | 静态依赖列表和 Cargo/xmake 路径改过，但未验证所有传递库、链接顺序、目标平台系统库与分发契约。 | 检查实际依赖闭包，分别完成 Windows、Linux/musl、macOS 静态验收；不能用共享库成功代替。 |
| REV-058 | P2 / WASM 行为 | 原生 PCRE2 正则路径与 WASM 字符串屏蔽路径语义不同。 | 明确能力协商或统一实现；相同 block list 在两端的结果差异必须可解释、可测试。 |
| REV-059 | P2 / WASM 产物 | 现有 `docs/wasm` 二进制未按本轮 C/导出表重新构建。 | 在用户允许构建后重新生成产物，核对版本、typed exports、加载器与回归；历史 smoke 不能代替。 |
| REV-060 | P2 / 文本渲染 | 测宽模块不是完整 libass 一致性实现；多字体 fallback、换行、缩放等未建立可视验收。 | 做字体覆盖与实际 ASS 渲染对比，再决定引入 libass 或继续改进测宽。 |
| REV-061 | P3 / 配置架构 | 历史 `readConfig/writeConfig` 与主 JSON parser 重复，契约可能不一致。 | 先列出调用者和兼容需求，再归一到单一配置模型；不因旧接口暂未调用就直接删除。 |
| REV-062 | P3 / Web 产品 | PWA、离线交付和实际字幕预览未实现；旧浏览器支持也未验证。 | 明确目标浏览器与离线/预览需求后实现；以真实浏览器和产物验收。 |
| REV-063 | P2 / 验证职责 | 全部源码修复和新增回归均未运行；当前不能据此确认 release readiness。 | 保持用户“本轮不构建”的约束；以后获准时按分层验收清单执行并填写证据。 |
| REV-064 | P3 / 模块职责 | ASS 文件仍混合格式解析、样式、排布、消息动画和统计，多语言入口也存在配置规范化重复。 | 按职责拆分并保持行为；先处理 P1，再单独做结构重构，避免与兼容行为变更混在一起。 |

## C API、配置模型与解析边界

以下 `REV-001` 至 `REV-010` 的源码状态均为：**已改源码，未运行验证**。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-001 / P1 | 文件类型查找把较窄 enum 地址当 `uint64_t *` 传入，较宽写入破坏相邻栈数据。 | [base/utils.c](../../lib/src/base/utils.c) 使用 `uint64_t` 临时接收再显式赋值。 | 静态核对写入宽度；后续用未知扩展名、显式类型和各平台 ABI 验收。 |
| REV-002 / P1 | block/statistics 的窄标志字段同样经 `uint64_t *` 写入，配置相邻字段可能被覆盖。 | [config/parse.c](../../lib/src/config/parse.c) 使用宽临时值与边界检查；空数组表示替换为空，避免保留旧模式。 | [test_config.py](../../tests/test_config.py) 的模式/配置回归源码；验收空数组、组合数组和数值模式不改其它字段。 |
| REV-003 / P2 | 句柄反复加载 block list，临时配置浅拷贝指针；失败时新列表或成功时旧列表未释放。 | [danmakufactory.c](../../lib/src/danmakufactory.c) 统一列表释放 helper；失败仅释放新列表，成功替换后释放旧列表，缺失字段继续保留旧列表。 | parser 在完整新列表分配后才提交指针；需加载成功/失败/替换/保留序列和内存检查。 |
| REV-004 / P1 | 输入数组直接 realloc 与 `realloc(0)`、部分失败清理交织，可能使旧指针失效或事务状态不完整。 | [config/parse.c](../../lib/src/config/parse.c) 使用局部 calloc 和临时数组，成功才提交；失败保留或清理明确的所有者。 | [test_config.py](../../tests/test_config.py) 覆盖输入结构；后续加入故障注入与空数组行为验收。 |
| REV-005 / P2 | 整份 JSON 转小写同时改变字体名、路径、黑名单和正则等字符串值；大小写敏感平台尤其受影响。 | [config/parse.c](../../lib/src/config/parse.c)、[parse.h](../../lib/src/config/parse.h) 增加 `df_json_get_member`，只做键的大小写不敏感查找；[danmakufactory.c](../../lib/src/danmakufactory.c) 取消整体小写并使用该 helper。 | 大小写路径、字体和黑名单值必须原样保存；键名仍可大小写混用。模式值保留既有大小写不敏感解析。 |
| REV-006 / P2 | 文档/调用方使用嵌套 screen/danmaku/text/msgbox 配置，解析路径与 flat/legacy aliases 的覆盖顺序可能不一致。 | [config/parse.c](../../lib/src/config/parse.c) 实际解析嵌套结构并保留 flat aliases；嵌套字段优先规则明确。 | [test_config.py](../../tests/test_config.py) 覆盖嵌套、flat、别名；与 CLI 多文件规范化共同验收。 |
| REV-007 / P1 | 数字 JSON 直接隐式转 int/float、坐标用宽松扫描，非有限值或越界值可导致未定义转换、截断和错误状态。 | [config/parse.c](../../lib/src/config/parse.c) 检查 finite、int/float 范围；有符号坐标使用有界 strtol；拒绝不合类型的值。 | [test_config.py](../../tests/test_config.py) 的 NaN/Inf/超界/坐标输入源码；配置字号和几何组合仍见 REV-054。 |
| REV-008 / P2 | 只做最小值比较会漏 NaN；极大有限 duration/price 在毫秒或厘转换时溢出；消息框尺寸原先缺约束。 | [config/validate.c](../../lib/src/config/validate.c) 检查 finite，scroll/fixed duration 上界为 `(MAX_ASS_MS_INT-10)/1000.0`，消息 duration 非负且有相同上界，礼物价格有界，msgbox size 两轴大于 0。 | [test_config.py](../../tests/test_config.py) 新增直接 handle 设置非有限及巨大 duration 的源码；未执行。字号上界未落盘。 |
| REV-009 / P2 | 文件描述缺 path、类型不对或含嵌入 NUL，可被错误解释为截断路径或触发空指针路径。 | [config/parse.c](../../lib/src/config/parse.c) 拒绝缺失/非法 path 与嵌入 NUL，并校验显式文件类型。 | 验收 bare path、typed descriptor、空值、NUL、无效格式名称；与固定长度路径约束共同检查。 |
| REV-010 / P2 | 非有限 time_shift 或 input_files 指针/数量组合无效，继续进入读取链路。 | [config/validate.c](../../lib/src/config/validate.c) 增加 finite 与数组结构约束。 | API/CLI/GUI 三入口均须拒绝非有限偏移；不能只依赖 JSON 通常无法表示 NaN 的假设。 |

配置合并的所有权约定是：上下文持有已提交的 block list 和输入数组；临时解析只持有新分配结果；失败不释放上下文旧资源；成功替换释放旧资源。此约定应成为后续故障注入和语言绑定测试的共同基础。

## C 核心、ASS 文件生命周期与排布

以下 `REV-011` 至 `REV-020` 的源码状态均为：**已改源码，未运行验证**。其中首对排序、ASS 读取/关闭和若干时间/排布缺陷明确存在于历史实现；当前转换链路复用它们，因此一并修复，不能表述成全部由新 API 引入。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-011 / P1 | 两次 realloc 一次成功移动、另一次失败，清理仍使用旧 regCodes/regCodeMap；造成失效指针读取、二次释放或泄漏。 | [core/list.c](../../lib/src/core/list.c) 每次 realloc 成功立即提交当前有效地址，失败出口从有效所有者清理。 | 当前仅静态路径推演；未来对两种单侧失败及双方失败做分配故障注入。其它正则编译/匹配失败策略尚未修复，见 REV-066。 |
| REV-012 / P2 | 两节点 `[late, early]` 或首节点晚于其余节点，排序检查漏首对并直接当成已排序；时间跨度 int 减法还可能溢出。 | [core/list.c](../../lib/src/core/list.c) 比较全部相邻对；桶跨度与归一化差先转 double。 | [test_convert.py](../../tests/test_convert.py) `test_two_descending_danmaku` 同时检查文本和输出时间顺序；未执行。 |
| REV-013 / P1 | 不存在 ASS 文件时仍把未初始化中间结构送入转换；读取 OOM 后可能继续处理已释放数据；成功路径泄漏整个中间 ASSFILE。 | [ass/file.c](../../lib/src/ass/file.c) 零初始化 ASSFILE，读取失败立即返回，转换后统一释放；读取 OOM 关闭输入 FILE，节点失败释放文本。 | `test_error_nonexistent_ass` 精确检查 FILE_NOT_FOUND 和不创建输出；再补读取故障注入与重复转换内存验收。 |
| REV-014 / P1 | ASS 子函数 OOM/写错误自行 fclose，外层仍写统计并再次 fclose；延迟 flush/close 错误被漏报。 | [ass/file.c](../../lib/src/ass/file.c) 由创建 FILE 的 `writeAss` 独占关闭；子函数仅释放自身资源并返回；失败停止后续写入；检查 fflush/fclose。 | 静态确认子函数不再 fclose；未来用满磁盘/失效输出与故障注入验证正确错误码、无二次关闭。 |
| REV-015 / P1 | 特殊弹幕文本可以超过 1024 字节，但用 strcpy 复制到固定 ASS 转义缓冲区，导致栈越界。 | [ass/file.c](../../lib/src/ass/file.c) 特殊文本直接使用原字符串输出；普通文本继续走有界 assEscape。 | `test_long_special_ass_roundtrip` 使用 1500+ 字节 SP 文本，检查内容完整；未执行。普通长文本转义截断仍需独立验收。 |
| REV-016 / P1 | 正显示比例乘低分辨率截断成 0 行；拥挤 fallback 的正序下标含义错误，底端把像素索引当底边坐标；可能访问 `array[-1]`。 | [ass/file.c](../../lib/src/ass/file.c) 区域至少 1 像素；findMin 正序包含合法终点；底端使用 `height-1` 查找并把结果 +1 转为底边坐标。 | `test_small_display_area_collision` 覆盖极小比例及滚动/顶部/底部拥挤；未执行。其它几何溢出仍见 REV-054。 |
| REV-017 / P2 | FreeType 不可用或初始化失败时 textLen 仍是 0，后备只在负数时触发；分段测量失败可能被其它成功宽度抵消。 | [ass/file.c](../../lib/src/ass/file.c) textLen 初始 -1，任一正文/用户名/分隔符测量失败后整体后备；分隔符按实际输出类型选取。 | 必须分别验收精确模式、未注册字体和无测量支持模式的 move 端点与碰撞。未声称像素与 libass 一致。 |
| REV-018 / P2 | 统计函数参数以毫秒使用，调用方却直接传秒数，统计存活区间及结果错误。 | [ass/file.c](../../lib/src/ass/file.c) 调用统计前使用 ASS 毫秒转换。 | 后续用已知 start/duration 样本检查统计与普通字幕的结束时间一致；当前仅单位追踪。 |
| REV-019 / P1 | 可表示的 start 和 duration 分别合法，相加仍可溢出；统计额外 30 秒和消息动画 250ms 也需要空间。 | [core/process.c](../../lib/src/core/process.c) ASS 写前用 int64 组合检查 start、各类型 duration、move+pause、统计尾部和动画，拒绝超界且不创建输出。 | `test_ass_end_time_overflow` 验证近上界 XML→XML 可保留、XML→ASS 必须拒绝；未执行。时间仍以 int 毫秒表示，未扩展为无限范围。 |
| REV-020 / P1 | 秒→毫秒 float 中间舍入靠近 INT 上界可能越界；历史 ASS 时间 parser 数字累积可能溢出；偏移继续用 float。 | [base/define.h](../../lib/src/base/define.h) 转换宏使用 double 中间值；[ass/string_processing.c](../../lib/src/ass/string_processing.c) 使用有界 strtol/strtod；[ass/file.c](../../lib/src/ass/file.c) 检查事件起止时间及 double 偏移，核心映射格式错误。 | 验收长小时字段、非法分钟/秒、近边界小数、超界偏移、end<start；输入错误与 OOM 必须分开。 |

`validate_ass_time_bounds` 针对 ASS 输出的时间组合约束；不能把它当作所有格式、所有几何计算都已安全的证明。`MAX_ASS_MS_INT` 是现有有限毫秒模型的一部分。输出 XML/JSON 与输出 ASS 的可接受范围可能不同，调用方应收到明确失败。

## JSON/XML 数据处理

以下 `REV-021` 至 `REV-027` 的源码状态均为：**已改源码，未运行验证**。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-021 / P1 | JSON writer 把复用的栈 c_buf 值交给可能保留指针的 mutable 文档，多条记录会覆盖前条；部分创建/flush 错误漏报。 | [json/file.c](../../lib/src/json/file.c) 复制字段字符串并检查 mutable 对象 OOM、写入和 flush。 | 多条不同时间/颜色/类型的 JSON 输出必须分别保留自己的 c 字段；OOM/写失败需故障注入。 |
| REV-022 / P2 | flat 数组或多个记录遍历形态不匹配，只读首条或漏读有效记录。 | [json/file.c](../../lib/src/json/file.c) 明确支持多元素 flat 记录数组及既有包装格式。 | [test_convert.py](../../tests/test_convert.py) 的 JSON 形态回归源码；按数量和内容验收，不能只检查文件非空。 |
| REV-023 / P1 | c 字段宽松读取后直接转 short/int，颜色/类型/字号/时间超界或非有限值引发截断或未定义转换。 | [json/file.c](../../lib/src/json/file.c) 有界数值解析、字段类型/完整性检查、double 偏移后再转毫秒。 | 验收短字段、越界颜色、非整数类型、short 字号上界、巨大时间；任意长 m 文本的估算仍见 REV-054。 |
| REV-024 / P2 | 追加读取已有 pool 时，用最终 pool 非空判断当前文件成功，空/无有效记录的新输入被掩盖。 | [json/file.c](../../lib/src/json/file.c)、[xml/file.c](../../lib/src/xml/file.c) 用当前 loaded_record 计数识别本次加载结果。 | 有效文件后追加空/非法文件应报告当前文件失败，并依既定所有权清理 pool。 |
| REV-025 / P2 | XML raw metadata 把超大 UID 经过 double 造成精度损失；price、duration 非有限或超界后转整数。 | [xml/file.c](../../lib/src/xml/file.c) 保留整数 UID 读取路径，并界定价格、时长转换。 | 超过 2^53 的 UID、uint 上界、非有限/超界价格与时长；确保错误值不污染礼物节点。 |
| REV-026 / P2 | XML text/node 分配失败清理不完整，空文档文件句柄未关闭；重复/失败读取泄漏。 | [xml/file.c](../../lib/src/xml/file.c) 修复 OOM 清理及空文档 fclose，配合核心读取错误所有权。 | 空文档、无有效记录、分配故障与重复读取内存验收；不能只凭单次转换成功确认。 |
| REV-027 / P1 | XML 标签临时内容缺 NUL 或特殊字体字段写入超出目标容量；输入时间偏移仍有窄浮点边界。 | [xml/file.c](../../lib/src/xml/file.c) 明确终止符与字段容量，double 时间范围校验。 | 长标签/特殊字体、缺失字段和边界时间回归源码；用静态界限审查加运行期内存工具验收。 |

## 字体、测宽与 FFI 基础

以下 `REV-028` 至 `REV-032` 的源码状态均为：**已改源码，未运行验证**。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-028 / P1 | 字体查找负缓存可能存在 face/font 为 NULL 的条目，后续仍进入 shaping。 | [text/measure.c](../../lib/src/text/measure.c) 检查负缓存/NULL，失败向调用方返回不可测量。 | [test_text_measure.py](../../tests/test_text_measure.py) 对不可用字体等路径的源码；ASS 后备配合 REV-017。 |
| REV-029 / P2 | HarfBuzz/FreeType 引用关系与缓存驱逐销毁顺序不明确，驱逐可能漏释放或破坏仍被使用的 face。 | [text/measure.c](../../lib/src/text/measure.c) 明确 FT 引用与缓存释放。 | 多字体/字号触发驱逐、反复创建销毁上下文及内存检查；当前仅所有权文本复查。 |
| REV-030 / P2 | 非法 pixel size、底层设置失败/OOM、missing glyph 会被当作成功宽度继续使用。 | [text/measure.c](../../lib/src/text/measure.c) 对设置和分配失败返回失败，缺失字形选择后备。 | 构建后覆盖空字符串、非正字号、缺字和字体不可用；配置字号上界还未修。 |
| REV-031 / P2 | 字体集合未使用 FC_INDEX；累计 advance 可能超过 int 后窄化。 | [text/measure.c](../../lib/src/text/measure.c) 传递字体集合 index，以 int64 累计并检查返回范围。 | TTC/集合字体与长文本测宽；估算后备的 int 乘法另见 REV-054，不能认为此处已覆盖整个排布。 |
| REV-032 / P2 | 字体注册接受空或过长名称，NULL 创建契约和“精确等同 libass”描述不准确。 | [text/measure.c](../../lib/src/text/measure.c)、[measure.h](../../lib/src/text/measure.h) 校验 registry 输入，修正文档和不可用/NULL 约定。 | 字体名 UTF-8、空名、超长名与注册失败；完整字幕视觉一致性仍是 REV-060。 |

公开接口的句柄生命周期、TLS 错误状态和 `DF_API` 导出是本轮工作区架构的基础。本轮静态审查确认了这些边界的重要性，但没有执行导出符号、线程或 ABI smoke，不能把头文件契约当作运行证明。相关入口见 [danmakufactory.h](../../lib/include/danmakufactory.h)、[base/error.c](../../lib/src/base/error.c)、[Rust binding](../../bindings/rust/src/lib.rs) 与 [ffi_smoke.py](../../tests/ffi_smoke.py)。

## Slint GUI、多语言绑定与原生装载

以下 `REV-033` 至 `REV-039` 的源码状态均为：**已改源码，未运行验证**。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-033 / P1 | 输入与输出同格式，或不同目录同名输入批处理，输出路径直接同名导致覆盖原文件/彼此覆盖。 | [gui/slint/src/main.rs](../../gui/slint/src/main.rs) `make_output_path` 生成唯一输出名称并保护输入。 | main.rs 中两个测试源码；验收同格式、同名批次、已有输出和非 ASCII 路径。未验证 TOCTOU 或并行外部写入。 |
| REV-034 / P2 | GUI 偏移在 UI 与模型间未即时双向更新，小数被抹掉；无效/NaN 偏移可继续转换。 | [main.rs](../../gui/slint/src/main.rs)、[main.slint](../../gui/slint/ui/main.slint) 修正回写和小数显示并拒绝非法值。 | UI 操作和模型值需真实 GUI 验收；源码存在不等于事件绑定已运行。 |
| REV-035 / P2 | GUI 配置导入丢 inline blacklist，外部黑名单读取失败被静默忽略；坐标和模式兼容不足。 | [gui/slint/src/config.rs](../../gui/slint/src/config.rs) 保留列表，读失败报错，兼容数组/字符串坐标和数字/大小写模式。 | config.rs 两个测试源码；与核心 nested/flat 配置逐项往返核对。 |
| REV-036 / P2 | Rust build script 用宿主平台判断目标路径，交叉编译 OS/arch/profile 选择错误；缺 links 元信息。 | [bindings/rust/build.rs](../../bindings/rust/build.rs)、[Cargo.toml](../../bindings/rust/Cargo.toml) 使用 Cargo target OS/arch 与 links metadata。 | 宿主和目标不同组合、debug/release、显式库路径；真实路径列表缺口见 REV-051。 |
| REV-037 / P2 | 原生链接依赖、DLL 与 rpath 不完整，binary/tests/examples 装载位置不同。 | [bindings/rust/build.rs](../../bindings/rust/build.rs) 补 yyjson/libm 等链接与 Windows 库目录 DLL staging，并跳过复制同文件；[cli/build.rs](../../cli/build.rs)、[gui/slint/build.rs](../../gui/slint/build.rs) 添加 binary rpath。 | 只证明源码有所改善；DLL 其它目录仍见 REV-052，静态闭包见 REV-057，GUI 打包见 REV-056。 |
| REV-038 / P2 | C# 错误字符串按平台默认编码解码，Lua 控制字符/数组判断不稳，Python 部分初始化后析构不安全。 | [C# binding](../../bindings/csharp/DanmakuFactory.cs) 按 UTF-8 解码；[Lua binding](../../bindings/lua/danmakufactory.lua) 完整 JSON 控制字符转义、数组判断不依赖迭代顺序；[Python binding](../../bindings/python/danmakufactory.py) 部分初始化析构安全。 | 各语言已有测试源码未运行；验收 Unicode 错误、Lua 字符与键顺序、构造失败后析构。 |
| REV-039 / P2 | JS 把 Emscripten classic script 当 ESM default；Windows file URL/default WASM 路径不稳，字体名转 ASCII 后可能碰撞；console.assert 不使测试失败。 | [JS binding](../../bindings/javascript/danmakufactory.mjs) 改 classic loader、file URL 与默认 docs WASM 路径、Unicode 字体命名；[test_binding.mjs](../../bindings/javascript/test_binding.mjs) 使用 strict assert。 | Node/browser 两环境和 Unicode 字体测试；本轮未运行，既有 WASM 版本也未同步重建。 |

Flutter 另一个仓库的实现不在本轮修改范围。Slint 与 Flutter 的框架比较、C ABI 多前端路线属于相关架构讨论，不能据此认为 Flutter 端已经与当前 API/配置行为对齐。

## Rust CLI 与浏览器页面

以下 `REV-040` 至 `REV-045` 的源码状态均为：**已改源码，未运行验证**。其中兼容行为只修到表内描述的范围，完整旧 CLI 契约保留在 REV-055。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-040 / P1 | 缺输出值直接索引引发 panic；多个 `-i` 分组模板跨组传递，格式名与文件名混淆，`-t` 归属错误。 | [cli/src/main.rs](../../cli/src/main.rs) 输出解析改 Result；输入按 `get_occurrences` 分组并保留全局参数索引，偏移按最近输入归属；显式拒绝空/过多/格式错误输出。 | main.rs 新增 Rust 测试源码；验收单独 `xml` 文件名、分组模板、多个偏移、缺输出值和错误格式。 |
| REV-041 / P2 | 配置非对象/section 非对象 panic 或被忽略；混合 wrapper、flat、nested 和多文件顺序造成后项无法覆盖前项。 | [cli/src/main.rs](../../cli/src/main.rs) 配置读取/JSON/section 错误致命，支持 BOM；逐文件拆 wrapper、只规范化 keys、统一 nested/flat/alias 后依顺序 merge。 | 多文件混合模式与 malformed config；值必须保留原大小写。四个 Rust 测试源码未运行。 |
| REV-042 / P2 | save/ignore-warnings 原先无效或覆盖行为不符合参数含义；某些旧下划线参数不识别。 | [cli/src/main.rs](../../cli/src/main.rs)、[cli/src/cli.rs](../../cli/src/cli.rs) 转换成功后保存本次有效合并覆盖到 `DanmakuFactoryConfig.json`，新增 `--block_mode`/`--statistics_mode` 别名。 | 此 save 不是完整默认配置，不能无输入/输出独立保存；只新增了两个别名，未标其它旧别名全部兼容。 |
| REV-043 / P2 | 输出/config 文件覆盖缺少有效约束，非交互环境可能卡住或静默覆盖；输出与 save 文件相同可互相破坏。 | [cli/src/main.rs](../../cli/src/main.rs) `--ignore-warnings` 允许覆盖；否则交互询问或非 TTY 报错；canonical path 防止 output 与 save config 相同。 | 交互/非交互/已有输出/save 同路径；未来测试不得默认交互才能通过。 |
| REV-044 / P2 | CLI f32 接受 NaN/Inf，路径和字符串全局转换破坏值；Unix 反斜杠可以是合法文件名；黑名单读取失败被吞。 | [cli/src/main.rs](../../cli/src/main.rs) 所有 f32 参数拒非有限；保留 Unix 反斜杠和字符串值；blacklist 读失败致命。 | 包含反斜杠/Unicode/大写路径以及非有限偏移；避免用 Windows 语义统一重写 Unix 文件名。 |
| REV-045 / P2 | Web 使用 classic script 的错误加载方式；resolution 对象不符合 API 数组 schema；转换后切换 format 会错误标记下载；注册字体 family 与配置不一致。 | [docs/app.js](../app.js)、[docs/index.html](../index.html) 从 window factory 加载，resolution 改数组，下载记录实际转换格式，导入字体 family 与 config 一致；提示改为字形测宽与后备。 | 真实浏览器加载/转换/切换格式/Unicode 字体导入；本轮未浏览器验证，预览能力不能从提示文字推定已实现。 |

## 构建、CI 与发布入口

以下 `REV-046` 至 `REV-050` 的源码状态均为：**已改源码，未运行验证**。

| ID / 级别 | 触发、影响与根因 | 已实施的修复与源码 | 回归源码 / 验收 / 残余风险 |
| --- | --- | --- | --- |
| REV-046 / P2 | 项目 basename 依 checkout 名变化，脚本/语言绑定依赖固定库名；WASM 不应依赖原生 Fontconfig；typed API 导出遗漏。 | [xmake.lua](../../xmake.lua) 固定 `PROJECT_NAME=DanmakuFactory`，非 WASM 才使用 Fontconfig，补 typed WASM exports。 | 变更目录名、native/WASM target、typed 调用；现有 WASM 二进制未重建，见 REV-059。 |
| REV-047 / P2 | Cargo 与 xmake targetdir/kind/profile/arch 不一致，run 与 build 的 release/musl 模式不一致，未先确保 C 核心。 | [scripts/rust.lua](../../scripts/rust.lua) 传实际 C target 目录和 kind，校正 musl arch/profile，先 core；输出原生包搜索和运行时目录列表。 | explicit target dir、debug/release、musl/非 musl；生产者修复而消费者未闭环，见 REV-051/052。 |
| REV-048 / P2 | CLI 发布包沿用旧二进制入口，Cargo/musl/custom targetdir 定位和 Windows DLL 打包不足。 | [scripts/package.lua](../../scripts/package.lua) 实现 `package_release_cli` 与 Cargo 产物打包定位，[scripts/rust.lua](../../scripts/rust.lua) 协调调用及 target/profile 路径；release workflow 使用新 CLI 入口、Rust setup、release configure 和 package-cli。 | 在干净 checkout 与解压目录验收；静态传递依赖和 GUI 发行仍未完成。 |
| REV-049 / P2 | 构建 workflow 分支/Alpine 工具/Rust 环境不足，Windows step 被误插 Unix 安装命令。 | [.github/workflows/build.yml](../../.github/workflows/build.yml) 支持 main/master，补 Alpine bash/file/curl 和 stable rustup；修复 Windows 步骤；相关 release workflow 同步入口。 | CI YAML 仅文本审查，未触发 CI；需要各 runner 上真实 build/package 证据。 |
| REV-050 / P2 | 原测试只看非空文件、console.assert 或示例，容易漏结构、错误路径和 ABI 边界。 | [test_convert.py](../../tests/test_convert.py)、[test_config.py](../../tests/test_config.py)、[JS binding tests](../../bindings/javascript/test_binding.mjs)、GUI Rust 内嵌测试扩充有意义断言。 | 本项只是测试源码改善；所有新测试未执行。构建和运行验证明确归 REV-063。 |

## 待处理项的详细责任与验收

### REV-051：多路径原生依赖搜索（P1）

- **触发**：xmake 向 `DF_XREPO_LIB_DIR` 导出多个库目录，Windows 使用分号、Unix 使用路径列表分隔符。
- **影响/根因**：`bindings/rust/build.rs` 仍输出一次 `cargo:rustc-link-search=native={整串}`；它把路径列表当目录，静态链接尤其易失败。真实文件没有 `sys/` 子目录。
- **源码状态**：生产者已改源码，未运行验证；消费者待处理。
- **修复 checkpoint**：用 `env::split_paths` 解析 `OsString`；逐个路径输出 search metadata；空值和无效路径策略一致；`rerun-if-env-changed` 覆盖相关变量。
- **验收**：两个以上目录分别放核心与依赖，包含空格/Unicode 路径；验证 target/profile 和平台分隔符；检查实际链接命令及正确库来源。

### REV-052：运行时 DLL 目录列表（P1）

- **触发**：Windows 共享库依赖位于 xrepo/package 的多个运行目录，而非核心 DLL 所在目录。
- **影响/根因**：当前 sys 只扫描 `lib_dir`，`DF_RUNTIME_DLL_DIRS` 完全未读；链接可能成功，运行仍因缺第三方 DLL 失败。
- **源码状态**：脚本提供环境变量；消费者待处理，现有 profile/deps/examples staging 只能覆盖已扫描目录。
- **修复 checkpoint**：解析目录列表、去重源目录，定义同名 DLL 冲突策略，复制前避免同文件；对 Cargo binary/test/example 和 GUI 发布包都明确目的地。
- **验收**：不依赖开发机 PATH，在干净解压目录启动；核对全部原生依赖和对应版本，不能用“主 DLL 存在”当作完成。

### REV-053：历史消息动画分配与错误传播（P1）

- **触发**：`getNewDlgListPtr`、COMBOLIST、MSGPTRLIST 或相关消息链分配失败。
- **影响/根因**：[ass/file.c](../../lib/src/ass/file.c) 多处 malloc 后直接写字段；`amendMessage/appendMessage/writeAliveMessage` 等多层 void helper 没有失败通道，部分链又可能已修改。此为继承问题，不能说本轮新引入。
- **源码状态**：待处理。本轮修复了其它 ASS OOM 和 FILE 所有权，但没有重构整条动画 OOM 链。
- **建议方案**：分配函数返回可检查结果；动画 helper 返回统一错误码；提交节点前完成必要分配；单一拥有者在错误出口释放消息链/对话链/临时排序链；避免在半提交状态继续动画。
- **验收**：依次故障注入每个分配点，检查无 NULL 解引用、悬空链、泄漏、二次释放；不写入成功状态，原生错误可被 FFI 查询。

### REV-054：几何、字号和估算宽度（P1）

- **具体缺口**：`text.font_size/msgbox.font_size` 没有上界；`getStrHei` 仍做 int 字号映射；`getStrLen` 以 int 计数字符并相乘；JSON m 字符串未限定长度；ASS 的 `textHei + line_spacing`、`textLen + resolution.x`、用户名各段宽度相加可能溢出。
- **源码位置**：[config/validate.c](../../lib/src/config/validate.c)、[base/string.c](../../lib/src/base/string.c)、[ass/file.c](../../lib/src/ass/file.c)、[text/measure.c](../../lib/src/text/measure.c)。
- **已修与未修边界**：精确测宽 int64 advance 上界、配置 duration 上界、ASS 时间组合、至少一行和 findMin 边界已修；它们没有修上述几何组合。提议字号限制至 SHRT_MAX、size_t/double 估算和宽整数行距仍未落盘。
- **方案选择**：先定义可接受字号/尺寸范围及有效字号非正时的行为；计算用宽中间值，再选择有明确语义的拒绝或钳制；失败必须有独立错误码，不能随意映射为 OOM。
- **验收**：INT_MAX 字号、INT_MIN/MAX 行距、超大分辨率、长文本、大输入字号、小配置字号、用户名长文本组合；同时检查 move、pos 和数组下标，不只看返回码。

### REV-055：CLI 配置打印、保存与旧参数契约（P2）

- **现状**：`-c` clap `num_args` 仍从 1 开始，不能零参数打印默认配置；没有完成文档所述最终 merge 配置打印。`--save` 只在转换成功后保存“有效合并覆盖”，不能在无输入/输出时独立保存，也不等同含全部默认项的最终配置。
- **旧名清单**：文档中的 `--scroll_duration`、`--fixed_duration`、`--font_size`、`--display_area`、`--scroll_area` 等下划线旧名尚未逐项闭合。仅能确认本轮增加 `--block_mode` 和 `--statistics_mode`。`-r 1920x1080` 与 `-r 1920 1080` 双形式也需要明确兼容与验收。
- **源码/文档**：[cli/src/cli.rs](../../cli/src/cli.rs)、[cli/src/main.rs](../../cli/src/main.rs)、[CLI 参数说明](../DanmakuFactory%20CLI%20参数说明.md)。
- **修复 checkpoint**：生成逐项兼容矩阵；明确打印/保存的完整配置模型；决定是否支持无转换的配置操作；以现有文档和公开 API 为兼容性基准完成实现；只有确有歧义时才向用户确认，不通过缩减文档恢复表面兼容。
- **验收**：旧 CLI 的典型命令、`-c` 零/单/多参数、wrapper 混合 merge、无 input/output save、默认值 round-trip、全部旧 alias 和两种坐标形式。

### REV-056 至 REV-059：发行和 WASM 闭环（P2）

- **REV-056 GUI**：源码修复与 GUI 可发布不同。需要 [gui/slint/Cargo.toml](../../gui/slint/Cargo.toml)、[build.rs](../../gui/slint/build.rs)、xmake tasks、CI、包内容以及干净目录启动共同完成；字体和所有 DLL/系统库均须核对。
- **REV-057 静态包**：当前 Rust build script 手写了 yyjson、字体栈、PCRE2、Expat 和压缩等传递库；名单存在不代表闭包、链接顺序或平台系统库已正确。建立每 target/configuration 的实际依赖证据，再验证独立运行与许可/分发内容。
- **REV-058 WASM 正则**：当前 [core/list.c](../../lib/src/core/list.c) 的 `__EMSCRIPTEN__` 路径不走原生 PCRE2，正则开关不能默默产生与原生同等能力的期待。选择明确拒绝/提示、引入同等实现或公开能力差异，并比较结果。
- **REV-059 WASM 产物**：[docs/wasm/danmakufactory.js](../wasm/danmakufactory.js) 与 [danmakufactory.wasm](../wasm/danmakufactory.wasm) 是已有产物，本轮未重建。[wasm_smoke.mjs](../../tests/wasm_smoke.mjs) 是待执行的验收入口；需同时核版本、API 导出、JS 包装和 Web 入口。

### REV-060 至 REV-064：架构与验证 checkpoints

- **REV-060 文本一致性**：FreeType/HarfBuzz 字形测宽及 fallback 是测量策略，不是完整 libass 渲染。验证 mixed scripts、TTC、emoji/缺字、fallback 字体、换行、字体缩放、描边及粗体后，再选择与渲染引擎共享模型或引入 libass。
- **REV-061 配置单一来源**：调查历史 [config/file.c](../../lib/src/config/file.c) 的 `readConfig/writeConfig` 与新 parser/CLI/GUI 的调用者；共享 schema、合并、验证和序列化，避免默认值/保存形态随入口变化。需要兼容计划再改接口。
- **REV-062 Web 能力**：PWA、离线资源缓存、实际字幕预览未完成；classic loader 修复不等于旧浏览器可用。以明确浏览器/版本、下载后离线、CSP 和真实字幕视觉结果验收，不能复用 Node 加载成功充当浏览器证明。
- **REV-063 验证**：当前只有文本证据；未来依次进行 C/API 静态与共享库、ctypes、Rust CLI/GUI、其它绑定、WASM/浏览器、发行包验收。是否开启这些操作由后续用户指令决定，本轮不执行。
- **REV-064 模块职责**：把格式解析、数据所有权、排布算法、消息动画和统计分开；建立 C ABI 统一错误/配置契约，让前端只规范输入与展示。先修已确认危险边界，再做单独、可回退的重构。

### REV-065：公开 API 与字体注册的并发契约（P2）

- **触发/根因**：[danmakufactory.h](../../lib/include/danmakufactory.h) 的线程安全说明仍声称库内部没有任何跨调用的全局可变状态；[measure.h](../../lib/src/text/measure.h) 描述的字体注册实际使用全局表，要求启动阶段完成注册，并在并发转换期间禁止修改。
- **影响**：调用者可能把所有 API 都视为无需同步，注册阶段与转换阶段的约束在两个头文件间不一致。当前仅确认契约冲突，没有据此证明遵守注册约束的并发转换本身存在数据竞争。
- **源码状态**：待处理。字体注册输入与内部缓存边界修复不等于公开并发说明已修正。
- **修复 checkpoint**：限定“无全局可变状态”断言的适用范围，统一初始化/注册/转换阶段与外部同步要求；若未来允许动态注册，再实现对应同步或上下文隔离。
- **验收**：头文件、语言绑定说明和架构文档描述一致；并发测试应分别覆盖允许的独立转换和明确约束的注册时机。
### REV-066：原生正则编译、分配与匹配失败策略（P2）

- **触发/根因**：[core/list.c](../../lib/src/core/list.c) 的 `blockByType` 在 `pcre2_compile` 返回 NULL 时继续，注释说明“静默跳过该条目”；后续未找到已编译条目时还可能退化为字面字符串匹配。`pcre2_match_data_create_from_pattern` 返回 NULL 时直接 continue。`pcre2_match` 的负返回值没有区分正常 `PCRE2_ERROR_NOMATCH` 和其它匹配运行错误。
- **影响**：内存不足、输入编码问题或其它实际匹配错误可能导致漏过滤，最终函数仍返回成功；调用者无法判断过滤是否完整执行。无效规则的字面降级也与用户开启正则后的期待可能不同。此项针对原生 PCRE2 路径，和 REV-058 的 WASM 能力差异分别追踪。
- **源码状态**：待处理。REV-011 只修了 regCodes/regCodeMap 双 realloc 的所有权；它没有覆盖 PCRE2 编译、匹配数据分配和运行错误的全部失败路径。
- **兼容策略 checkpoint**：区分无效规则与编译阶段分配失败。无效规则应警告、拒绝还是按既有策略字面降级，需要以现有文档/API 行为为基准明确；存在歧义时向用户确认。实际分配失败与匹配运行错误建议向调用者报告，并停止把该次过滤标为完整成功；正常 NOMATCH 继续检查其它规则。
- **所有权 checkpoint**：在错误传播前释放已编译规则、映射数组及匹配数据，保留调用者对 keyStrings 和弹幕池的所有权；不要为了报告错误引入二次释放或半销毁链表。
- **验收**：有效但不匹配的规则仍成功返回；无效规则得到已确定的警告/拒绝/降级行为；故障注入覆盖 compile、match-data 创建和实际 match 失败；错误码/消息可经 FFI 读取，过滤失败不被记为成功。当前没有执行这些验收。
## 回归源码与未来验收顺序

| 层级 | 当前源码入口 | 重点验收 | 本轮执行状态 |
| --- | --- | --- | --- |
| C FFI / 生命周期 | [ffi_smoke.py](../../tests/ffi_smoke.py) | 符号、UTF-8、TLS 错误、create/configure/run/destroy、重复失败后复用。 | 未运行。 |
| 配置 | [test_config.py](../../tests/test_config.py) | nested/flat/alias、case-preserving keys、坏结构、mode 窄写、坐标、finite/时长边界、原子加载。 | 已增加源码，未运行。 |
| 转换 | [test_convert.py](../../tests/test_convert.py) | JSON/XML 结构、追加空文件、缺 ASS、两节点降序、长特殊 ASS、极小区域拥挤、ASS 终点溢出。 | 已增加源码，未运行。 |
| 测宽 | [test_text_measure.py](../../tests/test_text_measure.py) | 不可用/缺字体/负缓存、字号、集合字体、缓存释放、超长 advance、后备。 | 未运行；视觉对比尚需设计。 |
| Rust CLI / GUI | [cli/src/main.rs](../../cli/src/main.rs)、[gui main.rs](../../gui/slint/src/main.rs)、[gui config.rs](../../gui/slint/src/config.rs)、[Rust integration](../../bindings/rust/tests/integration.rs) | 分组参数、输出错误、混合配置、输出唯一性、偏移回写、GUI blacklist。 | 内嵌回归源码已增加，未运行。 |
| 多语言绑定 | [Python](../../bindings/python/test_binding.py)、[JS](../../bindings/javascript/test_binding.mjs)、[Lua](../../bindings/lua/test_binding.lua)、[C#](../../bindings/csharp/TestBinding.cs)、[Dart](../../bindings/dart/test/test_binding.dart) | ABI 签名、错误编码、JSON 边界、加载路径、析构。 | 未运行。 |
| Web / WASM | [wasm_smoke.mjs](../../tests/wasm_smoke.mjs)、[app.js](../app.js)、[index.html](../index.html) | 新产物导出、classic factory、typed 格式、字体导入、下载格式、浏览器差异。 | 未重建、未运行、未浏览器验收。 |
| 发布 | [xmake.lua](../../xmake.lua)、[scripts/rust.lua](../../scripts/rust.lua)、[build workflow](../../.github/workflows/build.yml) | 平台/profile/targetdir、static/shared、路径列表、运行依赖、干净解压启动。 | 仅文本审查；未 build/package/CI。 |

历史相关对话中曾记录 Node 的 16 项 smoke 与 JS 绑定成功，它们针对当时既有 WASM 产物，且属于历史记录。不能把那次结果写成本轮 C/Rust/GUI/Web 源码通过测试，也不能推导旧浏览器或本轮重新构建产物已验证。

## 台账更新规则

1. 为每个 checkpoint 保留触发、影响、根因、责任模块、源文件和具体验收；“可能需要优化”不是可关闭的事项。
2. 源码更改和运行验收分别记录；若没有运行证据，继续标“已改源码，未运行验证”。
3. 后续变更在原 ID 下追加日期、具体提交或产物、执行命令/环境与结果；不覆盖本轮“仅 Text”的历史事实。
4. 待处理项修复后保留其原始边界和残余风险。若需求不再需要，应记录用户决策与契约调整，不直接删除台账。
5. 本文采用源码相对链接，不固定行号；源码后续移动时更新链接，避免过时行号使复核产生错位。
