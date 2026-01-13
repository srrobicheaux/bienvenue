// wifiform.html.h
#define RESPOND_FORM R"RAWHTML(<!DOCTYPE html>
    <html><head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    </head>
    <body>
    <div class="box">
    <h1> Bienvenue Setup</h1>
    <form action="/" method="post">
    SSID:<br><input type="text" name="_ssid" maxlength="32" required><br>
    Password:<br><input type="password" name="_password" maxlength="64"><br><br>
    BLE Name:<br><input type="text" name="_BLE_target" maxlength="32"><br><br>
    <button type="submit">Connect</button>
    </form>
    </div>
    </body></html>
)RAWHTML"