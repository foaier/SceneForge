---
title: 技术决策记录
doc_id: DEV-007
version: 0.7-draft
date_created: 2026-11-27
last_updated: 2026-11-28
status: discussion-draft
---

# 技术决策记录

## 已确认的产品/技术边界

| 编号 | 决定 | 来源 |
|---|---|---|
| ADR-001 | 桌面界面使用 Qt 6、C++20、QML/Qt Quick | 用户确认 |
| ADR-002 | 用户可在设置中指定作品资料库位置 | 用户确认“自定义设置目录” |
| ADR-003 | 文本、图片等能力分别配置 AI 服务；单任务可以临时换服务 | 用户确认 |
| ADR-004 | 首阶段侧重剧本、分镜、参考图和基础媒体探测；视频生成后置 | 用户此前确认的首发产物和本轮开发范围 |
| ADR-005 | RFC-001 至 RFC-008 的长期产品范围保留；首阶段排序不取代 RFC 优先级 | 产品基线解释 |
| ADR-006 | C++ 代码遵循标准 C++20；代码注释使用中文；每个源文件保持单一、内聚的职责 | 用户确认的开发规范 |
| ADR-007 | 源文件使用 UTF-8；命名风格保持一致；错误不得静默忽略、日志不得记录敏感内容；按领域和外部边界编写测试；第三方依赖固定版本并审查许可证 | 用户确认的开发规范 |
| ADR-008 | 产品中文名为“镜造”，英文名为“SceneForge”；中文用户界面和中文文档以“镜造”为主称呼，仓库名及英文标识保留“SceneForge” | 用户确认的产品命名 |
| ADR-009 | 首阶段资料库仅支持本机固定 NTFS 路径和单个应用进程写入；网络共享、同步盘、可移动磁盘、跨卷移动及多进程并写不支持 | TASK-R002-000 存储原型验证 |

## 暂定技术方案

| 编号 | 暂定方案 | 验证方式 |
|---|---|---|
| ADR-T-001 | 模块化单体，领域逻辑与 QML、存储和外部服务分层 | 用项目/剧本/分镜的垂直切片验证依赖方向与测试能力 |
| ADR-T-002 | 首阶段采用每个作品独立目录、SQLite 元数据和作品目录内媒体文件；经原型验证的支持边界为本机 NTFS、单个应用进程写入 | TASK-R002-000 原型验证；其他路径及多进程场景明确列为不支持 |
| ADR-T-003 | 首阶段通过 QProcess 调用 ffprobe/FFmpeg，不静态链接媒体库 | 验证 Windows 打包、取消、非 ASCII 路径、退出码及许可证清单 |
| ADR-T-004 | 文本/图片使用能力适配器；默认服务按能力配置，单任务可覆盖 | 模拟服务契约测试、密钥存储和用户确认流程 |
| ADR-T-005 | 时间线交换可评估 OpenTimelineIO；完整媒体引擎选择不在阶段 1 冻结 | 阶段 0/4 前用真实素材做引擎对比原型 |

## 待决策/需验证事项

- **DEC-001 最低 Windows 版本**：RFC-001 指定 Windows 10/11 优先。Windows 10 Home/Pro 常规支持已于 2025-10-14 结束；产品可兼容 Windows 10，但是否将其列为持续支持平台、以及具体版本，需要确认更新策略后确定。
- **DEC-002 Qt 与 FFmpeg 发行许可**：按实际 Qt 模块、FFmpeg build flags、编解码器和分发方式审查；调用子进程不免除随应用分发二进制的责任。
- **DEC-003 剧本文档库**：格式范围和候选依赖已做第一轮原型调查；PDF 文本提取依赖及真实办公软件互操作尚未通过验证，完整决策保持开放。结论见下文 TASK-R002-016 记录。
- **DEC-004 专业剪辑引擎**：阶段 4 前比较 MLT 和其他成熟方案；FFmpeg CLI 不承担实时时间线编辑器职责。
- **DEC-006 安装与更新**：MSIX、MSI 或其他 Windows 安装方式，自动更新与离线安装策略待发行模式确定后决策。
- **DEC-007 Windows 发行架构**：首发 x64 是否唯一支持，ARM64 是否列入兼容计划。
- **DEC-008 基础媒体预览后端**：阶段 0 比较 Qt Multimedia 与其他已验证解码/播放方案在代表性素材、硬件加速、失败降级和许可证上的表现；FFmpeg/ffprobe 的探测和抽帧能力不能直接作为播放能力结论。

所有待决策项在影响的开发阶段开始前关闭；若决定改变已确认产品需求，先更新相应 RFC。

## TASK-R002-000 存储原型验证结论

