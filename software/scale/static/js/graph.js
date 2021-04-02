(function () {
    const log = document.getElementById("log");
    const dots = document.getElementById("dots");
    const weight = document.getElementById("weight");

    const append_log = (text) => {
        var e = document.createElement('p');
        e.innerHTML = text;
        while (log.children.length > 99) {
          log.children[0].remove();
        }
        log.appendChild(e);
        log.scrollTop = log.scrollHeight;
    };

    const chart = Highcharts.chart('graph', {
        boost: { useGPUTranslations: true, usePreAllocated: true },
        chart: { animation: false },
        title: { text: 'Detected weighings' },
        yAxis: {
            title: { text: 'Weight (g)' },
            softMax: 25,
            softMin: 0,
            minPadding: 0,
            maxPadding: 0,
            plotLines: [
                { color: "#990000", width: 2, value: 19, label: { text: '19' }},
                { color: "#009900", width: 2, value: 38, label: { text: '38' }},
            ],
        },
        xAxis: { title: { text: 'Time (seconds)' }},
        plotOptions: { series: { label: { connectorAllowed: false }}},
        series: [ { name: 'Weight', data: [] } ],
    });

    const chart2 = Highcharts.chart('graph2', {
        boost: { useGPUTranslations: true, usePreAllocated: true },
        chart: { animation: false },
        title: { text: 'Continuous' },
        yAxis: {
            title: { text: 'Weight (g)' },
            softMax: 25,
            softMin: 0,
            min: -10,
            minPadding: 0,
            maxPadding: 0,
            plotLines: [
                { color: "#990000", width: 2, value: 19, label: { text: '19' }},
                { color: "#009900", width: 2, value: 38, label: { text: '38' }},
            ],
        },
        xAxis: { title: { text: 'Time' }, type: "datetime" },
        plotOptions: { series: { label: { connectorAllowed: false }}},
        series: [
            { name: 'Weight', data: [] },
            { name: 'ema2', data: [] },
            { name: 'ema3', data: [] },
        ],
    });

    let last_time = 0;
    let last_weight = 0;
    let ema = 0;

    const set_weight = (value, clear) => {
        if (clear === true || Math.abs(value - ema) > 1) {
            ema = value;
        } else {
            ema += (value - ema) * 0.1;
        }
        weight.innerHTML = `${ ema.toFixed(2) }g`;

        let now = new Date().getTime();
        const shift = chart2.series[0].data.length >= 1000;
        chart2.series[0].addPoint([now, ema], true, shift);
    };

    const clear_chart = (both) => {
        for (var i = 0; i < chart.series.length; i++) {
            chart.series[i].setData([]);
        }
        last_weight = 0;
        last_time = 0;

        if (both == true) {
            for (var i = 0; i < chart2.series.length; i++) {
                chart2.series[i].setData([]);
            }
        }
    };

    window.events.on("websocket_json", (json) => {
        dots.innerHTML += ".";
        if (dots.innerHTML.length > 50) {
            dots.innerHTML = ".";
        }

        if (json.total_time != undefined && json.weight != undefined) {
            set_weight(json.weight);
            append_log(` Extracted ${ (json.weight - json.weight_start).toFixed(1) }g (peak: ${ (json.weight_peak - json.weight_start).toFixed(1) }) in ${ json.total_time } seconds. Start weight: ${ json.weight_start }`);
        } else if (json.time != undefined && json.weight != undefined) {
            set_weight(json.weight);
            //append_log(` ${json.weight} g`);

            if (json.timer_running == true) {
                if (last_time > json.time) {
                    //append_log(` ${ last_weight } g in ${ last_time } s`);
                    clear_chart();
                }
                const shift = chart.series[0].data.length >= 1000;
                chart.series[0].addPoint([json.time, json.weight], true, shift);
                last_time = json.time;
                if (json.weight > last_weight) {
                    last_weight = json.weight;
                }

                // let x = (json.ema2 - json.ema3) * 10;
                // let running = 10;
                // if (json.timer_running) {
                //     running += 5;
                // }
                 chart2.series[1].addPoint([new Date().getTime(), json.ema2]);
                 chart2.series[2].addPoint([new Date().getTime(), json.ema3]);
                // chart.series[3].addPoint([json.time, x]);
                // chart.series[4].addPoint([json.time, running]);
            }
        } else if (json.tare != undefined) {
            clear_chart(true);
            set_weight(0, true);
            append_log("Tared");
        } else if (json.calibration_value != undefined) {
            clear_chart(true);
            append_log(`Calibration value: ${ json.calibration_value }`);
        } else {
            console.log("Unhandled message:", json);
        }
    });

    tare.onclick = () => window.events.emit("websocket_send", "do-tare");
    calibrate.onclick = () => window.events.emit("websocket_send", "do-calibrate");
})();
