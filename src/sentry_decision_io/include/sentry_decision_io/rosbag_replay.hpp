#pragma once

#include <string>

#include "sentry_decision_core/replay.hpp"

namespace sentry_decision_io {

// rosbag 话题配置。默认兼容两类来源：
//  - 旧仓库 sentry_DecisionMaking 的简单标量话题（/ifhealth 等）；
//  - 新格式 sentry_interfaces 上行（/sentry/*，decision_node 与 referee_sim_node 使用）。
struct ReplayTopics {
  // 旧格式标量话题
  std::string self_hp = "/ifhealth";
  std::string self_ammo = "/remain_ammo";
  std::string base_hp = "/our_base_health";
  std::string our_outpost_hp = "/our_outpost_health";
  std::string enemy_outpost_hp = "/enemy_outpost_health";
  std::string can_rebuild_outpost = "/can_rebuild_outpost";
  // 新格式（sentry_interfaces）
  std::string game_info = "/sentry/game_info";
  std::string online_info = "/sentry/online_info";
  std::string offline_info = "/sentry/offline_info";
  std::string team_info = "/sentry/team_info";
  std::string radar_info = "/sentry/radar_info";
  std::string odometry = "/odom";
  // 人工干预记录（sentry_decision_msgs/InterventionEvent），P3.2 起由决策节点发布。
  std::string interventions = "/decision/intervention";
};

// 读取 rosbag2，把已知话题的消息转成 core 的 ReplayData。
//
// 时间戳相对 bag 第一条消息，单位 ms；referee 通道每收到一条相关消息就写入一份
// 当前完整快照（与实时节点逐字段合并再取快照的语义一致）。读取失败抛异常，
// 由调用方捕获并记录 ERROR。
sentry_decision::ReplayData load_replay_data(const std::string& bag_uri,
                                             const ReplayTopics& topics = ReplayTopics{});

}  // namespace sentry_decision_io
