import { GlobalState, calculate_derived_state, sanitize_float, find_series, ChartState } from './state';
import { GlobalEvents } from './events';
import { connect } from './websocket';

export { GlobalState, calculate_derived_state, find_series, ChartState } from './state';
export { GlobalEvents } from './events';

GlobalEvents.on('websocket_json', (json) => {
  if (Object.prototype.hasOwnProperty.call(json, 'state')) {
    GlobalState.current_state.set(() => json.state);
  }

  if (Object.prototype.hasOwnProperty.call(json, 'weight')) {
    const weight = sanitize_float(json.weight);
    GlobalState.current_weight.set(() => weight);

    if (Object.prototype.hasOwnProperty.call(json, 'time')) {
      const time = json.time;
      const data = find_series('Actual').data;
      let add = data.length === 0;
      let i = data.length;
      if (!add) {
        const prev = data[data.length - 1].get();
        if (prev[0] < time) {
          add = true;
        } else if (prev[0] === time) {
          add = true;
          i -= 1;
        }
      }
      if (add) {
        data[i].set([time, weight]);
        // console.log(`time: ${time}`);
      }

      if (GlobalState.current_state.get() !== 'Idle') {
        const prev = GlobalState.time.current.get();
        if (prev < time) {
          GlobalState.time.current.set(() => time);
        }
      }
    }

    calculate_derived_state();
  }

  const last = GlobalState.last_message_date.get();
  const now = new Date().valueOf();
  const delta = now - last;
  GlobalState.last_message_date.set(() => now);
  GlobalState.message_interval.set(delta);
});

connect();

export const send_scale_command = (command) => {
  console.log(`send_scale_command: ${command}`);
  GlobalEvents.emit('websocket_send', command);
};
