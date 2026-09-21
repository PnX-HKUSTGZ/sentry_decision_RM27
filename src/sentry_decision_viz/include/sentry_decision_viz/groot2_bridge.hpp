#pragma once

#include <memory>

#include "behaviortree_cpp/bt_factory.h"

namespace sentry_decision_viz {

// 可选：把行为树挂到 BT::Groot2Publisher，供 Groot2 通过 TCP 端口实时连接。
// 构造即开始监听；对象存活期间保持连接。仅供开发调试，不参与决策。
//
// 需要 BT.CPP 编译时启用 zmq（Jazzy 的 ros-jazzy-behaviortree-cpp 4.9 已满足）。
class Groot2Bridge {
 public:
  // port 默认 1667（Groot2 默认端口）。
  explicit Groot2Bridge(const BT::Tree& tree, unsigned port = 1667);
  ~Groot2Bridge();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sentry_decision_viz
