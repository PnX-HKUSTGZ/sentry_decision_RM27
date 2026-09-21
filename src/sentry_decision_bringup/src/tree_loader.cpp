#include "sentry_decision_bringup/tree_loader.hpp"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

#include "sentry_decision_nodes/nodes.hpp"

namespace sentry_decision_bringup {
namespace {

namespace fs = std::filesystem;

std::string read_text(const fs::path& path) {
  std::ifstream in(path);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

bool is_expression(const std::string& value) {
  return value.find('{') != std::string::npos || value.find('$') != std::string::npos;
}

}  // namespace

TreeManifest load_tree_manifest(const std::string& manifest_path,
                                std::vector<std::string>* errors) {
  TreeManifest manifest;
  YAML::Node root;
  try {
    root = YAML::LoadFile(manifest_path);
  } catch (const std::exception& ex) {
    errors->push_back(std::string("读取 tree_manifest.yaml 失败: ") + ex.what());
    return manifest;
  }
  if (root["root"] && root["root"].IsScalar()) {
    manifest.root = root["root"].as<std::string>();
  } else {
    errors->push_back("tree_manifest.yaml 缺少 root");
  }
  if (root["modules"] && root["modules"].IsMap()) {
    for (auto it = root["modules"].begin(); it != root["modules"].end(); ++it) {
      ModuleSpec spec;
      const YAML::Node& node = it->second;
      if (node["enabled"]) {
        spec.enabled = node["enabled"].as<bool>();
      }
      if (node["builtin"]) {
        spec.builtin = node["builtin"].as<bool>();
      }
      if (node["library"]) {
        spec.library = node["library"].as<std::string>();
      }
      manifest.modules[it->first.as<std::string>()] = spec;
    }
  }
  return manifest;
}

void register_manifest_modules(BT::BehaviorTreeFactory& factory, const TreeManifest& manifest,
                               void (*register_builtin)(BT::BehaviorTreeFactory&),
                               std::vector<std::string>* errors) {
  bool need_builtin = false;
  for (const auto& entry : manifest.modules) {
    const std::string& name = entry.first;
    const ModuleSpec& spec = entry.second;
    if (!spec.enabled) {
      continue;
    }
    if (!spec.library.empty()) {
      try {
        factory.registerFromPlugin(spec.library);
      } catch (const std::exception& ex) {
        errors->push_back("加载模块 " + name + " 失败(" + spec.library + "): " + ex.what());
      }
    } else if (spec.builtin) {
      need_builtin = true;
    } else {
      errors->push_back("模块 " + name + " 未声明 library 或 builtin");
    }
  }
  if (need_builtin && register_builtin != nullptr) {
    register_builtin(factory);
  }
}

void validate_tree_config(const std::string& tree_dir, const sentry_decision::PolicyConfig& config,
                          std::vector<std::string>* errors) {
  // *point="..."：命名点；*_key="..."：配置 key。含 { 或 $ 的为表达式，跳过。
  const std::regex key_re("([A-Za-z0-9_]+_key)\\s*=\\s*\"([^\"]*)\"");
  const std::regex point_re("([A-Za-z0-9_]*point)\\s*=\\s*\"([^\"]*)\"");

  std::error_code ec;
  for (const auto& entry : fs::recursive_directory_iterator(tree_dir, ec)) {
    if (ec) {
      errors->push_back("遍历行为树目录失败: " + ec.message());
      break;
    }
    if (!entry.is_regular_file() || entry.path().extension() != ".xml") {
      continue;
    }
    const std::string text = read_text(entry.path());
    const std::string file = entry.path().filename().string();

    for (auto it = std::sregex_iterator(text.begin(), text.end(), key_re);
         it != std::sregex_iterator(); ++it) {
      const std::string key = (*it)[2].str();
      if (is_expression(key)) {
        continue;
      }
      if (!config.number(key).has_value() && !config.flag(key).has_value()) {
        errors->push_back(file + ": 配置缺少 key [" + key + "]");
      }
    }
    for (auto it = std::sregex_iterator(text.begin(), text.end(), point_re);
         it != std::sregex_iterator(); ++it) {
      const std::string name = (*it)[2].str();
      if (is_expression(name)) {
        continue;
      }
      if (config.find_point(name) == nullptr) {
        errors->push_back(file + ": 配置缺少命名点 [" + name + "]");
      }
    }
  }
}

bool setup_tree_factory(BT::BehaviorTreeFactory& factory, const std::string& tree_path,
                        const sentry_decision::PolicyConfig* config,
                        std::vector<std::string>* errors, bool register_builtin) {
  const fs::path root(tree_path);
  const fs::path tree_dir = root.parent_path();
  const fs::path manifest_path = tree_dir / "tree_manifest.yaml";

  if (fs::exists(manifest_path)) {
    const TreeManifest manifest = load_tree_manifest(manifest_path.string(), errors);
    register_manifest_modules(factory, manifest,
                              register_builtin ? &sentry_decision::register_sentry_nodes : nullptr,
                              errors);
    if (!manifest.root.empty()) {
      const fs::path manifest_root = tree_dir / manifest.root;
      if (!fs::exists(manifest_root)) {
        errors->push_back("tree_manifest.yaml 的 root 不存在: " + manifest_root.string());
      }
    }
  } else if (register_builtin) {
    sentry_decision::register_sentry_nodes(factory);
  }

  if (config != nullptr) {
    validate_tree_config(tree_dir.string(), *config, errors);
  }
  return errors->empty();
}

}  // namespace sentry_decision_bringup