- **结论**：首阶段可按 ADR-T-002 实现每个作品独立目录、SQLite 元数据及同目录媒体资产；只承诺本机 NTFS 路径和单个应用进程写入。写入锁、事务迁移、WAL 下的一致快照、恢复到新目录及同卷移动后重开均有原型结果。
- **失败语义**：只读 ACL、SQLite 只读连接、缺失父目录和模拟容量耗尽均产生可见失败；容量失败没有改变已提交数据。产品编辑器仍须保留未保存内容并显示未保存/保存失败，不得把原型错误处理等同于用户界面验收。
- **备份依据**：SQLite `VACUUM INTO` 在含已提交 WAL 内容的活动连接上创建快照；快照通过 `PRAGMA integrity_check`，复制到新目录后可重新打开。首阶段备份实现采用 `VACUUM INTO` 或 SQLite Online Backup API；禁止在 WAL 写入期间只复制主数据库文件。
- **明确不支持**：网络共享、同步盘、可移动磁盘、跨卷移动和同一作品的多进程写入未验证，不列入首阶段支持范围。多连接锁争用原型仅验证同一进程内的 SQLite 连接。
- **验证限制**：容量不足通过 `PRAGMA max_page_count` 模拟数据库满并验证 SQLite 错误与已提交数据保留；没有实际耗尽 NTFS 卷。真实卷空间不足、系统级崩溃和断电恢复仍须在阶段 1 存储集成测试中覆盖。
- **证据**：[`app/prototypes/storage-probe`](../../app/prototypes/storage-probe/README.md)，开发机运行结果：Qt 6.11.2、SQLite 3.53.4、MinGW 15.2.0、NTFS；主原型与拒绝写入 ACL 检查通过。

## TASK-R002-016 剧本文档格式与依赖调查

**检查日期：2026-11-28。状态：部分完成。** 代表性合成样例已建立并验证。当前开发机未安装 Final Draft、Microsoft Word 或 LibreOffice，因此没有这些应用实际导出的互操作文件；PDF 文本提取器和产品分发许可策略也尚未确定。因此本节定义的是暂定支持边界，不表示应用已实现或已通过格式互操作验收。

| 格式/用途 | 首阶段拟定范围 | 样例结果与证据 | 当前结论 |
|---|---|---|---|
| Fountain 导入/导出 | 有限支持 UTF-8 剧本文本、标题页常用字段、分集/段落、场景标题、动作、角色、对白、括号、转场、注释及删稿区；非标准语法和无法映射字段需在转换预览中列明，保留原文件 | 合成样例覆盖上述结构及中文；对照 Fountain 官方语法说明。候选 `screenplay-tools` C++ 解析器 smoke test 得到 12 个语义元素，但未做完整字段对照和往返测试 | 可继续实现窄范围原型；不承诺支持所有方言或所有排版特性 |
| FDX 导入/导出 | 有限支持常用剧本段落和已验证的场景编号；已知支持的扩展信息逐项核对，未识别 XML/属性必须保留原件并列入损失预览 | 合成样例包含 `SceneProperties`、`SceneArcBeats`、`Summary`、`DualDialogue`、页眉动态标签及水印。候选解析器 smoke test 仅产出 9 个基础元素：未读取标题页和场景元数据，双人对白子段落变为空角色；代码将未知类型降为 Action，异常时捕获后静默返回 | 候选不可直接采用；必须有真实应用导出样例验证版本和扩展字段。没有公开官方完整 schema 的结论仅限本次检索 |
| DOCX 导入/导出 | 有限支持语义段落样式、常见标题/对白、基本表格和顺序；复杂排版作为校正线索，不把版面推断伪装成确定剧本结构。输出须使用可理解的语义样式，具体辅助技术可访问性需单独验收 | 合成 OOXML 包包含双栏、带重复表头的表格、场景标题大纲级别、页眉/页脚、分页、修订、超链接及中英文字；.NET 包/XML 检查通过，python-docx 1.2.0 可打开并读出 12 个段落、1 个表格和双栏设置。libzip v1.12.1 静态库与 zlib v1.4 在 MinGW 构建后可读出样例 8 个包成员及 `word/document.xml` 的表格/修订标记。未用 Word/LibreOffice 渲染或屏幕阅读器验证 | OOXML 包处理已有可行候选原型；复杂文档识别、真实软件互操作和可访问性仍待测 |
| PDF 文本导入 | 仅考虑具有可提取文本层的 PDF；按页提取并进入人工校正，阅读顺序/结构无法确定时标为待确认 | 合成最小文本层 PDF 用 pypdf 6.10.0 抽取 206 字符且内容符合预期；Qt `QPdfWriter` 文字绘制 PDF 抽取为 0 字符；图像 PDF 抽取 0 字符且含一个图像对象。Qt PDF 的 `QPdfDocument::getAllText()` API 存在于 Qt PDF 模块文档，但开发机 Qt 6.11.2 未安装该模块，尚未执行 API 验证 | 样例覆盖已完成；产品提取器仍待定，需在真实文本 PDF 上验证所选依赖。不得将视觉绘制文本视为文本层 |
| PDF 输出/扫描件 | 可打印视觉 PDF 与可访问 PDF 是不同范围；扫描图片 PDF 首阶段只保留原件、提示人工整理，不承诺 OCR。未证明带结构标签和辅助技术兼容前，不宣称符合 PDF/UA 或等效可访问性要求 | 本机可生成视觉 PDF；所有样例均无 `/StructTreeRoot`，Qt `QPdfWriter` 绘制文字样例也未被 pypdf 提取。Qt 官方文档将 QPdfWriter 描述为绘图输出设备，没有给出本原型所需的结构化标签验收证据 | 视觉输出可作为候选能力；可访问 PDF 输出状态待定，不能把打印成功等同于可访问 |

