#include "sentry_decision_bringup/tree_loader.hpp"

#include <yaml-cpp/yaml.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include "sentry_decision_nodes/common_nodes.hpp"
#include "sentry_decision_nodes/nav_nodes.hpp"

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

std::string resolve_library(const ModuleSpec& spec, const std::string& lib_dir) {
  if (spec.library.empty()) {
    return {};
  }
  const fs::path library(spec.library);
  if (library.is_absolute() || lib_dir.empty()) {
    return spec.library;
  }
  return (fs::path(lib_dir) / library).string();
}

void register_builtin_module(const std::string& name, BT::BehaviorTreeFactory& factory,
                             std::vector<std::string>* errors) {
  if (name == "common") {
    sentry_decision::register_common_nodes(factory);
  } else if (name == "nav") {
    sentry_decision::register_nav_nodes(factory);
  } else if (name == "strategic") {
    // 非 BT 插件：由组合根链接，无需注册到工厂。
  } else {
    errors->push_back("未知 builtin 模块: " + name);
  }
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
  if (root["module_lib_dir"] && root["module_lib_dir"].IsScalar()) {
    manifest.module_lib_dir = root["module_lib_dir"].as<std::string>();
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
      if (node["type"]) {
        spec.type = node["type"].as<std::string>();
      }
      if (node["manifest"]) {
        spec.manifest = node["manifest"].as<std::string>();
      }
      manifest.modules[it->first.as<std::string>()] = spec;
    }
  }
  return manifest;
}

void register_manifest_modules(BT::BehaviorTreeFactory& factory, const TreeManifest& manifest,
                               const std::string& default_lib_dir, bool register_builtin,
                               std::vector<std::string>* errors) {
  const std::string lib_dir =
      manifest.module_lib_dir.empty() ? default_lib_dir : manifest.module_lib_dir;
  for (const auto& entry : manifest.modules) {
    const std::string& name = entry.first;
    const ModuleSpec& spec = entry.second;
    if (!spec.enabled) {
      continue;
    }
    if (spec.type == "policy") {
      continue;  // 非 BT 插件，已链接。
    }
    if (!spec.library.empty()) {
      const std::string path = resolve_library(spec, lib_dir);
      try {
        factory.registerFromPlugin(path);
      } catch (const std::exception& ex) {
        errors->push_back("加载模块 " + name + " 失败(" + path + "): " + ex.what());
      }
    } else if (spec.builtin) {
      if (register_builtin) {
        register_builtin_module(name, factory, errors);
      }
    } else {
      errors->push_back("模块 " + name + " 未声明 library 或 builtin");
    }
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

void validate_module_manifests(const std::string& tree_dir, const TreeManifest& manifest,
                               std::vector<std::string>* errors) {
  // 核心字段：由 IO / 信念层 / 战略层或上下文直接提供，不要求模块 provides。
  static const std::set<std::string> kCoreFields = {
      "world.referee", "world.self",     "world.nav",    "world.enemy",     "world.allies",
      "config.points", "config.numbers", "config.flags", "context.intents",
  };

  std::map<std::string, std::string> provider_of;  // field -> module
  std::vector<std::pair<std::string, std::vector<std::string>>> consumers;
  const fs::path modules_dir = fs::path(tree_dir) / "modules";

  for (const auto& entry : manifest.modules) {
    const std::string& name = entry.first;
    const ModuleSpec& spec = entry.second;
    if (!spec.enabled) {
      continue;
    }
    const std::string file = spec.manifest.empty() ? name + ".yaml" : spec.manifest;
    const fs::path path = modules_dir / file;
    if (!fs::exists(path)) {
      errors->push_back("缺少模块清单: " + path.string());
      continue;
    }
    YAML::Node node;
    try {
      node = YAML::LoadFile(path.string());
    } catch (const std::exception& ex) {
      errors->push_back("读取模块清单失败(" + path.string() + "): " + ex.what());
      continue;
    }
    if (node["provides"] && node["provides"].IsSequence()) {
      for (const auto& item : node["provides"]) {
        const std::string field = item.as<std::string>();
        if (provider_of.count(field) != 0) {
          errors->push_back("字段 [" + field + "] 被模块 " + provider_of[field] + " 与 " + name +
                            " 重复提供");
        } else {
          provider_of[field] = name;
        }
      }
    }
    std::vector<std::string> consumes;
    if (node["consumes"] && node["consumes"].IsSequence()) {
      for (const auto& item : node["consumes"]) {
        consumes.push_back(item.as<std::string>());
      }
    }
    consumers.emplace_back(name, std::move(consumes));
  }

  for (const auto& consumer : consumers) {
    for (const auto& field : consumer.second) {
      if (kCoreFields.count(field) != 0 || provider_of.count(field) != 0) {
        continue;
      }
      errors->push_back("模块 " + consumer.first + " 消费字段 [" + field + "] 无提供者");
    }
  }
}

bool setup_tree_factory(BT::BehaviorTreeFactory& factory, const std::string& tree_path,
                        const sentry_decision::PolicyConfig* config,
                        std::vector<std::string>* errors, const std::string& module_lib_dir,
                        bool register_builtin) {
  const fs::path root(tree_path);
  const fs::path tree_dir = root.parent_path();
  const fs::path manifest_path = tree_dir / "tree_manifest.yaml";

  if (fs::exists(manifest_path)) {
    const TreeManifest manifest = load_tree_manifest(manifest_path.string(), errors);
    validate_module_manifests(tree_dir.string(), manifest, errors);
    register_manifest_modules(factory, manifest, module_lib_dir, register_builtin, errors);
    if (!manifest.root.empty()) {
      const fs::path manifest_root = tree_dir / manifest.root;
      if (!fs::exists(manifest_root)) {
        errors->push_back("tree_manifest.yaml 的 root 不存在: " + manifest_root.string());
      }
    }
  } else if (register_builtin) {
    sentry_decision::register_common_nodes(factory);
    sentry_decision::register_nav_nodes(factory);
  }

  if (config != nullptr) {
    validate_tree_config(tree_dir.string(), *config, errors);
  }
  return errors->empty();
}

}  // namespace sentry_decision_bringup
