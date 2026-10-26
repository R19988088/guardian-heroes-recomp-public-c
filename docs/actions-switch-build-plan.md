# GitHub Actions Switch 构建计划

## 目标

只用 GitHub Actions 构建含中文资源与作弊修改的 Guardian Heroes Switch NRO；本机不运行 codegen 或完整构建。构建沿用参考项目的步骤：先对自有 `game/default.xex` 运行 ReXGlue codegen，再运行 `switch/build.sh`。

## 当前证据

Actions run `40308866044` 使用项目锁定的 ReXGlue commit。codegen 连续运行 30 分钟后被 workflow 的 `timeout 30m` 主动结束；结束前进程仍约占用 112% CPU，输出目录为 184 个文件、92 MB。run 因超时失败，NVK 与 NRO 步骤未开始。此前若仅凭部分文件停止 codegen，CMake 会再次调用 codegen；不能将部分输出视为成功。

## 最小实施

1. 保留 devkitPro 官方容器、锁定的 ReXGlue SDK 修改、输入资源准备、完整 codegen、NVK 获取和 `switch/build.sh`。
2. 移除 codegen 命令的 30 分钟内层超时；由 job 的 180 分钟上限统一约束。codegen 必须正常退出，并通过现有 stamp、depfile、PCH 与 sources 清单检查。
3. 将每分钟的进程、耗时和生成文件数量/体积直接写入 Actions 实时日志，便于判断仍在工作还是停滞。失败时输出 codegen 日志并保留失败状态。
4. 推送后只以 Actions 验证：codegen 正常完成、NVK 下载成功、`switch/build.sh` 生成非空 `out/build/switch/gh.nro`、artifact 上传成功。

## 完成判据与边界

Actions artifact 中存在当前提交对应的 NRO 即为 CI 构建完成。该结果不等同于 Switch 真机启动、中文显示或作弊效果验收；这些需要目标设备验证。
