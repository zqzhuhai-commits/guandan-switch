# v0.5.2 全新主题背景

本版使用内置 imagegen 生成三张新的独立游戏场景，复制原始生成文件到项目中；不再用旧版程序绘制的渐变/多边形场景充当主题背景。运行时作为全屏 SDL 纹理载入，所有资源均内嵌进 NRO 和独立 NSP。

| 主题 | 项目资源路径 | 实际程序截图 |
| --- | --- | --- |
| 原色夜景 | romfs/theme-night-v052.png | preview/theme-night.png |
| 深海蓝 | romfs/theme-ocean-v052.png | preview/theme-ocean.png |
| 午夜紫 | romfs/theme-purple-v052.png | preview/theme-purple.png |

生成模式：内置工具，生成新图，无参考图片、无品牌美术。完整提示记录见 [imagegen-v052-prompts.json](imagegen-v052-prompts.json)。三张背景均保留宽阔、低对比的空白桌面供程序绘制牌面和按钮；没有生成 UI、文字、人物、牌或商标。

打包时校验 NRO/NSP 中三张 PNG 与源码的 SHA-256 一致。截图由 v0.5.2 同源 SDL2 桌面可执行文件真实渲染，不是真机截图。
