#include "ESP8266WiFi.h"
#include "ESPAsyncWebServer.h"
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h> 
// Remplacez par vos identifiants WiFi
const char* ssid = "ssid";
const char* password = "password";

bool relayState1 = 0;
bool relayState2 = 0;

const int relayPin1 = 5;  // GPIO 5 (D1)
const int relayPin2 = 4;  // GPIO 4 (D2)

// Créer un objet serveur AsyncWebServer sur le port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP WebSocket Server</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
  }
  h1 {
    font-size: 1.8rem;
    color: white;
  }
  h2 {
    font-size: 1.5rem;
    font-weight: bold;
    color: #143642;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  .content {
    padding: 30px;
    max-width: 600px;
    margin: 0 auto;
  }
  .card {
    background-color: #F8F7F9;
    box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
    padding-top:10px;
    padding-bottom:20px;
  }
  .button {
    padding: 15px 50px;
    font-size: 24px;
    text-align: center;
    outline: none;
    color: #fff;
    background-color: #0f8b8d;
    border: none;
    border-radius: 5px;
    user-select: none;
    cursor: pointer;
  }
  .button:active {
    background-color: #0f8b8d;
    transform: translateY(2px);
  }
  .state {
    font-size: 1.5rem;
    color:#8c8c8c;
    font-weight: bold;
  }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>ESP WebSocket Server</h1>
  </div>
  <div class="content">
    <div class="card">
      <h2>Relay 1 (GPIO 5)</h2>
      <p class="state">State: <span id="state1">%STATE1%</span></p>
      <p><button id="button1" class="button">Toggle Relay 1</button></p>
    </div>
    <div class="card">
      <h2>Relay 2 (GPIO 4)</h2>
      <p class="state">State: <span id="state2">%STATE2%</span></p>
      <p><button id="button2" class="button">Toggle Relay 2</button></p>
    </div>
  </div>
  <script>
    var gateway = `ws://${window.location.hostname}/ws`;
    var websocket;
    window.addEventListener('load', onLoad);
    
    function initWebSocket() {
      console.log('Trying to open a WebSocket connection...');
      websocket = new WebSocket(gateway);
      websocket.onopen    = onOpen;
      websocket.onclose   = onClose;
      websocket.onmessage = onMessage;
    }
    
    function onOpen(event) {
      console.log('Connection opened');
    }
    
    function onClose(event) {
      console.log('Connection closed');
      setTimeout(initWebSocket, 2000);
    }
    
    function onMessage(event) {
      var data = JSON.parse(event.data);
      document.getElementById('state1').innerHTML = data.relay1 === "1" ? "ON" : "OFF";
      document.getElementById('state2').innerHTML = data.relay2 === "1" ? "ON" : "OFF";
    }
    
    function onLoad(event) {
      initWebSocket();
      document.getElementById('button1').addEventListener('click', function() { toggleRelay(1); });
      document.getElementById('button2').addEventListener('click', function() { toggleRelay(2); });
    }
    
    function toggleRelay(relay) {
      websocket.send(JSON.stringify({ relay: relay }));
    }
  </script>
</body>
</html>
)rawliteral";

// Notifier les clients des états des relais
void notifyClients() {
  String json = "{\"relay1\":\"" + String(relayState1) + "\", \"relay2\":\"" + String(relayState2) + "\"}";
  ws.textAll(json);
}

// Gestion des messages WebSocket
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = String((char*)data);
    Serial.println("Received message: " + message);

    DynamicJsonDocument doc(1024);
    deserializeJson(doc, message);

    int relay = doc["relay"];
    if (relay == 1) {
      relayState1 = !relayState1;
      digitalWrite(relayPin1, relayState1);
    } else if (relay == 2) {
      relayState2 = !relayState2;
      digitalWrite(relayPin2, relayState2);
    }

    notifyClients();  // Met à jour les états des relais auprès des clients
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      notifyClients();  // Envoyer les états actuels lors de la connexion du client
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

void setup(){
  // Port série pour le débogage
  Serial.begin(115200);

  // Configuration des pins des relais
  pinMode(relayPin1, OUTPUT);
  pinMode(relayPin2, OUTPUT);
  digitalWrite(relayPin1, LOW);
  digitalWrite(relayPin2, LOW);
  
  // Connexion au Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }

  // Affichage de l'adresse IP locale
  Serial.println(WiFi.localIP());

  // Initialisation du WebSocket
  initWebSocket();

  // Route pour la page HTML
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // Démarrage du serveur
  server.begin();
}

void loop() {
  // Gestion des clients WebSocket
  ws.cleanupClients();
  // Changement d'état du relais toutes les 500 ms
  relayState1 = !relayState1;  // Inverser l'état du relais 1
  digitalWrite(relayPin1, relayState1);  // Appliquer l'état à la broche du relais
  notifyClients();  // Notifier tous les clients connectés
  delay(1000); 
}
