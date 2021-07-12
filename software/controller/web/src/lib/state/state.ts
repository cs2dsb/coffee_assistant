import { createState, State } from '@hookstate/core';
import { Persistence } from '@hookstate/persistence';

interface TargetAndCurrent {
  current: number;
  target: number;
}

interface WeighingState {
  current_state: String;
  current_weight: number;
  bean_weight: TargetAndCurrent;
  extraction_ratio: TargetAndCurrent;
  extraction_weight: TargetAndCurrent;
  extraction_rate: TargetAndCurrent;
  time: TargetAndCurrent;
  last_message_date: number;
  message_interval: number;
  chart_state: Object;
  grind_level: number;
  preinfusion: number;
}

const default_chart_state = {
  chartOptions: {
    title: null,
    boost: { useGPUTranslations: true, usePreAllocated: true },
    chart: { animation: false },
    // legend: { enabled: false },
    yAxis: {
      title: { text: 'Weight (g)' },
      softMax: 25,
      softMin: 0,
      minPadding: 0,
      maxPadding: 0,
      plotLines: [
        // { color: "#990000", width: 2, value: 19, label: { text: '19' }},
        // { color: "#009900", width: 2, value: 38, label: { text: '38' }},
      ],
    },
    xAxis: { title: { text: 'Time (seconds)' } },
    plotOptions: { series: { label: { connectorAllowed: false } } },
    series: [
      { name: 'Ideal', data: [], color: '#33AA33' },
      // { name: 'Previous', data: [], color: '#3333AA' },
      { name: 'Actual', data: [], color: '#AA3333' },
    ],
  },
};

export const ChartState = createState(default_chart_state);

// These are the default values if there is nothing in local storage
export const GlobalState: State<WeighingState> = createState({
  current_state: 'Idle',
  current_weight: 0,
  bean_weight: { current: 0, target: 16.5 },
  extraction_ratio: { current: 0, target: 2.6 },
  extraction_weight: { current: 0, target: 42.9 },
  extraction_rate: { current: 0, target: 1.1 },
  time: { current: 0, target: 47.19 },
  last_message_date: new Date().valueOf(),
  message_interval: 0,
  grind_level: 10,
  preinfusion: 5,
} as WeighingState);
GlobalState.attach(Persistence('weighing_state_persistence'));

export const find_series = (name) => {
  const series =ChartState.chartOptions.series.get();
  for (let i = 0; i < series.length; i += 1) {
    if (series[i].name === name) {
      return ChartState.chartOptions.series[i];
    }
  }
  return undefined;
};

export const plot_ideal = () => {
  const ideal_points = [];
  const previous_points = [];
  const time_step = 2;
  const target_time = GlobalState.time.target.get();
  const target_weight = GlobalState.extraction_weight.target.get();
  const weight_step = target_weight / (target_time / time_step);

  let weight = 0.0;
  let previous_weight = 0.0;
  for (let t = 0; t < target_time; t += time_step) {
    ideal_points.push([t, weight]);
    previous_points.push([t, previous_weight]);
    weight += weight_step;
    previous_weight += weight_step * 0.9;
  }

  find_series('Ideal').data.set(ideal_points);
  // find_series('Previous').data.set(previous_points);
};

export const sanitize_float = (f) => {
  if (Number.isNaN(f)) {
    return 0;
  }
  if (!Number.isFinite(f)) {
    return 0;
  }
  return Math.round(f * 100) / 100;
};

const set_if_new = (state, new_value) => {
  if (state.get() !== new_value) {
    state.set(() => new_value);
    return true;
  }
  return false;
};

const sanitize_if_new = (state) => {
  set_if_new(
    state,
    sanitize_float(state.get()),
  );
};

export const sanitize_floats = () => {
  sanitize_if_new(GlobalState.current_weight);
  sanitize_if_new(GlobalState.bean_weight.current);
  sanitize_if_new(GlobalState.bean_weight.target);
  sanitize_if_new(GlobalState.extraction_ratio.current);
  sanitize_if_new(GlobalState.extraction_ratio.target);
  sanitize_if_new(GlobalState.extraction_weight.current);
  sanitize_if_new(GlobalState.extraction_weight.target);
  sanitize_if_new(GlobalState.extraction_rate.current);
  sanitize_if_new(GlobalState.extraction_rate.target);
  sanitize_if_new(GlobalState.time.current);
  sanitize_if_new(GlobalState.time.target);
  sanitize_if_new(GlobalState.grind_level);
  sanitize_if_new(GlobalState.preinfusion);
};

export const reset_weighing = () => {
  GlobalState.current_state.set(() => 'Idle');
  GlobalState.current_weight.set(() => 0);
  GlobalState.bean_weight.current.set(() => 0);
  GlobalState.extraction_ratio.current.set(() => 0);
  GlobalState.extraction_weight.current.set(() => 0);
  GlobalState.extraction_rate.current.set(() => 0);
  GlobalState.time.current.set(() => 0);
  GlobalState.last_message_date.set(() => new Date().valueOf());
  GlobalState.message_interval.set(() => 0);
  GlobalState.chart_state.set(() => default_chart_state);
  sanitize_floats();
  plot_ideal();
};
reset_weighing();

export const calculate_derived_state = () => {
  let diff = false;

  let source_weight = GlobalState.bean_weight.current.get();
  if (source_weight < 5) {
    source_weight = GlobalState.bean_weight.target.get();
  }

  if (GlobalState.current_state.get() !== 'Idle') {
    set_if_new(
      GlobalState.extraction_weight.current,
      GlobalState.current_weight.get(),
    );

    set_if_new(
      GlobalState.extraction_ratio.current,
      sanitize_float(GlobalState.extraction_weight.current.get() / source_weight),
    );

    set_if_new(
      GlobalState.extraction_rate.current,
      sanitize_float(GlobalState.extraction_weight.current.get() / GlobalState.time.current.get()),
    );
  }

  // Targets
  diff |= set_if_new(
    GlobalState.extraction_weight.target,
    sanitize_float(GlobalState.bean_weight.target.get() * GlobalState.extraction_ratio.target.get()),
  );
  diff |= set_if_new(
    GlobalState.time.target,
    sanitize_float(GlobalState.extraction_weight.target.get() * (1 / GlobalState.extraction_rate.target.get())),
  );

  if (diff) {
    plot_ideal();
  }
};

// setInterval(() => GlobalState.current_weight.set((p) => p + 1), 1000);

// setInterval(() => {
//   const state = find_series('Actual').data;
//   state[state.length].set(state.length);
//   // This is just a hack to make it re-render
//   // I don't know how to solve this properly. Highcharts won't accept a proxy object
//   calculate_derived_state();
// }, 1000);
