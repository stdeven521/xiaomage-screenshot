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
- 截图默认快捷键为 **Ctrl+Alt+A**，可在设置中更改。已有配置中的自定义快捷键和旧版快捷键会保留。
- 调整截图工具栏、圆角与阴影、水印、文字编辑和撤销等操作。
- 关闭上游自动更新，避免上游安装包覆盖个人版修改。

## 构建与使用

当前仓库提供源码，尚未提供个人版安装包。此版本已在 Windows x64 本机完成构建和相关功能验证；macOS 尚未构建验证。

```powershell
git clone --recurse-submodules https://github.com/stdeven521/xiaomage-screenshot.git
cd xiaomage-screenshot
./scripts/bootstrap.ps1
./scripts/build.ps1 -Preset windows-msvc-debug -Target snow_shot
./scripts/run-snow-shot.ps1
```

构建需要 Visual Studio 2026 / MSVC、CMake 4.2+、Rust 1.97.1，以及仓库管理的 Qt 和 vcpkg 依赖。具体版本、环境准备和平台注意事项见[开发指南](docs/development.md)、[仓库构建约定](AGENTS.md)、[Windows ARM64 指南](docs-windows-arm64-build.md)与 [macOS 指南](docs-macos-build.md)。开发构建产物不能直接视为可分发安装包。

## 上游与许可

感谢 Snow Shot 作者 mg-chao 及各依赖项目的贡献。原项目介绍和功能文档保留在 [README.upstream.md](README.upstream.md) 与[上游中文说明](README.upstream.zh-CN.md)；其中的发布下载链接指向上游版本。

保留原项目的版权、许可和第三方声明，修改不会改变原有许可范围：

- 主应用及相关组件使用 [GPL-3.0-or-later](snow_shot/LICENSE)。
- `ant_design_qt`、`snow_draw_engine_qt`、`snow_memory` 等组件分别保留各自的 [Apache-2.0](ant_design_qt/LICENSE)、[Apache-2.0](snow_draw_engine_qt/LICENSE)、[Apache-2.0](snow_memory/LICENSE) 许可。
- 完整范围见[仓库许可说明](LICENSE.md)、[应用版权声明](snow_shot/COPYRIGHT)与[第三方声明](snow_shot/THIRD_PARTY_NOTICES.md)。嵌套第三方组件以其自身声明为准。
