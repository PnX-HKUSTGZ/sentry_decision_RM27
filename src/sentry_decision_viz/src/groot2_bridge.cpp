#include "sentry_decision_viz/groot2_bridge.hpp"

#include <memory>

#include "behaviortree_cpp/loggers/groot2_publisher.h"

namespace sentry_decision_viz {

class Groot2Bridge::Impl {
 public:
  Impl(const BT::Tree& tree, unsigned port) : publisher(tree, port) {}

  BT::Groot2Publisher publisher;
};

Groot2Bridge::Groot2Bridge(const BT::Tree& tree, unsigned port)
    : impl_(std::make_unique<Impl>(tree, port)) {}

// Impl 定义在本翻译单元，析构必须一并定义，否则 pimpl 的 unique_ptr 无法实例化。
Groot2Bridge::~Groot2Bridge() = default;

}  // namespace sentry_decision_viz
