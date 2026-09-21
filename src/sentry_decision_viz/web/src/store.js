// 极简订阅式状态容器：面板只读 state，更新时统一重渲染。
export function createStore(initial) {
  let state = initial || {};
  const listeners = new Set();

  function get() {
    return state;
  }

  function update(patch) {
    state = Object.assign({}, state, patch);
    listeners.forEach(function (listener) {
      listener(state);
    });
  }

  function subscribe(listener) {
    listeners.add(listener);
    listener(state);
    return function () {
      listeners.delete(listener);
    };
  }

  return { get: get, update: update, subscribe: subscribe };
}
