#define RESPOND_DASHBOARD R"RAWHTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Tesla Sync Node + Radar</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg-color: #0e1012;
            --card-bg: #1a1d21;
            --text-primary: #ffffff;
            --text-secondary: #9ca3af;
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --accent-purple: #8b5cf6;
            --accent-orange: #f59e0b;
            --border-color: #374151;
            --btn-off: #374151;
        }

        * { box-sizing: border-box; margin: 0; padding: 0; }

        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-primary);
            padding: 20px;
            display: flex;
            justify-content: center;
        }

        .container {
            width: 100%;
            max-width: 1200px;
            display: grid;
            grid-template-columns: repeat(4, 1fr);
            gap: 20px;
        }

        header {
            grid-column: 1 / -1;
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
        }

        h1 { font-size: 1.5rem; font-weight: 600; letter-spacing: -0.5px; }
        .live-indicator { 
            font-size: 0.8rem; color: var(--text-secondary); 
            display: flex; align-items: center; gap: 6px; 
        }
        .dot { width: 8px; height: 8px; background-color: var(--text-secondary); border-radius: 50%; }
        .dot.active { background-color: var(--accent-green); box-shadow: 0 0 8px var(--accent-green); }

        .card {
            background-color: var(--card-bg);
            border-radius: 16px;
            padding: 24px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.2);
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            grid-column: span 1;
        }

        .card-label { font-size: 0.85rem; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
        .card-value { font-size: 2rem; font-weight: 700; }
        .card-sub { font-size: 0.9rem; color: var(--text-secondary); margin-top: 4px; }

        .card.double { grid-column: span 2; }
        .card.full-width { grid-column: 1 / -1; min-height: 250px; }
        
        /* --- NEW CONTROL STYLES --- */
        .card.controls {
            grid-column: 1 / -1;
            min-height: auto;
            flex-direction: row;
            align-items: center;
            gap: 20px;
            padding: 20px;
        }

        .gpio-btn {
            flex: 1;
            background-color: var(--btn-off);
            color: var(--text-secondary);
            border: 1px solid var(--border-color);
            padding: 15px;
            border-radius: 12px;
            font-size: 1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .gpio-btn:hover { background-color: #4b5563; }
        .gpio-btn:active { transform: scale(0.98); }
        
        /* Active State */
        .gpio-btn.on {
            background-color: rgba(16, 185, 129, 0.2);
            border-color: var(--accent-green);
            color: var(--accent-green);
        }
        
        .status-dot { width: 10px; height: 10px; border-radius: 50%; background-color: #6b7280; }
        .gpio-btn.on .status-dot { background-color: var(--accent-green); box-shadow: 0 0 8px var(--accent-green); }

        /* --- END CONTROL STYLES --- */

        .card.log-card { grid-column: 1 / -1; min-height: auto; max-height: 300px; overflow: hidden; }
        .log-container {
            width: 100%;
            overflow-y: auto;
            max-height: 200px;
            border-top: 1px solid var(--border-color);
            margin-top: 10px;
        }
        table { width: 100%; border-collapse: collapse; font-size: 0.9rem; }
        th { text-align: left; color: var(--text-secondary); padding: 10px; border-bottom: 1px solid var(--border-color); font-weight: 500; }
        td { padding: 10px; border-bottom: 1px solid #2a2e33; color: var(--text-primary); }
        tr:last-child td { border-bottom: none; }
        .log-time { font-family: monospace; color: var(--accent-blue); }

        #status-value.approaching { color: var(--accent-blue); }
        #status-value.leaving { color: var(--accent-orange); }
        #status-value.stationary { color: var(--text-secondary); }
        #status-value.connected { color: var(--accent-green); }

        .signal-bars { display: inline-flex; gap: 3px; height: 20px; align-items: flex-end; margin-left: 10px; }
        .bar { width: 6px; background-color: #374151; border-radius: 2px; }
        .bar.active { background-color: var(--accent-green); }

        .unit { font-size: 1rem; color: var(--text-secondary); margin-left: 4px; }

        @media (max-width: 800px) {
            .container { grid-template-columns: 1fr; }
            .card.double { grid-column: span 1; }
            .card.controls { flex-direction: column; }
            .gpio-btn { width: 100%; }
        }
    </style>
</head>
<body>

    <div class="container">
        <header>
            <h1>Tesla Sync Node</h1>
            <div class="live-indicator">
                <span id="rate-value" style="font-family:monospace; margin-right:10px">0 eps</span>
                <div class="dot" id="conn-dot"></div> 
                <span id="conn-text">Connecting...</span>
            </div>
        </header>

        <div class="card controls">
            <button class="gpio-btn" id="btn-10" onclick="toggleGpio(10)">
                <span>Relay 1 (GPIO 10)</span>
                <div class="status-dot"></div>
            </button>
            <button class="gpio-btn" id="btn-12" onclick="toggleGpio(12)">
                <span>Relay 2 (GPIO 12)</span>
                <div class="status-dot"></div>
            </button>
        </div>

        <div class="card double">
            <div class="card-label">BLE Beacon Status</div>
            <div class="card-value" id="status-value">WAITING</div>
            <div class="card-sub">Tracking Trend</div>
        </div>

        <div class="card double">
            <div class="card-label">BLE Signal Strength</div>
            <div style="display:flex; align-items:baseline;">
                <div class="card-value" id="rssi-value">--</div>
                <span class="unit">dBm</span>
                <div class="signal-bars" id="signal-bars">
                    <div class="bar"></div><div class="bar"></div><div class="bar"></div><div class="bar"></div>
                </div>
            </div>
        </div>

        <div class="card full-width">
            <div class="card-label">RSSI Trend (History)</div>
            <div style="position: relative; height: 100%; width: 100%;">
                <canvas id="rssiChart"></canvas>
            </div>
        </div>

        <div class="card double">
            <div class="card-label">Radar Distance</div>
            <div style="display:flex; align-items:baseline;">
                <div class="card-value" id="dist-value" style="color: var(--accent-purple)">--</div>
                <span class="unit">mm</span>
            </div>
            <div class="card-sub">Peak Index: <span id="peak-idx">0</span></div>
        </div>

        <div class="card">
            <div class="card-label">Echo Mag</div>
            <div class="card-value" id="mag-value">--</div>
            <div class="card-sub">Signal Power</div>
        </div>

        <div class="card">
            <div class="card-label">Noise Floor</div>
            <div class="card-value" id="noise-value">--</div>
            <div class="card-sub">Interference</div>
        </div>

        <div class="card full-width">
            <div class="card-label">Radar Magnitude (Real-time)</div>
            <div style="position: relative; height: 100%; width: 100%;">
                <canvas id="radarChart"></canvas>
            </div>
        </div>

        <div class="card log-card">
            <div class="card-label">System Log</div>
            <div class="log-container">
                <table id="log-table">
                    <thead>
                        <tr>
                            <th>Time</th>
                            <th>Type</th>
                            <th>Details</th>
                        </tr>
                    </thead>
                    <tbody id="log-body">
                    </tbody>
                </table>
            </div>
        </div>
    </div>

<script>
    // --- GPIO CONTROL LOGIC ---
    function toggleGpio(pin) {
        // Optimistic UI update (optional, but makes it feel fast)
        // const btn = document.getElementById('btn-' + pin);
        // btn.style.opacity = "0.5"; 

        fetch('/pio=' + pin + '?toggle')
            .then(response => response.json())
            .then(data => {
                // Expecting: {"pio":"10","value":true}
                const btn = document.getElementById('btn-' + data.pio);
                if (data.value === true || data.value === "true") {
                    btn.classList.add('on');
                } else {
                    btn.classList.remove('on');
                }
                btn.style.opacity = "1";
                addLogEntry('GPIO', `GPIO ${data.pio} toggled to ${data.value}`);
            })
            .catch(err => {
                console.error("GPIO Error", err);
                addLogEntry('ERR', `Failed to toggle GPIO ${pin}`);
            });
    }

    const MAX_POINTS = 200;
    let lastStatus = null; 
    let lastTextUpdate = 0; 
    
    // FPS Counters
    let frameCount = 0;
    let lastRateCheck = Date.now();

    function formatTime(seconds) {
        if(!seconds) return "00:00:00";
        const sTotal = Math.floor(seconds / 1000000); 
        const h = Math.floor(sTotal / 3600).toString().padStart(2, '0');
        const m = Math.floor((sTotal % 3600) / 60).toString().padStart(2, '0');
        const s = Math.floor(sTotal % 60).toString().padStart(2, '0');
        return `${h}:${m}:${s}`;
    }

    function addLogEntry(type, msg) {
        const tbody = document.getElementById('log-body');
        const row = document.createElement('tr');
        const timeStr = new Date().toLocaleTimeString();
        
        let color = '#fff';
        if(type === 'BLE') color = 'var(--accent-blue)';
        if(type === 'RADAR') color = 'var(--accent-purple)';
        if(type === 'GPIO') color = 'var(--accent-green)';

        row.innerHTML = `
            <td class="log-time">${timeStr}</td>
            <td style="color:${color}; font-weight:bold">${type}</td>
            <td>${msg}</td>
        `;
        tbody.prepend(row);
        if (tbody.children.length > 50) tbody.removeChild(tbody.lastChild);
    }

    function updateSignalBars(rssi) {
        const bars = document.querySelectorAll('.bar');
        bars.forEach(b => b.classList.remove('active'));
        if (rssi > -90) bars[0].classList.add('active');
        if (rssi > -80) bars[1].classList.add('active');
        if (rssi > -70) bars[2].classList.add('active');
        if (rssi > -55) bars[3].classList.add('active');
    }

    // --- CHART 1: RSSI ---
    const ctxRssi = document.getElementById('rssiChart').getContext('2d');
    const gradRssi = ctxRssi.createLinearGradient(0, 0, 0, 300);
    gradRssi.addColorStop(0, 'rgba(59, 130, 246, 0.5)');
    gradRssi.addColorStop(1, 'rgba(59, 130, 246, 0.0)');

    const chartRssi = new Chart(ctxRssi, {
        type: 'line',
        data: {
            labels: Array(MAX_POINTS).fill(''),
            datasets: [{
                data: Array(MAX_POINTS).fill(null),
                borderColor: '#3b82f6',
                backgroundColor: gradRssi,
                borderWidth: 2,
                pointRadius: 0,
                tension: 0.3,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false, 
            plugins: { legend: { display: false } },
            scales: {
                x: { display: false },
                y: { min: -100, max: -30, grid: { color: '#374151' } }
            }
        }
    });

    // --- CHART 2: RADAR ---
    const ctxRadar = document.getElementById('radarChart').getContext('2d');
    const gradRadar = ctxRadar.createLinearGradient(0, 0, 0, 300);
    gradRadar.addColorStop(0, 'rgba(139, 92, 246, 0.5)');
    gradRadar.addColorStop(1, 'rgba(139, 92, 246, 0.0)');

    const chartRadar = new Chart(ctxRadar, {
        type: 'line',
        data: {
            labels: Array(MAX_POINTS).fill(''),
            datasets: [{
                data: Array(MAX_POINTS).fill(null),
                borderColor: '#8b5cf6',
                backgroundColor: gradRadar,
                borderWidth: 2,
                pointRadius: 0,
                tension: 0.1,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false, 
            plugins: { legend: { display: false } },
            scales: {
                x: { display: false },
                y: { min: 0, max: 1000, grid: { color: '#374151' } }
            }
        }
    });

    const evtSource = new EventSource('/events');
    const connDot = document.getElementById('conn-dot');
    const connText = document.getElementById('conn-text');

    evtSource.onopen = () => {
        connDot.classList.add('active');
        connText.textContent = "Active";
    };

    evtSource.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            const now = Date.now();
            frameCount++;

            if ('peak' in data) {
                const rData = chartRadar.data.datasets[0].data;
                rData.push(data.magnitude);
                rData.shift();

                if (now - lastTextUpdate > 200) {
                    document.getElementById('dist-value').textContent = data.mm;
                    document.getElementById('mag-value').textContent = data.magnitude;
                    document.getElementById('noise-value').textContent = data.noise;
                    document.getElementById('peak-idx').textContent = data.peak;
                }
            }

            if ('rssi' in data) {
                const bData = chartRssi.data.datasets[0].data;
                bData.push(data.rssi);
                bData.shift();

                if (now - lastTextUpdate > 200) {
                    document.getElementById('rssi-value').textContent = data.rssi;
                    updateSignalBars(data.rssi);

                    let fullTrend = 'UNCONNECTED';
                    if (data.trend === "STARTED") fullTrend = 'SCANNING';
                    else if (parseInt(data.trend) > 0) fullTrend = 'APPROACHING';
                    else if (parseInt(data.trend) < 0) fullTrend = 'LEAVING';
                    else fullTrend = 'STATIONARY';

                    const statusEl = document.getElementById('status-value');
                    statusEl.textContent = fullTrend;
                    statusEl.className = 'card-value ' + fullTrend.toLowerCase();

                    if (lastStatus !== fullTrend) {
                        addLogEntry('BLE', `Status: ${fullTrend} (${data.rssi}dBm)`);
                        lastStatus = fullTrend;
                    }
                }
            }

            if (now - lastTextUpdate > 200) {
                chartRssi.update();
                chartRadar.update();
                lastTextUpdate = now;
            }

            if (now - lastRateCheck >= 1000) {
                document.getElementById('rate-value').textContent = frameCount + " eps";
                frameCount = 0;
                lastRateCheck = now;
            }

        } catch (e) {
            console.error("Parse error", e);
        }
    };

    evtSource.onerror = () => {
        connDot.classList.remove('active');
        connText.textContent = "Reconnecting...";
    };

</script>
</body>
</html>
)RAWHTML";