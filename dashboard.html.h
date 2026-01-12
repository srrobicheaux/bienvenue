// dashboard.html.h
R"RAWHTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Tesla Sync Node</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --bg-color: #0e1012;
            --card-bg: #1a1d21;
            --text-primary: #ffffff;
            --text-secondary: #9ca3af;
            --accent-blue: #3b82f6;
            --accent-green: #10b981;
            --accent-red: #ef4444;
            --accent-orange: #f59e0b;
            --border-color: #374151;
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
            max-width: 1000px;
            display: grid;
            grid-template-columns: repeat(2, 1fr);
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
        }

        .card-label { font-size: 0.85rem; color: var(--text-secondary); text-transform: uppercase; letter-spacing: 1px; margin-bottom: 8px; }
        .card-value { font-size: 2rem; font-weight: 700; }
        .card-sub { font-size: 0.9rem; color: var(--text-secondary); margin-top: 4px; }

        .card.full-width { grid-column: 1 / -1; min-height: 350px; }
        
        .card.log-card { grid-column: 1 / -1; min-height: auto; max-height: 400px; overflow: hidden; }
        .log-container {
            width: 100%;
            overflow-y: auto;
            max-height: 300px;
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

        @media (max-width: 600px) {
            .container { grid-template-columns: 1fr; }
            .card-value { font-size: 1.75rem; }
        }
    </style>
</head>
<body>

    <div class="container">
        <header>
            <h1>Tesla Sync Node</h1>
            <div class="live-indicator"><div class="dot" id="conn-dot"></div> <span id="conn-text">Connecting...</span></div>
        </header>

        <div class="card">
            <div class="card-label">Vehicle Status</div>
            <div class="card-value" id="status-value">WAITING</div>
            <div class="card-sub">Tracking active</div>
        </div>

        <div class="card">
            <div class="card-label">Signal Strength</div>
            <div style="display:flex; align-items:baseline;">
                <div class="card-value" id="rssi-value">--</div>
                <div class="card-value" style="font-size: 1rem; margin-left: 5px; color: var(--text-secondary);">dBm</div>
                <div class="signal-bars" id="signal-bars">
                    <div class="bar" style="height: 30%"></div>
                    <div class="bar" style="height: 50%"></div>
                    <div class="bar" style="height: 70%"></div>
                    <div class="bar" style="height: 100%"></div>
                </div>
            </div>
            <div class="card-sub">Data Rate</div><div id="update-rate-value" style="font-family:monospace; color: var(--accent-green)">Computing...</div>
        </div>

        <div class="card">
            <div class="card-label">Free Memory</div>
            <div class="card-value"><span id="memory-value">--</span><span style="font-size:1.5rem">Bytes</span></div>
        </div>

        <div class="card">
            <div class="card-label">Event Time</div>
            <div class="card-value" id="time-value" style="font-size: 1.5rem; font-family: monospace;">00:00:00</div>
        </div>

        <div class="card full-width">
            <div class="card-label">RSSI Trend (Live)</div>
            <div style="position: relative; height: 100%; width: 100%;">
                <canvas id="rssiChart"></canvas>
            </div>
        </div>

        <div class="card log-card">
            <div class="card-label">State Change Log</div>
            <div class="log-container">
                <table id="log-table">
                    <thead>
                        <tr>
                            <th>Time</th>
                            <th>Previous State</th>
                            <th>New State</th>
                            <th>RSSI</th>
                        </tr>
                    </thead>
                    <tbody id="log-body">
                        </tbody>
                </table>
            </div>
        </div>
    </div>

<script>
    const MAX_DATA_POINTS = 1000;
    let lastStatus = null; 
    let lastTextUpdate = 0; // Throttle timestamp
    
    // FPS / Rate Counters
    let frameCount = 0;
    let lastRateCheck = Date.now();

    function formatTime(seconds) {
        const h = Math.floor(seconds / 3600).toString().padStart(2, '0');
        const m = Math.floor((seconds % 3600) / 60).toString().padStart(2, '0');
        const s = Math.floor(seconds % 60).toString().padStart(2, '0');
        return `${h}:${m}:${s}`;
    }

    function addLogEntry(prev, curr, rssi) {
        const tbody = document.getElementById('log-body');
        const row = document.createElement('tr');
        const timeStr = new Date().toLocaleTimeString();
        
        row.innerHTML = `
            <td class="log-time">${timeStr}</td>
            <td>${prev || '-'}</td>
            <td style="font-weight:bold">${curr}</td>
            <td>${rssi} dBm</td>
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

    const ctx = document.getElementById('rssiChart').getContext('2d');
    const gradient = ctx.createLinearGradient(0, 0, 0, 400);
    gradient.addColorStop(0, 'rgba(59, 130, 246, 0.5)');
    gradient.addColorStop(1, 'rgba(59, 130, 246, 0.0)');

    const chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: Array(MAX_DATA_POINTS).fill(''),
            datasets: [{
                label: 'Signal (dBm)',
                data: Array(MAX_DATA_POINTS).fill(null),
                borderColor: '#3b82f6',
                backgroundColor: gradient,
                borderWidth: 2,
                pointRadius: 0,
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false, 
            interaction: { mode: 'none' },
            plugins: { legend: { display: false } },
            scales: {
                x: { display: false },
                y: { min: -100, max: -30, grid: { color: '#374151' }, ticks: { color: '#9ca3af' } }
            }
        }
    });

    const evtSource = new EventSource('/events');
    const connDot = document.getElementById('conn-dot');
    const connText = document.getElementById('conn-text');

    evtSource.onopen = () => {
        console.log("Connected");
        connDot.classList.add('active');
        connText.textContent = "Live Feed Active";
    };

    evtSource.onmessage = (event) => {
        try {
            const data = JSON.parse(event.data);
            const now = Date.now();
            frameCount++; // Count every single packet

            // 1. UPDATE CHART (Always, for smooth lines)
            const datasets = chart.data.datasets[0].data;
            datasets.push(data.rssi);
            datasets.shift();
            
            // Limit chart redraws to 30fps to prevent browser freeze if data is <10ms
            if (now - lastTextUpdate > 33) {
                 chart.update();
            }

            // 2. RATE CALCULATION (Events Per Second)
            if (now - lastRateCheck >= 1000) {
                const fps = frameCount; // Packets in the last second
                document.getElementById('update-rate-value').textContent = fps + " events/sec";
                frameCount = 0;
                lastRateCheck = now;
            }

            // 3. LOGIC & TEXT UPDATES (Throttled to 500ms)
            // This prevents the numbers from being a blur
            if (now - lastTextUpdate > 500) {
                document.getElementById('rssi-value').textContent = data.rssi;
                updateSignalBars(data.rssi);

                if(data.memory) document.getElementById('memory-value').textContent = data.memory;
                // Assuming data.time is microseconds, convert to HH:MM:SS
                if(data.time) {
                    const sec = Math.floor(data.time / 1000000); 
                    document.getElementById('time-value').textContent = formatTime(sec);
                }

                // Trend Logic
                let fullTrend = 'UNCONNECTED';
                if (data.trend > 0) fullTrend = 'APPROACHING';
                else if (data.trend < 0) fullTrend = 'LEAVING';
                else if (data.trend == 0) fullTrend = 'STATIONARY';

                const statusEl = document.getElementById('status-value');
                statusEl.textContent = fullTrend;
                statusEl.className = 'card-value ' + fullTrend.toLowerCase();

                // Logging Logic (Only log if status ACTUALLY changed)
                if (lastStatus !== null && lastStatus !== fullTrend) {
                    addLogEntry(lastStatus, fullTrend, data.rssi);
                }
                lastStatus = fullTrend;
                
                lastTextUpdate = now;
            }

        } catch (e) {
            console.error("Parse error", e);
        }
    };

    evtSource.onerror = () => {
        connDot.classList.remove('active');
        connText.textContent = "Connection Lost - Retrying...";
    };

</script>
</body>
</html>
)RAWHTML";