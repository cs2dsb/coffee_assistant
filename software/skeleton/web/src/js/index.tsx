import React from 'react';
import ReactDOM from 'react-dom';
import { Container } from 'semantic-ui-react';

import './styles';

export { Button } from '../lib/components/Button';
export type { IButtonProps } from '../lib/components/Button';

const container = document.getElementById('container');
const runApp = () => {
  ReactDOM.render(
    <Container fluid>
      <h1>Hello bob 2</h1>
    </Container>,
    container,
  );
};

export default runApp;
