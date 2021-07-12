import '../lib/utils/process_fix';
import '@hookstate/devtools';
import React from 'react';
import ReactDOM from 'react-dom';
import { Container, Table, Grid, Button, Tab, Divider, Input, Dropdown } from 'semantic-ui-react';
import Highcharts from 'highcharts';
import HighchartsReact from 'highcharts-react-official';
import { useState, Downgraded } from '@hookstate/core';
import './styles';
import { GlobalState, calculate_derived_state, send_scale_command, find_series, ChartState } from '../lib/state/index';

Highcharts.setOptions({
  plotOptions: {
    series: {
      animation: false,
    },
  },
});

const Chart = () => {
  const state = useState(ChartState.chartOptions);
  return <HighchartsReact highcharts={Highcharts} options={state.attach(Downgraded).get()} updateArgs={[true]} />;
};

const Setting = (props) => {
  const state = useState(props.state);
  const label = props.label;
  const step = props?.step || 0.5;

  return (
    <Table.Row>
      <Table.Cell width={5} content={label} />
      <Table.Cell>
        <Input
          fluid
          value={state.get()}
          onChange={(e) => {
            const n = Number(e.target.value);
            if (!Number.isNaN(n)) {
              state.set(() => n);
              calculate_derived_state();
            }
          }}
        />
      </Table.Cell>
      <Table.Cell width={2}>
        <Button
          fluid
          content="+"
          onClick={() => {
            state.set((p) => Number(p) + Number(step));
            calculate_derived_state();
          }}
        />
      </Table.Cell>
      <Table.Cell width={2}>
        <Button
          fluid
          content="-"
          onClick={() => {
            state.set((p) => p - step);
            calculate_derived_state();
          }}
        />
      </Table.Cell>
    </Table.Row>
  );
};

const to_ms = (f) => Math.round(f * 1000);

const start_extraction = (state) => {
  if (state.bean_weight.current.get() < 5) {
    state.bean_weight.current.set(() => state.bean_weight.target.get());
  }

  const extraction_weight = to_ms(state.extraction_weight.target.get());
  const preinfusion = to_ms(state.preinfusion.get());
  send_scale_command(`do-extract:${preinfusion}:${extraction_weight}`);
  const series = find_series(`Actual`);
  series.data.set(() => []);
};

