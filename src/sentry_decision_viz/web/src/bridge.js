// rosbridge 适配：把 ROSLIB 的 topic / service / action 包成面板易用的接口。
// 只有这个模块接触全局 ROSLIB，其余模块保持纯数据 / DOM。
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

  function sendManualOverride(field, value, leaseSec, reason) {
    return new Promise(function (resolve, reject) {
      const client = new ROSLIB.ActionClient({
        ros: ros,
        serverName: '/decision/manual_override',
        actionName: 'sentry_decision_msgs/action/ManualOverride',
      });
      const goal = new ROSLIB.Goal({
        actionClient: client,
        goalMessage: {
          field: field,
          value: value,
          lease_sec: leaseSec,
          reason: reason || 'web',
        },
      });
      goal.on('result', function (result) {
        resolve(result);
      });
      goal.on('timeout', function () {
        reject(new Error('action timeout'));
      });
      goal.send();
    });
  }

  return {
    connect: connect,
    callDebug: callDebug,
    sendManualOverride: sendManualOverride,
  };
}
