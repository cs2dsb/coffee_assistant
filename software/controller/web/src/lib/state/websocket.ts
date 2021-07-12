import { GlobalEvents } from './events';

const trace = true;
const timeout = 5000;
const heartbeat = timeout / 2;
const initial_wait = 100;
const max_wait = 30000;

let wait = initial_wait;

let host = window.location.host;
const icolon = host.indexOf(':');
if (icolon > 0) {
  host = host.substring(0, icolon);
}
if (host.startsWith('127.0.0.1')) {
  host = 'esp32-controller.local';
}
const url = `ws://${host}/ws`;

let socket;
let ping_interval;
let reconnect_timeout;
let last_data;

const cancel_ping = () => {
  if (ping_interval !== undefined) {
    clearInterval(ping_interval);
    ping_interval = undefined;
  }
};

const got_data = () => {
  last_data = new Date().getTime();
};

const on_message = (event) => {
  got_data();
  if (trace) {
    console.debug('Got data:', event.data);
  }
  if (event.data === "heartbeat") {
    return;
  }

  try {
    let obj = JSON.parse(event.data);
    if (
      Object.prototype.hasOwnProperty.call(obj, 'relay') &&
      Object.prototype.hasOwnProperty.call(obj, 'origin') &&
      Object.prototype.hasOwnProperty.call(obj, 'contents')
    ) {
      if (trace) {
        console.debug(`Data is relayed by ${obj.relay} on behalf of ${obj.origin}. Unwrapping`);
      }
      obj = obj.contents;
    }
    GlobalEvents.emit('websocket_json', obj);
  } catch (e) {
    console.error(`Error parsing event.data as json: ${e} (${event.data})`);
  }
};

const on_error = (event) => {
  console.warn('WebSocket error:', event);
};

const on_close = (event) => {
  if (event.target === socket) {
    console.warn('WebSocket close:', event);
  } else {
    console.warn('Stale socket close:', event);
  }
};

const send = (data) => {
  if (trace) {
    console.log('Sending:', data);
  }
  socket.send(data);
};

const on_open = () => {
  console.log('Socket opened');

  socket.addEventListener('message', on_message);
  socket.addEventListener('error', on_error);
  socket.addEventListener('close', on_close);

  cancel_ping();
  ping_interval = setInterval(() => send('heartbeat'), heartbeat);

  wait = initial_wait;
  got_data();
};

export const connect = () => {
  reconnect_timeout = undefined;
  console.log(`Connecting to ${url}...`);
  if (socket !== undefined) {
    cancel_ping();
    socket.close();
  }
  socket = new WebSocket(url);
  socket.addEventListener('open', on_open);
  got_data();
};

const reconnect = () => {
  if (reconnect_timeout === undefined) {
    wait = Math.min(wait * 2, max_wait);
    console.log(`Reconnecting in ${wait}`);
    reconnect_timeout = setTimeout(connect, wait);
  }
};

setInterval(() => {
  if (socket !== undefined && (socket.readyState === 0 || socket.readyState === 1)) {
    const now = new Date().getTime();
    if (now - last_data > timeout) {
      console.warn('Socket timed out');
      reconnect();
      socket.close();
    }
  }
}, 1000);

GlobalEvents.on('websocket_send', send);
