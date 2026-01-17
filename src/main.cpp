#include "Pedal.h"
#include <ArduinoJson.h>
#include <Joystick_ESP32S2.h>
#include <LittleFS.h>
#include <USB.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFiManager.h>

// --- DEFINITIONS ---
#define PIN_GAS 1
#define PIN_BRAKE 2
#define PIN_CLUTCH 3

// --- GLOBALS ---
// 3 Axes (X, Y, Z), 0 buttons, 0 hats
Joystick_ Joystick(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_JOYSTICK, 0, 0,
                   true, true, true,     // X, Y, Z
                   false, false, false,  // Rx, Ry, Rz
                   false, false,         // Rudder, Throttle
                   false, false, false); // Accelerator, Brake, Steering

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
WiFiManager wm;

// Pedals
Pedal pedalGas(PIN_GAS, 0);
Pedal pedalBrake(PIN_BRAKE, 1);
Pedal pedalClutch(PIN_CLUTCH, 2);

// Threading
TaskHandle_t TaskSensorHandle;
QueueHandle_t qPedalData;

// Output to HID: Verified in TaskSensor
int customCurves[4][11] = {{0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
                           {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
                           {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100},
                           {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}};

struct PedalData {
  int gasRaw, brakeRaw, clutchRaw;
  int gasOut, brakeOut, clutchOut;
};

// WiFi Config
// Managed by WiFiManager

// --- CONFIGURATION ---
const char *configPath = "/config.json";

// Forward declarations of helper functions
void applyConfigDoc(const StaticJsonDocument<2048> &doc);
void serializeConfigDoc(StaticJsonDocument<2048> &doc);

void loadConfig() {
  if (!LittleFS.exists(configPath))
    return;
  File f = LittleFS.open(configPath, "r");
  if (!f)
    return;

  StaticJsonDocument<2048> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err)
    return;

  // Load Custom Curves
  JsonArray customs = doc["customs"];
  if (!customs.isNull() && customs.size() == 4) {
    for (int i = 0; i < 4; i++) {
      JsonArray pts = customs[i];
      if (!pts.isNull() && pts.size() == 11) {
        for (int j = 0; j < 11; j++)
          customCurves[i][j] = pts[j];
      }
    }
  }

  // Load Config

  applyConfigDoc(doc);
}

void saveConfig() {
  StaticJsonDocument<2048> doc;
  serializeConfigDoc(doc);
  File f = LittleFS.open(configPath, "w");
  serializeJson(doc, f);
  wm.process();

  static uint32_t last_print = 0;
  if (millis() - last_print > 5000) {
    last_print = millis();
    Serial0.println("Loop heartbeat...");
  }
  f.close();
}

// Helper to apply JSON to Pedals
void applyConfigDoc(const StaticJsonDocument<2048> &doc) {
  auto loadOne = [](Pedal &p, JsonVariantConst obj) {
    PedalConfig c = p.getConfig();
    if (obj.containsKey("min"))
      c.min = obj["min"];
    if (obj.containsKey("max"))
      c.max = obj["max"];
    if (obj.containsKey("dzStart"))
      c.deadzoneStart = obj["dzStart"];
    if (obj.containsKey("dzEnd"))
      c.deadzoneEnd = obj["dzEnd"];
    if (obj.containsKey("inverted"))
      c.inverted = obj["inverted"];
    if (obj.containsKey("smooth"))
      c.smoothing = obj["smooth"];
    if (obj.containsKey("ceil"))
      c.outputCeiling = obj["ceil"];
    JsonArrayConst curve = obj["curve"];
    if (!curve.isNull() && curve.size() == 11) {
      for (int i = 0; i < 11; i++)
        c.curvePoints[i] = curve[i];
    }
    p.setConfig(c);
  };
  if (doc.containsKey("gas"))
    loadOne(pedalGas, doc["gas"]);
  if (doc.containsKey("brake"))
    loadOne(pedalBrake, doc["brake"]);
  if (doc.containsKey("clutch"))
    loadOne(pedalClutch, doc["clutch"]);
}

// Helper to serialize Pedals to JSON
void serializeConfigDoc(StaticJsonDocument<2048> &doc) {
  auto saveOne = [](Pedal &p, JsonObject obj) {
    PedalConfig c = p.getConfig();
    obj["min"] = c.min;
    obj["max"] = c.max;
    obj["dzStart"] = c.deadzoneStart;
    obj["dzEnd"] = c.deadzoneEnd;
    obj["inverted"] = c.inverted;
    obj["smooth"] = c.smoothing;
    obj["ceil"] = c.outputCeiling;
    JsonArray curve = obj.createNestedArray("curve");
    for (int i = 0; i < 11; i++)
      curve.add(c.curvePoints[i]);
  };
  saveOne(pedalGas, doc.createNestedObject("gas"));
  saveOne(pedalBrake, doc.createNestedObject("brake"));
  saveOne(pedalClutch, doc.createNestedObject("clutch"));

  JsonArray customs = doc.createNestedArray("customs");
  for (int i = 0; i < 4; i++) {
    JsonArray row = customs.createNestedArray();
    for (int j = 0; j < 11; j++)
      row.add(customCurves[i][j]);
  }
}

// --- TASKS ---
void TaskSensor(void *pvParameters) {
  (void)pvParameters;

  pedalGas.begin();
  pedalBrake.begin();
  pedalClutch.begin();
  // Joystick.begin(); // Moved to setup()

  for (;;) {
    pedalGas.update();
    pedalBrake.update();
    pedalClutch.update();

    Joystick.setXAxis(map(pedalGas.getOutput(), 0, 4095, 0, 1023));
    Joystick.setYAxis(map(pedalBrake.getOutput(), 0, 4095, 0, 1023));
    Joystick.setZAxis(map(pedalClutch.getOutput(), 0, 4095, 0, 1023));
    // Joystick.sendState(); // Auto-send is default in library

    static unsigned long lastUI = 0;
    if (millis() - lastUI > 30) {
      lastUI = millis();
      PedalData d;
      d.gasRaw = pedalGas.getRaw();
      d.brakeRaw = pedalBrake.getRaw();
      d.clutchRaw = pedalClutch.getRaw();
      d.gasOut = pedalGas.getOutput();
      d.brakeOut = pedalBrake.getOutput();
      d.clutchOut = pedalClutch.getOutput();
      xQueueOverwrite(qPedalData, &d);
    }
    vTaskDelay(1);
  }
}

// --- WEB HANDLERS ---
void handleSocket(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
}

void handleConfigPost() {
  if (server.hasArg("plain")) {
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    if (err)
      return server.send(400);

    vTaskSuspend(TaskSensorHandle);

    // Save Custom Curves if present
    JsonArray customs = doc["customs"];
    if (!customs.isNull() && customs.size() == 4) {
      for (int i = 0; i < 4; i++) {
        JsonArray pts = customs[i];
        if (!pts.isNull() && pts.size() == 11) {
          for (int j = 0; j < 11; j++)
            customCurves[i][j] = pts[j];
        }
      }
    }

    applyConfigDoc(doc);
    saveConfig();
    vTaskResume(TaskSensorHandle);
    server.send(200, "application/json", "{\"status\":\"ok\"}");
  } else {
    server.send(400);
  }
}

void handleConfigGet() {
  StaticJsonDocument<2048> doc;
  serializeConfigDoc(doc);
  // Add current IP for UI display
  doc["ip"] = WiFi.localIP().toString();
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// --- PROFILES API ---
// GET /api/profiles/list
void handleProfilesList() {
  if (!LittleFS.exists("/profiles"))
    LittleFS.mkdir("/profiles");
  File root = LittleFS.open("/profiles");
  if (!root || !root.isDirectory()) {
    server.send(200, "application/json", "[]");
    return;
  }
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.to<JsonArray>();

  File file = root.openNextFile();
  while (file) {
    String name = file.name();
    if (name.endsWith(".json")) {
      // remove extension for display?
      // name is usually "/profiles/foo.json" or just "foo.json" depending on
      // lib version Let's just send filename
      arr.add(name);
    }
    file = root.openNextFile();
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /api/profiles/save?name=F1
void handleProfileSave() {
  if (!server.hasArg("name"))
    return server.send(400);
  String name = server.arg("name");
  // sanitize
  name.replace("/", "");
  if (!name.endsWith(".json"))
    name += ".json";
  String path = "/profiles/" + name;

  // Get current config
  StaticJsonDocument<2048> doc;
  serializeConfigDoc(doc);

  File f = LittleFS.open(path, "w");
  if (f) {
    serializeJson(doc, f);
    f.close();
    server.send(200, "application/json", "{\"status\":\"saved\"}");
  } else {
    server.send(500, "application/json", "{\"status\":\"error\"}");
  }
}

// POST /api/profiles/load?name=F1.json
void handleProfileLoad() {
  if (!server.hasArg("name"))
    return server.send(400);
  String name = server.arg("name");
  if (!name.endsWith(".json"))
    name += ".json";
  String path = "/profiles/" + name;

  if (!LittleFS.exists(path))
    return server.send(404);

  File f = LittleFS.open(path, "r");
  if (f) {
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (!err) {
      vTaskSuspend(TaskSensorHandle);
      applyConfigDoc(doc);
      saveConfig(); // Persist as active config
      vTaskResume(TaskSensorHandle);
      server.send(200, "application/json", "{\"status\":\"loaded\"}");
      return;
    }
  }
  server.send(500);
}

// POST /api/profiles/delete?name=F1.json
void handleProfileDelete() {
  if (!server.hasArg("name"))
    return server.send(400);
  String name = server.arg("name");
  String path = "/profiles/" + name;
  if (LittleFS.remove(path))
    server.send(200);
  else
    server.send(500);
}

void setup() {
  // Init IO
  Serial0.begin(115200);
  delay(1000); // Give serial time to wake up
  Serial0.println("\n\n=== FIRMWARE BOOT START ===");

  // Init USB immediately so it enumerates before WiFi blocks
  Serial0.println("Initializing USB...");
  USB.begin();
  Joystick.setXAxisRange(0, 1023);
  Joystick.setYAxisRange(0, 1023);
  Joystick.setZAxisRange(0, 1023);
  Joystick.begin();
  Serial0.println("USB Initialized.");

  LittleFS.begin(true); // format on fail
  if (!LittleFS.exists("/profiles"))
    LittleFS.mkdir("/profiles");

  loadConfig();

  qPedalData = xQueueCreate(1, sizeof(PedalData));

  xTaskCreatePinnedToCore(TaskSensor, "SensorTask", 4096, NULL, 1,
                          &TaskSensorHandle, 1);

  // WiFi Setup
  // WiFi Setup (WiFiManager)
  // wm.resetSettings(); // Unlock if you need to wipe during dev

  // Set non-blocking if we want pedals to work during partial connection fail,
  // but for initial setup, blocking is safer to guarantee IP.
  // We'll trust autoConnect to handle it.
  wm.setAPCallback([](WiFiManager *myWiFiManager) {
    Serial0.println("Entered config mode");
    Serial0.println(WiFi.softAPIP());
    Serial0.println(myWiFiManager->getConfigPortalSSID());
  });

  bool res = wm.autoConnect("G25_S3_Pedals");

  if (!res) {
    Serial0.println("Failed to connect");
    // ESP.restart();
  } else {
    Serial0.println("WiFi Connected!");
    Serial0.print("IP: ");
    Serial0.println(WiFi.localIP());
  }

  // Explicitly handle root to ensure index.html is served
  server.on("/", HTTP_GET, []() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
      server.send(404, "text/plain", "File Not Found");
      return;
    }
    server.streamFile(file, "text/html");
    file.close();
  });
  server.serveStatic("/", LittleFS, "/");
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  // Profiles
  server.on("/api/profiles/list", HTTP_GET, handleProfilesList);
  server.on("/api/profiles/save", HTTP_POST, handleProfileSave);
  server.on("/api/profiles/load", HTTP_POST, handleProfileLoad);
  server.on("/api/profiles/delete", HTTP_POST, handleProfileDelete);

  server.begin();
  webSocket.begin();
  webSocket.onEvent(handleSocket);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast > 50) {
    lastBroadcast = millis();
    PedalData d;
    if (xQueuePeek(qPedalData, &d, 0)) {
      String json = "{";
      json += "\"gas\":{\"r\":" + String(d.gasRaw) +
              ",\"o\":" + String(d.gasOut) + "},";
      json += "\"brake\":{\"r\":" + String(d.brakeRaw) +
              ",\"o\":" + String(d.brakeOut) + "},";
      json += "\"clutch\":{\"r\":" + String(d.clutchRaw) +
              ",\"o\":" + String(d.clutchOut) + "}";
      json += "}";
      webSocket.broadcastTXT(json);
    }
  }
}