### 候选依赖

- **Fountain/FDX：`screenplay-tools`**。公开仓库为 MIT 许可证，C++ 代码可用 MinGW 15.2.0 编译；本次检查提交 `d0bd5587d5bbe395b12ccda2bb277ce93cf4e0a6`（2026-11-18）。候选工程测试因其引用的 `tests` 样例文件缺失而 24 个用例全部无法打开输入；自建 smoke test 发现 FDX 扩展丢失及异常静默问题。暂不作为可直接接入的依赖，Fountain 部分可作为后续对照对象。若未来采用，需保留 MIT 版权与许可文本并固定提交。
- **DOCX ZIP 容器：libzip + zlib**。libzip `v1.12.1` 为 BSD 3-Clause，zlib `v1.4` 使用 zlib license；两者可分别静态构建，组合后能读取合成 DOCX 的 8 个 ZIP 成员并取出 XML。MinGW 15.2.0 下 libzip 首次构建遇到缺少 `DWORD_MAX` 定义，增加构建期兼容定义 `DWORD_MAX=4294967295UL` 后通过；该适配需在正式依赖构建规则中复验。若静态链接，发行包仍须保留两项适用版权/许可声明。它们是当前优先候选，还需验证重复/恶意 ZIP 条目、大小限制、损坏包和实际 Word/LibreOffice 文件。OOXML XML 解析可评估现有 Qt Core 能力，避免仅为 XML 再引入一套解析库；实际选择仍要做打包和许可清单检查。
- **PDF 文本提取：Qt PDF / PDFium 或其他候选**。Qt 6.11 文档说明 Qt PDF 使用 PDFium，且 `QPdfDocument` 提供 `getAllText()`；Qt 的第三方组件清单列出 PDFium、Abseil、FreeType、ICU、libjpeg-turbo、libpng、zlib 等对应许可。必须同时确认 Qt PDF 模块本身在选定发行模式下的授权条件，并随包履行所有适用第三方声明；本机未安装该 Qt 模块，不能据文档 API 直接认定可用。暂不冻结依赖。

本次原型目录为 [`app/prototypes/document-format-probe`](../../app/prototypes/document-format-probe/README.md)。样例和检查脚本不是产品依赖，且未覆盖实际办公软件生成文件、损坏/加密 PDF、文本阅读顺序、OCR、屏幕阅读器或所有 Fountain/FDX 方言。

### 参考资料

- [Fountain 官方语法说明](https://fountain.io/syntax/)；本次访问：2026-11-28。该页面描述语法而非某个应用的完整往返兼容保证。
- [ECMA-376 Office Open XML 文件格式](https://ecma-international.org/publications-and-standards/standards/ecma-376/)；页面列出的最新版本为第 5 版，Open Packaging Conventions 为 2021 年版；本次访问：2026-11-28。
- [Qt 6.11 QPdfDocument](https://doc.qt.io/qt-6.11/qpdfdocument.html)、[Qt PDF 模块](https://doc.qt.io/qt-6.11/qtpdf-index.html)、[Qt 6.11 QPdfWriter](https://doc.qt.io/qt-6.11/qpdfwriter.html) 和 [Qt 使用的第三方许可清单](https://doc.qt.io/qt-6.11/licenses-used-in-qt.html)；本次访问：2026-11-28。在线文档为 Qt 6.11.3，开发机安装为 Qt 6.11.2。
- [`screenplay-tools` 源码与 MIT 许可](https://github.com/wildwinter/screenplay-tools/tree/d0bd5587d5bbe395b12ccda2bb277ce93cf4e0a6)；提交与测试结果见上述记录。
- [libzip 发布页](https://github.com/nih-at/libzip/releases/tag/v1.12.1)与 [BSD 3-Clause 许可文本](https://github.com/nih-at/libzip/blob/v1.12.1/LICENSE)；本次核对：2026-11-28。
- [zlib v1.4 发布页](https://github.com/madler/zlib/releases/tag/v1.4)与 [zlib license](https://github.com/madler/zlib/blob/v1.4/LICENSE)；本次核对：2026-11-28。
- [Open FDX Toolkit 的公开样例](https://github.com/sfingali/open-fdx-toolkit/tree/main/tests/fixtures)仅作为 FDX XML 结构的社区观察来源；其项目声明格式知识经反向分析获得，不视为 Final Draft 官方规范或背书。本次访问：2026-11-28。
