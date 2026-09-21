#pragma once

#include <map>
#include <string>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/config.hpp"

namespace sentry_decision_bringup {

// 模块声明：library 指向独立 .so（相对 module_lib_dir）；builtin 表示由组合根链接注册；
// type 为 "policy" 时表示非 BT 插件（已链接，不注册到 BT 工厂）。
struct ModuleSpec {
  bool enabled = false;
  bool builtin = false;
  std::string library;
  std::string type;      // "bt"（默认）或 "policy"
  std::string manifest;  // 模块元数据文件名，位于 tree/modules/ 下；默认 <name>.yaml
};

struct TreeManifest {
  std::string root;
  // 模块库目录；空则使用调用方传入的默认值（安装 lib 目录）。
  std::string module_lib_dir;
  std::map<std::string, ModuleSpec> modules;
};

TreeManifest load_tree_manifest(const std::string& manifest_path, std::vector<std::string>* errors);

// 按 manifest 注册 enabled 模块：
//   - library 非空：从 module_lib_dir 解析后 registerFromPlugin；
//   - builtin：按模块名分发到已链接的注册函数（register_builtin=false 时跳过）。
void register_manifest_modules(BT::BehaviorTreeFactory& factory, const TreeManifest& manifest,
                               const std::string& default_lib_dir, bool register_builtin,
                               std::vector<std::string>* errors);

// 启动校验：遍历 tree_dir 下所有 .xml，检查 *_key / *point 引用的配置是否存在。
void validate_tree_config(const std::string& tree_dir, const sentry_decision::PolicyConfig& config,
                          std::vector<std::string>* errors);

// 启动校验：读取 tree_dir/modules/ 下各启用模块的 module.yaml，
// 检查「每个 consumes 都有 provides 或核心字段提供者」「provides 不重复」。
void validate_module_manifests(const std::string& tree_dir, const TreeManifest& manifest,
                               std::vector<std::string>* errors);

// 组合步骤：读 manifest / 注册模块 / 校验树与配置。完成后再 createTreeFromFile。
bool setup_tree_factory(BT::BehaviorTreeFactory& factory, const std::string& tree_path,
                        const sentry_decision::PolicyConfig* config,
                        std::vector<std::string>* errors, const std::string& module_lib_dir = "",
                        bool register_builtin = true);

}  // namespace sentry_decision_bringup
