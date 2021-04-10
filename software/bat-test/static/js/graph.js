(function () {
    const log = document.getElementById("log");
    const voltage = document.getElementById("voltage");

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
        title: { text: 'Battery Voltage' },
        yAxis: {
            title: { text: 'Voltage' },
            // softMax: 5,
            // min: 0,
            // minPadding: 0,
            // maxPadding: 0,
            // plotLines: [
            //     { color: "#990000", width: 2, value: 4.2, label: { text: '4.2V' }},
            //     { color: "#009900", width: 2, value: 2.5, label: { text: '2.5V' }},
            // ],
        },
        xAxis: { title: { text: 'Date' }},
        plotOptions: { series: { label: { connectorAllowed: false }}},
        series: [ { name: 'Voltage', data: [] } ],
    });

    const downsample = () => {
        const new_data = [];
        const factor = 20;

        const data = chart.series[0].data;

        if (data.length < factor) {
            return;
        }

        let i = 0;
        while (i < data.length - 1) {
            let max = -Infinity;
            let min = Infinity;
            let max_date;
            let min_date;

            for (let j = 0; j < factor; j++) {
                let k = i + j;
                if (k >= data.length - 1) {
                    break;
                }

                const date = data[k].x;
                const value = data[k].y;

                if (value > max) {
                    max = value;
                    max_date = date;
                }

                if (value < min) {
                    min = value;
                    min_date = date;
                }
            }

            new_data.push([max_date, max]);
            if (min_date != max_date || max != min) {
                new_data.push([min_date, min]);
            }

            i += factor;
        }

        const original_count = data.length;
        chart.series[0].setData(new_data);

        append_log(`${ new Date().toLocaleString() }: Downsampled ${ original_count } points to ${ new_data.length } points`);
    };

    const set_voltage = (value, clear) => {
        voltage.innerHTML = `${ value.toFixed(2) }V`;

        let now = new Date().getTime();
        chart.series[0].addPoint([now, value]);

        if (chart.series[0].data.length > 1000) {
            downsample();
        }
    };

    window.events.on("websocket_json", (json) => {
        if (json.voltage != undefined) {
            set_voltage(json.voltage);
        } else {
            console.log("Unhandled message:", json);
        }
    });
})();
