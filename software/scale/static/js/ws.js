(function() {
    var host = location.host;
    var icolon = host.indexOf(":");
    if (icolon > 0) {
        host = host.substring(0, icolon);
    }

    const options = {
        connectionTimeout: 1000,
        debug: false, //true,
    };
    const socket = new ReconnectingWebSocket("ws://" + host + "/ws", [], options);

    socket.onmessage = function (event) {
        try {
            const obj = JSON.parse(event.data);
            window.events.emit("websocket_json", obj);
        } catch (e) {
            console.error(`Error parsing event.data as json: ${e} (${event.data})`);
        }
    };
    socket.onopen = function (event) {
        console.log("Socket opened");
    };

    window.events.on("websocket_send", (data) => {
        socket.send(data);
    });
})();
