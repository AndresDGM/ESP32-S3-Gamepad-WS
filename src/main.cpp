#include <WiFi.h>
#include <ESPAsyncWebServer.h>

// Replace with your network credentials
const char* ssid = "GUEVARA";
const char* password = "192307856";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
// Create a WebSocket object on the /ws path
AsyncWebSocket ws("/ws");

// Basic HTML & JS interface hosted on the ESP32
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>Gamepad Tester</title>
  <style>
    body { font-family: sans-serif; text-align: center; margin-top: 50px; }
    #status { font-weight: bold; color: red; }
    .connected { color: green !important; }
    #buttons { display: flex; flex-wrap: wrap; justify-content: center; gap: 10px; max-width: 600px; margin: 20px auto; }
    .btn { padding: 10px 15px; border: 1px solid #ccc; background: #eee; border-radius: 4px; }
    .pressed { background: #4caf50; color: white; }
  </style>
</head>
<body>
  <h1>Web Gamepad Tester</h1>
  <div id="status">Connect a controller and press any button.</div>
  <div id="buttons"></div>
  <div id="axis"></div>

  <script>
    var gateway = `wss://${window.location.hostname}/ws`;
    var websocket;
    
    window.addEventListener('load', onLoad);
    const statusDiv = document.getElementById('status');
    const buttonsDiv = document.getElementById('buttons');
    const axisDiv = document.getElementById('axis');
    let gamepadIndex = null;
    let start = performance.now();
    const superNovaButtonMapping = [
        "b-a",
        "b-b",
        "b-x",
        "b-y",
        "b-l1",
        "b-r1",
        "b-l2",
        "b-r2",
        "b-minus",
        "b-plus",
        "b-l3",
        "b-R3",
        "d-p-u",
        "d-p-d",
        "d-p-l",
        "d-p-r",
        "b-home"
    ];

    const superNovaAxisMapping = [
        "lx",
        "ly",
        "rx",
        "ry"
    ];

    let previousButtonState = [];
    let previousAxisValue = [];

    window.addEventListener('gamepadconnected', (event) => {
    gamepadIndex = event.gamepad.index;
    statusDiv.textContent = `Connected: ${event.gamepad.id}`;
    statusDiv.classList.add('connected');
    requestAnimationFrame(updateGamepads);
    });

    window.addEventListener('gamepaddisconnected', (event) => {
    if (gamepadIndex === event.gamepad.index) {
        statusDiv.textContent = 'Controller disconnected.';
        statusDiv.classList.remove('connected');
        buttonsDiv.innerHTML = '';
        gamepadIndex = null;
    }
    });

    function updateGamepads() {
        if (gamepadIndex === null) return;

        const gamepad = navigator.getGamepads()[gamepadIndex];
        if (gamepad) {
            if (buttonsDiv.children.length !== gamepad.buttons.length) {
                buttonsDiv.innerHTML = '';
                gamepad.buttons.forEach((button, i) => {
                    const el = document.createElement('div');
                    el.className = 'btn';
                    el.id = `btn-${i}`;
                    el.textContent = `${superNovaButtonMapping[i]}`;
                    buttonsDiv.appendChild(el);
                    previousButtonState[i] = button.pressed;
                });
            }

            if (axisDiv.children.length !== gamepad.axes.length) {
                axisDiv.innerHTML = '';
                gamepad.axes.forEach((value, i)=>{
                    const el = document.createElement('div');
                    el.id = `axis-${i}`;
                    previousAxisValue[i] = Number(value.toFixed(2));
                    el.textContent = `${superNovaAxisMapping[i]} = ${previousAxisValue[i]}`;
                    axisDiv.appendChild(el);
                })
            }

            gamepad.buttons.forEach((button, i) => {
                const el = document.getElementById(`btn-${i}`);
                if (el) {
                    if (previousButtonState[i] !== button.pressed &&
                            websocket.readyState === WebSocket.OPEN) {
                        websocket.send(`${superNovaButtonMapping[i]}:${button.pressed}`);
                    }
                    if (button.pressed) {
                        el.classList.add('pressed');
                    } else {
                        el.classList.remove('pressed');
                    }
                    previousButtonState[i] = button.pressed;            
                }
            });

            let end = performance.now();
            if(end - start >= 20){
                gamepad.axes.forEach((value, i)=>{
                    const el = document.getElementById(`axis-${i}`);
                    if(el){
                        const fixedValue = Number(value.toFixed(2));
                        const data = `${superNovaAxisMapping[i]}:${fixedValue}`;
                        el.textContent = data;
                        if (fixedValue !== previousAxisValue[i] &&
                                websocket.readyState === WebSocket.OPEN) {
                            websocket.send(data);
                        }
                        previousAxisValue[i] = fixedValue;
                    }
                })
                start = performance.now();
            }
        }
        requestAnimationFrame(updateGamepads);
    }

        function initWebSocket() {
            console.log('Trying to open a WebSocket connection...');
            websocket = new WebSocket(gateway);
            websocket.onopen    = onOpen;
            websocket.onclose   = onClose;
            websocket.onmessage = onMessage;
        }
        function onOpen(event) { console.log('Connection opened'); }
        function onClose(event) { console.log('Connection closed'); setTimeout(initWebSocket, 2000); }
        
        function onMessage(event) {
            var state;
            if (event.data == "1"){ state = "ON"; } else { state = "OFF"; }
            document.getElementById('status').innerHTML = state;
        }
        function onLoad(event) { initWebSocket(); }
  </script>
</body>
</html>
)rawliteral";

// Handles incoming WebSocket string data transfers
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    Serial.write(data, len);
    Serial.println();
  }
}

// WebSocket Event Handler (Connections, disconnections, data)
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  Serial.begin(115200);
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP Address: ");
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Start server
  server.begin();
}

void loop() {
  // Clean up disconnected clients periodically
  ws.cleanupClients();
}
