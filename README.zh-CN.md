<div align="center">
  <img src="snow_shot/resources/app-icon.svg" width="112" height="112" alt="天蓝色 M 图标">
  <h1>小马哥专用截图工具</h1>
  <p>基于 Snow Shot 开发的个人截图工具</p>
</div>

基于 [mg-chao/snow-shot](https://github.com/mg-chao/snow-shot) 修改，保留截图、标注、长截图、录屏、文字识别（OCR）、贴图和翻译等功能。部分截图界面与操作参考 QQ；本项目不是腾讯官方产品，也不是完整的 QQ 截图复刻。

## 个人版调整

- 主界面使用“小马哥专用截图工具”名称，程序、任务栏与托盘统一为天蓝色 M 图标。
- 移除注册、登录、账户页面及原厂账户服务器入口。
- 保留 AI 翻译的自定义 API 地址、密钥和模型配置，直接调用用户配置的服务，无需 Snow Shot 账号。
- 截图默认快捷键为 Windows **Ctrl+Alt+A**、macOS **Control+Option+A**，均可在设置中更改。已有配置中的自定义快捷键和旧版快捷键会保留。
- 调整截图工具栏、圆角与阴影、水印、文字编辑和撤销等操作。
- 关闭上游自动更新，避免上游安装包覆盖个人版修改。

## 下载与安装

| 平台 | 完整安装包 | 下载大小 |
| --- | --- | --- |
| Windows x64 | [小马哥专用截图工具 · Windows x64 离线安装包](https://github.com/stdeven521/xiaomage-screenshot/releases/download/personal-v1.5.4-windows.2/XiaoMaGe-Screenshot-1.5.4-windows-x64-offline.exe) | 59270049 字节（56.52 MiB） |
| Mac：Apple Silicon M1 及之后，macOS 15+ | [小马哥专用截图工具 · Mac M1及更新芯片](https://github.com/stdeven521/xiaomage-screenshot/releases/download/personal-v1.5.4-macos.2/XiaoMaGe-Screenshot-1.5.4-macos-arm64.dmg) | 66239214 字节（63.17 MiB） |

两个安装包均为完整个人版，使用天蓝色 M 图标，无注册登录环节，并保留自定义 API 的 AI 翻译。程序与 OCR 运行组件随包提供；调用 AI 翻译仍需访问自己配置的 API 服务。

下载文件使用兼容 GitHub 的 `XiaoMaGe-Screenshot` 名称，下载页和安装后的应用显示“小马哥专用截图工具”。首次正常打开主窗口时，可在确认窗口选择开机/登录启动：新用户的复选框默认勾选，也可以取消勾选；已有明确关闭的选择会保留，以后仍可在设置中更改。

发布说明、SHA-256 校验文件和构建验证记录分别见 [Windows 版发布页](https://github.com/stdeven521/xiaomage-screenshot/releases/tag/personal-v1.5.4-windows.2)和 [Mac 版发布页](https://github.com/stdeven521/xiaomage-screenshot/releases/tag/personal-v1.5.4-macos.2)。下载大小是安装包本身的大小，安装后磁盘占用会更大。

### Windows

1. 下载并运行上表的 x64 离线安装器，按向导选择安装位置并完成安装。安装许可页面包含个人版来源说明，并保留完整 GPL 原文。
2. 从开始菜单或桌面快捷方式打开“小马哥专用截图工具”。
3. 使用 **Ctrl+Alt+A** 截图，也可在应用设置中更改快捷键。AI 翻译需在设置中填入自己的 API 地址、密钥和模型。

### Mac

Mac 安装包仅支持 **Apple Silicon M1 及之后的芯片**，要求 **macOS 15 或更新版本**。

1. 打开 ARM64 DMG，可先查看包内的“许可说明.txt”，其中包含个人版来源说明及完整 GPL 原文。将“小马哥专用截图工具”拖入“应用程序”文件夹，再从“应用程序”打开。
2. 此个人版采用临时签名（ad-hoc），尚未通过 Apple 公证。首次打开若被系统拦截，请在“系统设置 → 隐私与安全性”中找到本应用的提示，点击“仍要打开”并确认。该操作仅允许本应用；具体步骤见 [Apple 官方说明](https://support.apple.com/zh-cn/102445)。
3. 首次截图或录屏时，按系统提示授予屏幕录制权限；使用麦克风或摄像头时，再授予相应权限。
4. 使用 **Control+Option+A** 截图，也可在应用设置中更改快捷键。AI 翻译需在设置中填入自己的 API 地址、密钥和模型。

Mac 发布检查覆盖 ARM64 CI 的针对性测试、包内启动与分发探针、OCR 运行组件及签名完整性；这些自动检查不代表已在用户的 Mac 上实测所有截图、录屏和系统权限交互。具体构建提交和检查结果以发布页附带的记录为准。

## 构建与使用

直接使用可下载上方安装包；需要自行修改时，可从本仓库构建。以下是 Windows 开发构建示例，Mac 构建步骤见 [macOS 指南](docs-macos-build.md)。

```powershell
git clone --recurse-submodules https://github.com/stdeven521/xiaomage-screenshot.git
cd xiaomage-screenshot
./scripts/bootstrap.ps1
./scripts/build.ps1 -Preset windows-msvc-debug -Target snow_shot
./scripts/run-snow-shot.ps1
```

Windows 构建需要 Visual Studio 2026 / MSVC、CMake 4.2+、Rust 1.97.1，以及仓库管理的 Qt 和 vcpkg 依赖。具体版本、环境准备和平台注意事项见[开发指南](docs/development.md)、[仓库构建约定](AGENTS.md)、[Windows ARM64 指南](docs-windows-arm64-build.md)与 [macOS 指南](docs-macos-build.md)。开发构建产物不能直接视为可分发安装包。

## 上游与许可

感谢 Snow Shot 作者 mg-chao 及各依赖项目的贡献。原项目介绍和功能文档保留在 [README.upstream.md](README.upstream.md) 与[上游中文说明](README.upstream.zh-CN.md)；其中的发布下载链接指向上游版本。

保留原项目的版权、许可和第三方声明，修改不会改变原有许可范围：

- 主应用及相关组件使用 [GPL-3.0-or-later](snow_shot/LICENSE)。
- `ant_design_qt`、`snow_draw_engine_qt`、`snow_memory` 等组件分别保留各自的 [Apache-2.0](ant_design_qt/LICENSE)、[Apache-2.0](snow_draw_engine_qt/LICENSE)、[Apache-2.0](snow_memory/LICENSE) 许可。
- 完整范围见[仓库许可说明](LICENSE.md)、[应用版权声明](snow_shot/COPYRIGHT)与[第三方声明](snow_shot/THIRD_PARTY_NOTICES.md)。嵌套第三方组件以其自身声明为准。
