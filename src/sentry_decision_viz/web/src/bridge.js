// rosbridge 适配：把 ROSLIB 的 topic / service 包成面板易用的接口。
// 只有这个模块接触全局 ROSLIB，其余模块保持纯数据 / DOM。
// 不使用 ROSLIB.ActionClient：vendored roslib 1.4.1 的 ActionClient 采用 ROS 1
// actionlib 命名，无法对接 ROS 2 action，面板统一走 service。
export function createBridge(options) {
  const url = options.url;
  let ros = null;
  const topics = [];

  function setConnected(connected, detail) {
    if (options.onStatus) {
      options.onStatus(connected, detail || '');
    }
  }

  function subscribe(name, type, callback) {
    const topic = new ROSLIB.Topic({
      ros: ros,
      name: name,
      messageType: type,
      throttle_rate: 0,
    });
    topic.subscribe(callback);
    topics.push(topic);
    return topic;
  }

  function connect(newUrl) {
    const target = newUrl || url;
    if (ros) {
      topics.splice(0).forEach(function (topic) {
        topic.unsubscribe();
      });
      ros.close();
    }
    ros = new ROSLIB.Ros({ url: target });
    ros.on('connection', function () {
      setConnected(true, target);
      subscribe('/decision/tree_status', 'sentry_decision_msgs/TreeStatus', options.onTree);
      subscribe('/decision/world_state', 'sentry_decision_msgs/WorldState', options.onWorld);
      subscribe('/decision/state', 'sentry_decision_msgs/DecisionState', options.onDecision);
    });
    ros.on('error', function (error) {
      setConnected(false, String(error));
    });
    ros.on('close', function () {
      setConnected(false, '连接关闭');
    });
  }

  function callDebug(command, args) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.Service({
        ros: ros,
        name: '/decision/debug',
        serviceType: 'sentry_decision_msgs/srv/DebugCommand',
      });
      client.callService(
        { command: command, args: args || '' },
        function (result) {
          resolve(result);
        },
        function (error) {
          reject(error);
        }
      );
    });
  }

  // 比赛阶段控制（裁判仿真服务）：0 重置 / 1 准备 / 2 自检 / 3 倒计时 / 4 比赛。
  function setGameStage(stage) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.Service({
        ros: ros,
        name: '/sentry_sim/set_game_stage',
        serviceType: 'sentry_decision_msgs/srv/SetGameStage',
      });
      client.callService({ stage: stage }, resolve, reject);
    });
  }

  // 暂停 / 恢复裁判仿真（冻结计时与机器人运动）。
  function setGamePause(paused) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.Service({
        ros: ros,
        name: '/sentry_sim/set_game_pause',
        serviceType: 'sentry_decision_msgs/srv/SetGamePause',
      });
      client.callService({ paused: paused }, resolve, reject);
    });
  }

  // 直接修改裁判仿真的模拟世界（字段同场景 set_world）。
  function setSimWorld(field, value) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.Service({
        ros: ros,
        name: '/sentry_sim/set_world',
        serviceType: 'sentry_decision_msgs/srv/SetWorld',
      });
      client.callService({ field: field, value: value }, resolve, reject);
    });
  }

  // 对仿真世界施加具名效果（扣血 / 扣弹 / 摧毁）；步长由 sim.yaml 的 effects 决定。
  function applyEffect(effect) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.Service({
        ros: ros,
        name: '/sentry_sim/apply_effect',
        serviceType: 'sentry_decision_msgs/srv/ApplyEffect',
      });
      client.callService({ effect: effect }, resolve, reject);
    });
  }

  return {
    connect: connect,
    callDebug: callDebug,
    setGameStage: setGameStage,
    setGamePause: setGamePause,
    setSimWorld: setSimWorld,
    applyEffect: applyEffect,
  };
}
