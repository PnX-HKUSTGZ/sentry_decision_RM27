#pragma once

#include <map>
#include <string>
#include <vector>

#include "behaviortree_cpp/bt_factory.h"
#include "sentry_decision_core/config.hpp"

namespace sentry_decision_bringup {

// 模块声明：builtin 表示由组合根链接注册（P2.0 过渡），library 表示独立 .so。
struct ModuleSpec {
  bool enabled = false;
  bool builtin = false;
  std::string library;
};

struct TreeManifest {
  std::string root;
  std::map<std::string, ModuleSpec> modules;
};

TreeManifest load_tree_manifest(const std::string& manifest_path, std::vector<std::string>* errors);

// 按 manifest 注册 enabled 模块；library 走 registerFromPlugin，builtin 走回调（只调用一次）。
void register_manifest_modules(BT::BehaviorTreeFactory& factory, const TreeManifest& manifest,
                               void (*register_builtin)(BT::BehaviorTreeFactory&),
                               std::vector<std::string>* errors);

// 启动校验：遍历 tree_dir 下所有 .xml，检查 *_key / *point 引用的配置是否存在。
void validate_tree_config(const std::string& tree_dir, const sentry_decision::PolicyConfig& config,
                          std::vector<std::string>* errors);

// 组合步骤：读 manifest / 注册模块 / 校验树与配置。完成后再 createTreeFromFile。
bool setup_tree_factory(BT::BehaviorTreeFactory& factory, const std::string& tree_path,
                        const sentry_decision::PolicyConfig* config,
                        std::vector<std::string>* errors, bool register_builtin = true);

}  // namespace sentry_decision_bringup
