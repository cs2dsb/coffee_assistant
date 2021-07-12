(function () {
    const log = document.getElementById("log");
    const dots = document.getElementById("dots");
    const pressit = document.getElementById("pressit");

    const append_log = (text) => {
        var e = document.createElement('p');
        e.innerHTML = text;
        while (log.children.length > 99) {
          log.children[0].remove();
        }
        log.appendChild(e);
        log.scrollTop = log.scrollHeight;
    };

    window.events.on("websocket_json", (json) => {
        dots.innerHTML += ".";
        if (dots.innerHTML.length > 50) {
            dots.innerHTML = ".";
        }


        append_log(`Unhandled message: ${JSON.stringify(json)}`);
    });

    pressit.onclick = () => window.events.emit("websocket_send", "do-pressit");
})();
