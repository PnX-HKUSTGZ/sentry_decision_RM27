#include <cstdio>
#include <memory>
#include <string>

#include "sentry_decision_core/task_runtime.hpp"

using namespace sentry_decision;

namespace {

int g_failures = 0;

void check(bool ok, const char* expr, const char* file, int line) {
  if (!ok) {
    std::printf("CHECK failed: %s (%s:%d)\n", expr, file, line);
    ++g_failures;
  }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

class FakeTask : public Task {
 public:
  bool can_start(const WorldState&) const override {
    return can_start_;
  }
  void start(const TaskCall&, TimePoint) override {
    ++starts;
  }
  TaskOutput tick(const WorldState&, TimePoint) override {
    ++ticks;
    TaskOutput out;
    out.status = status;
    out.task_state = state;
    out.phase = "fake";
    out.progress = 0.5f;
    out.detail = detail;
    out.metrics["ticks"] = static_cast<double>(ticks);
    return out;
  }
  bool can_abort() const override {
    return abortable;
  }
  void abort() override {
    aborted = true;
    if (abort_flag != nullptr) {
      *abort_flag = true;
    }
  }
  TaskState task_state() const override {
    return state;
  }

  bool can_start_ = true;
  bool abortable = true;
  bool aborted = false;
  bool* abort_flag = nullptr;
  int starts = 0;
  int ticks = 0;
  TaskStatus status = TaskStatus::kRunning;
  TaskState state = TaskState::kScan;
  std::string detail;
};

TaskCall make_call(const std::string& name, Duration timeout = Duration{0}) {
  return TaskCall{name, ScanParams{}, timeout};
}

TaskPlan make_plan(TimePoint created_at, TaskCall call, Duration valid_for = Duration{1000}) {
  TaskPlan plan;
  plan.id = 1;
  plan.created_at = created_at;
  plan.valid_for = valid_for;
  plan.call = std::move(call);
  return plan;
}

void test_apply_starts_task() {
  TaskRegistry registry;
  FakeTask* fake = nullptr;
  registry.register_task("Fake", [&fake] {
    auto task = std::make_unique<FakeTask>();
    fake = task.get();
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake")), WorldState{}, now);

  CHECK(fake != nullptr);
  CHECK(fake->starts == 1);
  CHECK(runtime.task_state() == TaskState::kScan);
  const auto snapshot = runtime.snapshot();
  CHECK(snapshot.has_value());
  CHECK(snapshot->task == "Fake");
  CHECK(snapshot->status == TaskStatus::kRunning);
}

void test_same_plan_does_not_restart() {
  TaskRegistry registry;
  FakeTask* fake = nullptr;
  registry.register_task("Fake", [&fake] {
    auto task = std::make_unique<FakeTask>();
    fake = task.get();
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  const TaskPlan plan = make_plan(now, make_call("Fake"));
  runtime.apply(plan, WorldState{}, now);
  runtime.apply(plan, WorldState{}, now);

  CHECK(fake->starts == 1);
}

void test_task_success() {
  TaskRegistry registry;
  FakeTask* fake = nullptr;
  registry.register_task("Fake", [&fake] {
    auto task = std::make_unique<FakeTask>();
    fake = task.get();
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake")), WorldState{}, now);
  fake->status = TaskStatus::kSuccess;
  fake->detail = "done";
  runtime.tick(WorldState{}, now + Duration{10});

  const auto result = runtime.take_result();
  CHECK(result.has_value());
  CHECK(result->task == "Fake");
  CHECK(result->status == TaskStatus::kSuccess);
  CHECK(result->reason == "done");
  CHECK(result->metrics.at("ticks") == 1.0);
  CHECK(runtime.task_state() == TaskState::kIdle);

  const auto epoch = runtime.take_epoch();
  CHECK(epoch.has_value());
  CHECK(epoch->reason == ReplanReason::kTaskFinished);
}

void test_task_timeout() {
  TaskRegistry registry;
  registry.register_task("Fake", [] { return std::make_unique<FakeTask>(); });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake", Duration{100})), WorldState{}, now);
  runtime.tick(WorldState{}, now + Duration{99});
  CHECK(!runtime.take_result().has_value());
  runtime.tick(WorldState{}, now + Duration{100});

  const auto result = runtime.take_result();
  CHECK(result.has_value());
  CHECK(result->status == TaskStatus::kTimeout);
  CHECK(runtime.task_state() == TaskState::kIdle);
}

void test_preemption_aborts_old_task() {
  TaskRegistry registry;
  FakeTask* first = nullptr;
  FakeTask* second = nullptr;
  int created = 0;
  registry.register_task("Fake", [&] {
    auto task = std::make_unique<FakeTask>();
    if (created++ == 0) {
      first = task.get();
    } else {
      second = task.get();
    }
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake")), WorldState{}, now);
  bool first_aborted = false;
  first->abort_flag = &first_aborted;
  TaskPlan other = make_plan(now, TaskCall{"Fake", HealParams{}, Duration{0}});
  runtime.apply(other, WorldState{}, now);

  CHECK(first_aborted);
  CHECK(second != nullptr);
  CHECK(second->starts == 1);
  CHECK(runtime.task_state() == TaskState::kScan);
}

void test_non_abortable_is_deferred() {
  TaskRegistry registry;
  FakeTask* first = nullptr;
  FakeTask* second = nullptr;
  int created = 0;
  registry.register_task("Fake", [&] {
    auto task = std::make_unique<FakeTask>();
    if (created++ == 0) {
      first = task.get();
    } else {
      second = task.get();
    }
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake")), WorldState{}, now);
  bool first_aborted = false;
  first->abort_flag = &first_aborted;
  first->abortable = false;
  runtime.apply(make_plan(now, TaskCall{"Fake", HealParams{}, Duration{0}}), WorldState{}, now);

  CHECK(!first_aborted);
  CHECK(second == nullptr);
  CHECK(runtime.snapshot()->abort_block_reason == "pending task switch");

  first->abortable = true;
  runtime.tick(WorldState{}, now + Duration{10});
  CHECK(first_aborted);
  CHECK(second != nullptr);
  CHECK(second->starts == 1);
}

void test_resuming_active_task_clears_pending() {
  TaskRegistry registry;
  FakeTask* first = nullptr;
  FakeTask* second = nullptr;
  int created = 0;
  registry.register_task("Fake", [&] {
    auto task = std::make_unique<FakeTask>();
    if (created++ == 0) {
      first = task.get();
    } else {
      second = task.get();
    }
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  const TaskPlan current = make_plan(now, make_call("Fake"));
  runtime.apply(current, WorldState{}, now);
  first->abortable = false;

  // 先请求切换到 Heal，再改回当前 Navigate；旧的 pending 必须撤销。
  runtime.apply(make_plan(now, TaskCall{"Fake", HealParams{}, Duration{0}}), WorldState{}, now);
  runtime.apply(current, WorldState{}, now);
  CHECK(runtime.snapshot()->abort_block_reason.empty());

  first->abortable = true;
  runtime.tick(WorldState{}, now + Duration{10});
  CHECK(!first->aborted);
  CHECK(second == nullptr);
}

void test_expired_plan() {
  TaskRegistry registry;
  registry.register_task("Fake", [] { return std::make_unique<FakeTask>(); });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now - Duration{2000}, make_call("Fake")), WorldState{}, now);

  CHECK(!runtime.snapshot().has_value());
  const auto epoch = runtime.take_epoch();
  CHECK(epoch.has_value());
  CHECK(epoch->reason == ReplanReason::kPlanExpired);
}

void test_can_start_is_enforced() {
  TaskRegistry registry;
  registry.register_task("Fake", [] {
    auto task = std::make_unique<FakeTask>();
    task->can_start_ = false;
    return task;
  });

  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Fake")), WorldState{}, now);

  CHECK(!runtime.snapshot().has_value());
  const auto result = runtime.take_result();
  CHECK(result.has_value());
  CHECK(result->status == TaskStatus::kFailure);
  CHECK(result->reason == "task cannot start");
}

void test_unknown_task() {
  TaskRegistry registry;
  TaskRuntime runtime(registry);
  const TimePoint now{};
  runtime.apply(make_plan(now, make_call("Missing")), WorldState{}, now);

  CHECK(!runtime.snapshot().has_value());
  const auto result = runtime.take_result();
  CHECK(result.has_value());
  CHECK(result->status == TaskStatus::kFailure);
  CHECK(result->reason == "unknown task");
  const auto epoch = runtime.take_epoch();
  CHECK(epoch.has_value());
  CHECK(epoch->reason == ReplanReason::kTaskFailed);
}

}  // namespace

int main() {
  test_apply_starts_task();
  test_same_plan_does_not_restart();
  test_task_success();
  test_task_timeout();
  test_preemption_aborts_old_task();
  test_non_abortable_is_deferred();
  test_resuming_active_task_clears_pending();
  test_expired_plan();
  test_can_start_is_enforced();
  test_unknown_task();

  if (g_failures == 0) {
    std::printf("all task_runtime tests passed\n");
    return 0;
  }
  std::printf("%d checks failed\n", g_failures);
  return 1;
}
