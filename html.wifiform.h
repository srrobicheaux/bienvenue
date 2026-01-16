#ifndef WIFI_FORM_H
#define WIFI_FORM_H

// Using a Raw String Literal for clean HTML embedding
#define RESPOND_FORM R"RAWHTML(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=0">
    <title>Bienvenue Setup</title>
    <style>
        :root {
            --primary: #007AFF;
            --bg: #F2F2F7;
            --card: #FFFFFF;
            --text: #1C1C1E;
            --subtext: #8E8E93;
            --border: #E5E5EA;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "SF Pro Text", "Helvetica Neue", Arial, sans-serif;
            background-color: var(--bg);
            color: var(--text);
            display: flex;
            justify-content: center;
            align-items: center; /* Center vertically */
            min-height: 100vh;
            margin: 0;
            padding: 20px;
            box-sizing: border-box;
        }
        .box {
            background: var(--card);
            padding: 32px;
            border-radius: 20px;
            box-shadow: 0 12px 40px rgba(0,0,0,0.08);
            width: 100%;
            max-width: 380px;
        }
        h1 {
            font-size: 24px;
            font-weight: 700;
            margin: 0 0 24px 0;
            text-align: center;
        }
        label {
            display: block;
            font-size: 13px;
            font-weight: 500;
            color: var(--subtext);
            margin-bottom: 6px;
            margin-top: 16px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        input {
            width: 100%;
            padding: 12px;
            font-size: 16px; /* Prevents zoom on iOS */
            border: 1px solid var(--border);
            border-radius: 10px;
            background: var(--bg);
            box-sizing: border-box;
            transition: border-color 0.2s;
            -webkit-appearance: none;
        }
        input:focus {
            outline: none;
            border-color: var(--primary);
            background: #fff;
        }
        button {
            background-color: var(--primary);
            color: white;
            border: none;
            border-radius: 12px;
            padding: 16px;
            font-size: 17px;
            font-weight: 600;
            width: 100%;
            margin-top: 32px;
            cursor: pointer;
            transition: transform 0.1s, opacity 0.2s;
        }
        button:active {
            transform: scale(0.98);
            opacity: 0.9;
        }
        /* Loading skeleton */
        .loading { opacity: 0.5; pointer-events: none; }
    </style>
</head>
<body>

    <div class="box">
        <h1>Bienvenue Setup</h1>
        <form id="setupForm" action="/" method="post" class="loading">
            
            <label for="_ssid">Wi-Fi Network Name (SSID)</label>
            <input type="text" name="_ssid" id="_ssid" maxlength="32" placeholder="Searching..." required>

            <label for="_password">Wi-Fi Password</label>
            <input type="password" name="_password" id="_password" maxlength="64" placeholder="Optional">

            <label for="_BLE_target">Target Device Name</label>
            <input type="text" name="_BLE_target" id="_BLE_target" maxlength="32" placeholder="e.g. MyTag">

            <button type="submit" id="saveBtn">Connect</button>
        </form>
    </div>

    <script>
        const form = document.getElementById('setupForm');
        const inputs = {
            ssid: document.getElementById('_ssid'),
            pass: document.getElementById('_password'),
            ble: document.getElementById('_BLE_target')
        };

        // Fetch settings immediately on load
        fetch('/settings')
            .then(response => {
                if (!response.ok) throw new Error('API Error');
                return response.json();
            })
            .then(data => {
                // Populate fields
                if(data.ssid) inputs.ssid.value = data.ssid;
                if(data.password) inputs.pass.value = data.password;
                if(data.bleTarget) inputs.ble.value = data.bleTarget;
                
                // Update UX
                inputs.ssid.placeholder = "Enter Wi-Fi Name";
                form.classList.remove('loading');
            })
            .catch(error => {
                console.warn('Could not load settings:', error);
                // Even if fetch fails, let user type
                inputs.ssid.placeholder = "Enter Wi-Fi Name";
                form.classList.remove('loading');
            });
    </script>
</body>
</html>
)RAWHTML"

#endif