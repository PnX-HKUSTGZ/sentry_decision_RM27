# 代码与文档规范

> 本文档是编码、注释、测试与文档规范的单一事实来源，配合 `docs/AGENT.md` 使用。

## 1. 命名

- 目录 / 文件 / 函数 / 变量：`snake_case`
- 类型 / 类 / 结构体：`PascalCase`
- 枚举类型：`PascalCase`；枚举值：`kPascalCase`（如 `kNavGoal`）
- 成员变量：尾部下划线（如 `intents_`）
- 常量：`kPascalCase`
- 宏：`SCREAMING_SNAKE_CASE`
- BT 注册名：`PascalCase`；BT 端口与黑板 key：`snake_case`；黑板 key 用 `domain.field`

## 2. 格式

- 统一使用根目录 `.clang-format`（clang-format 18.1.8）；提交前用 `tools/format.sh` 格式化，`tools/format.sh --check` 校验，不得有格式差异。
- 编译 0 warning：core 以 `-Wall -Wextra -Werror` 为基线。
- 头文件用 `#pragma once`；include 顺序：对应头文件、标准库、第三方、本项目。
- 头文件中禁止 `using namespace`。

## 3. 注释

- 文件头可选，但需说明模块用途。
- 公共 API 必须说明：用途、参数、返回值、线程 / 副作用。
- 复杂逻辑必须说明「为什么」，而不是复述「做了什么」。
- 行为树节点必须使用固定模板（见 `docs/ARCHITECTURE.md` 第 7.2 节），缺一即 CI 失败。
- 禁止保留大段注释掉的代码。

## 4. 日志

- 统一使用 core 日志接口，禁止 `std::cout` 与裸 `RCLCPP_*`。
- 级别：`DEBUG` / `INFO` / `ACT` / `WARN` / `ERROR`。
- 高频回调禁止逐帧 `INFO` / `DEBUG`；状态变化只在变化时打印。
- 决策行为（模式切换、目标变更、分支抢占、资源请求）使用 `ACT`。

## 5. 错误处理

- 库代码优先返回 `std::optional` / 状态码，不把异常作为常规控制流。
- 跨 BT 边界的异常必须捕获，转为节点状态并记 `ERROR` 日志。
- 外部输入必须做有效性检查；失效时显式降级。

## 6. ROS 边界

- `core` 不依赖 ROS。
- 行为树节点不直接创建 ROS 对象，只读写数据契约或调用抽象接口。
- 所有 I/O 集中在 `io` 的单一节点。

## 7. 测试

- 每个 core 模块必须有可在宿主运行的纯逻辑单测（`test/test_*.cpp`，由 `tools/host_core_test.sh` 驱动）。
- 测试文件命名 `test_<模块>`；用例函数命名 `test_<行为>`。
- 需要 ROS 的测试放 `colcon test`；能脱离 ROS 的测试尽量下沉到宿主测试。
- 修改决策逻辑或行为树 XML 必须补充对应的表驱动用例。

## 8. 文档

- docs-first：设计变更先更新文档，再改代码。
- 架构变更更新 `docs/ARCHITECTURE.md`；阶段 / 进度更新 `docs/ROADMAP.md`；用户可见变更更新 `docs/CHANGELOG.md`；当前 sprint 记录在 `docs/NOTE.md`。
- 文档使用中文，避免 ASCII 图，优先 Mermaid 或表格。

## 9. 提交

见 `docs/AGENT.md`：`<type>(<scope>): <subject>`，中文一行；正文可选、分点且简明；脚注用于关联 issue / PR / co-author。

## 10. 模块骨架

新增模块（通常是一个 ROS 2 包）至少包含：

```text
src/<pkg>/
├── package.xml
├── CMakeLists.txt
├── include/<pkg>/*.hpp
├── src/*.cpp
├── test/test_*.cpp
└── module.yaml        # 仅 BT 插件模块需要
```

新增 BT 节点的检查清单：

- [ ] 头文件包含节点注释模板的全部字段
- [ ] `providedPorts()` 与注释一致
- [ ] 异步节点实现可中断（`halt()` 取消未完成动作）
- [ ] 有对应单测或表驱动用例
- [ ] 已加入模块清单与 tree XML
