#pragma once

#include <optional>
#include <string>
#include <vector>

#include "sentry_decision_core/task_policy.hpp"

namespace sentry_decision {

// 2027 utility baseline：先做可行性过滤，再对 TaskCall 打分。
// 战略优先级用权重/惩罚表达，不引入 TacticalMode；硬门限只用于非法/不可执行候选。
class UtilityTaskPolicy : public TaskPolicy {
 public:
  explicit UtilityTaskPolicy(const PolicyConfig& config);

  TaskPlan decide(const WorldState& world, const std::optional<TaskSnapshot>& active_task,
                  const HistoryView& history) const override;

 private:
  struct Candidate {
    TaskCall call;
    double score = 0.0;
    std::string reason;
  };

  struct Context {
    bool own_full = false;
    bool own_temp = false;
    bool own_lost = false;
    bool enemy_full = false;
    bool enemy_temp = false;
    bool enemy_lost = false;
    bool own_alive = false;
    bool enemy_alive = false;
    bool own_under_attack = false;
    bool enemy_under_attack = false;
    double own_hp_ratio = 0.0;
    double enemy_hp_ratio = 0.0;
    double own_risk = 0.0;
    double own_attack = 0.0;
    double enemy_attack = 0.0;
    double enemy_convert = 0.0;
    double enemy_hold = 0.0;
    double own_hold = 0.0;
  };

  Context make_context(const WorldState& world) const;
  std::vector<Candidate> candidates(const WorldState& world, const Context& context) const;
  void add_navigate_candidates(const WorldState& world, const Context& context,
                               std::vector<Candidate>* out) const;
  void add_heal_candidates(const WorldState& world, const Context& context,
                           std::vector<Candidate>* out) const;
  void add_outpost_candidates(const WorldState& world, const Context& context,
                              std::vector<Candidate>* out) const;
  void add_scan_candidate(const WorldState& world, const Context& context,
                          std::vector<Candidate>* out) const;

  double score_navigate(const WorldState& world, const Context& context,
                        const std::string& point, const TaskCall& call) const;
  double score_heal(const WorldState& world, const Context& context,
                    const std::string& point, const TaskCall& call) const;
  double score_outpost(const WorldState& world, const Context& context, Side side,
                       const TaskCall& call) const;
  double score_scan(const WorldState& world, const Context& context, const TaskCall& call) const;

  bool valid_point(const std::string& name) const;
  bool at_point(const WorldState& world, const std::string& name, double radius) const;
  bool near_any_strike(const WorldState& world, Side side) const;
  bool free_supply_ammo_pending(const WorldState& world) const;
  bool is_gain_point(const std::string& name) const;
  bool is_highland(const std::string& name) const;
  bool is_buff(const std::string& name) const;
  double distance_to(const WorldState& world, const std::string& name) const;
  double travel_cost(const WorldState& world, const std::string& name) const;
  double outpost_hp_ratio(int hp) const;

  const PolicyConfig& config_;
  double safe_hp_ratio_ = 0.55;
  double low_hp_ratio_ = 0.35;
  double critical_hp_ratio_ = 0.5;
  double attack_radius_ = 0.65;
  double travel_weight_ = 0.06;
  double switch_penalty_ = 0.35;
  double continue_bonus_ = 0.55;
  double last_call_bonus_ = 0.15;
};

}  // namespace sentry_decision