const BottomHalf = () => {
  const state = useState(GlobalState);
  return (
    <Grid columns={2} divided>
      <Grid.Row>
        <Grid.Column width={5}>
          <Table basic="very">
            <Table.Body>
              <Table.Row>
                <Table.Cell>State</Table.Cell>
                <Table.Cell colSpan={2}>{state.current_state.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Current Weight (g)</Table.Cell>
                <Table.Cell />
                <Table.Cell>{state.current_weight.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Bean Weight (g)</Table.Cell>
                <Table.Cell width={3}>{state.bean_weight.target.get()}</Table.Cell>
                <Table.Cell width={3}>{state.bean_weight.current.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Extraction Ratio</Table.Cell>
                <Table.Cell>{state.extraction_ratio.target.get()}</Table.Cell>
                <Table.Cell>{state.extraction_ratio.current.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Extraction Weight (g)</Table.Cell>
                <Table.Cell>{state.extraction_weight.target.get()}</Table.Cell>
                <Table.Cell>{state.extraction_weight.current.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Extraction Rate (g/s)</Table.Cell>
                <Table.Cell>{state.extraction_rate.target.get()}</Table.Cell>
                <Table.Cell>{state.extraction_rate.current.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Time (s)</Table.Cell>
                <Table.Cell>{state.time.target.get()}</Table.Cell>
                <Table.Cell>{state.time.current.get()}</Table.Cell>
              </Table.Row>
              <Table.Row>
                <Table.Cell>Message interval (ms)</Table.Cell>
                <Table.Cell />
                <Table.Cell>{state.message_interval.get()}</Table.Cell>
              </Table.Row>
            </Table.Body>
          </Table>
        </Grid.Column>
        <Grid.Column width={11}>
          <Tab
            id="instruction_tabs"
            panes={[
              {
                menuItem: '1. Set parameters',
                render: () => (
                  <Tab.Pane>
                    <Table basic="very">
                      <Table.Body>
                        <Table.Row>
                          <Table.Cell colSpan={4}>
                            <Dropdown
                              placeholder="Select saved profile"
                              fluid
                              selection
                              search
                              defaultValue="Fika Uganda"
                              options={[
                                { key: '1', text: 'Fika Uganda', value: 'Fika Uganda' },
                                { key: '2', text: 'Fika Bom Jesus', value: 'Fika Bom Jesus' },
                              ]}
                            />
                          </Table.Cell>
                        </Table.Row>
                        <Setting label="Bean dose (g)" state={state.bean_weight.target} />
                        <Setting label="Grind level" state={state.grind_level} />
                        <Setting label="Pre-infusion time (s)" state={state.preinfusion} />
                        <Setting label="Bean to liquid ratio" state={state.extraction_ratio.target} step={0.05} />
                        <Setting label="Extraction rate (g/s)" state={state.extraction_rate.target} step={0.05} />
                        <Table.Row>
                          <Table.Cell colSpan={4} content="&nbsp;" />
                        </Table.Row>
                        <Table.Row>
                          <Table.Cell content="Save as profile" />
                          <Table.Cell colSpan={2}>
                            <Input fluid />
                          </Table.Cell>
                          <Table.Cell colSpan={2}>
                            <Button fluid width={2} content="Save" />
                          </Table.Cell>
                        </Table.Row>
                        <Table.Row>
                          <Table.Cell colSpan={4} textAlign="right">
                            <a href="manage_profiles.html">Manage profiles</a>
                          </Table.Cell>
                        </Table.Row>
                      </Table.Body>
                    </Table>
                  </Tab.Pane>
                ),
              },
              {
                menuItem: '2. Weigh beans',
                render: () => (
                  <Tab.Pane>
                    <p>Tare the scale</p>
                    <p>
                      Add <strong>~{state.bean_weight.target.get()}g</strong> of beans
                    </p>
                    <p>Hit done once the scale has settled</p>
                    <p>
                      Grind the beans at <strong>{state.grind_level.get()}</strong>
                    </p>
                    <p>Tamp and put the portafilter in the machine</p>
                    <Divider />
                    <Grid columns={2} relaxed stretched>
                      <Grid.Row>
                        <Grid.Column>
                          <Button content="Tare" size="large" onClick={() => send_scale_command('do-tare')} />
                        </Grid.Column>
                        <Grid.Column>
                          <Button
                            content="Done"
                            size="large"
                            onClick={() => {
                              state.bean_weight.current.set(() => state.current_weight.get());
                              calculate_derived_state();
                            }}
                          />
                        </Grid.Column>
                      </Grid.Row>
                    </Grid>
                  </Tab.Pane>
                ),
              },
              {
                menuItem: '3. Extract',
                render: () => (
                  <Tab.Pane>
                    <p>Place scale under portafilter</p>
                    <p>Place cup on scale</p>
                    <p>Tare the scale</p>
                    <p>Hit extract to:</p>
                    <ol>
                      <li>
                        Pre-infuse for <strong>{state.preinfusion.get()}s</strong>
                      </li>
                      <li>
                        Extract <strong>{state.extraction_weight.target.get()}g</strong> of coffee
                      </li>
                      <li>
                        Aiming for a <strong>{state.time.target.get()}s</strong> extraction time
                      </li>
                    </ol>
                    <Divider />
                    <Grid columns={2} relaxed stretched>
                      <Grid.Row>
                        <Grid.Column>
                          <Button content="Tare" size="large" onClick={() => send_scale_command('do-tare')} />
                        </Grid.Column>
                        <Grid.Column>
                          <Button content="Extract" size="large" onClick={() => start_extraction(state)} />
                        </Grid.Column>
                      </Grid.Row>
                    </Grid>
                  </Tab.Pane>
                ),
              },
            ]}
          />
        </Grid.Column>
      </Grid.Row>
    </Grid>
  );
};

const App = () => {
  return (
    <Container>
      <Chart />
      <Divider />
      <BottomHalf />
    </Container>
  );
};

const container = document.getElementById('container');
const runApp = () => {
  ReactDOM.render(<App />, container);
};

export default runApp;
