# SceneForge 产品规范

本目录按产品 topic 组织 RFC。每个 topic 的 `spec.md` 是该领域当前版本的 RFC，RFC 编号保留在文档元数据与标题中；文档元数据中的状态决定它是讨论稿还是已确认基线。topic 专属的实施任务与该规范放在同一目录。

## 目录规则

- `specs/<topic>/spec.md`：该 topic 当前版本的规范；确认基线后，以此作为需求来源。
- `specs/<topic>/tasks.md`：与该 topic 对应的开发任务拆分；没有专属任务时不创建。
- RFC 之间使用相对链接，并以 RFC 编号作为跨文档引用依据。
- 全局技术架构、开发规范和阶段路线放在 `docs/development/`；视觉设计放在 `docs/design/`。
- 暂不为常规文档优化或实施细化创建变更档案。发生已确认需求变更时，再按实际需要增加变更提案与归档流程。

## Topic 索引

| Topic | RFC 文档 | 任务 | 范围 |
|---|---|---|---|
| `001-product-overview` | [RFC-001 产品总纲](001-product-overview/spec.md) | — | 产品目标、全局范围、通用需求与 RFC 索引 |
| `002-story-and-script` | [RFC-002 作品、故事与剧本](002-story-and-script/spec.md) | [开发任务](002-story-and-script/tasks.md) | 作品结构、创作简报、创作圣经、故事和剧本 |
| `003-storyboard-and-visual` | [RFC-003 场次、分镜与视觉资产](003-storyboard-and-visual/spec.md) | — | 场次、镜头、视觉参考与连续性 |
| `004-ai-video-animation` | [RFC-004 AI 视频与动画](004-ai-video-animation/spec.md) | — | 视频、动画及镜头生成 |
| `005-voice-sound` | [RFC-005 对白、配音与声音](005-voice-sound/spec.md) | — | 角色声音、配音、音乐、音效与口型同步 |
| `006-editing-effects` | [RFC-006 剪辑、特效与媒体处理](006-editing-effects/spec.md) | — | 时间线、专业剪辑、效果与媒体工具 |
| `007-subtitles-localization-delivery` | [RFC-007 字幕、语言版本与交付](007-subtitles-localization-delivery/spec.md) | — | 字幕、本地化、多画幅和成片交付 |
| `008-rights-privacy-compliance` | [RFC-008 来源、授权、隐私与内容标识](008-rights-privacy-compliance/spec.md) | — | 素材权利、隐私、AI 来源和发布标识 |

RFC-001 是产品总纲；RFC-002 至 RFC-008 是专题规范。专题规范细化对应领域，不应与总纲产生静默冲突。若需求归属不清或跨多个 topic，应在 RFC-001 和相关专题规范中明确交叉引用。
