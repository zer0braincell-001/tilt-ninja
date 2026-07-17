// tilt-ninja — wand firmware, STEP 1: TRANSPORT ONLY.
// ESP32-C3 SuperMini. Tidak ada IMU, tidak ada tombol — cuma buktiin wand bisa
// jadi controller WSS yang niru protokol index.html persis, biar display.html
// nganggep wand = HP biasa. Belum bisa di-flash (nunggu hardware datang) —
// review logika + parity protokol dulu.
//
// Library: arduinoWebSockets (Links2004) — install lewat Library Manager
// ("WebSockets by Markus Sattler"). Board: "ESP32C3 Dev Module" di Arduino IDE.
//
// Protokol (niru code/tilt-ninja/index.html + relay/src/index.js persis):
//   - Connect: wss://<relay-host>/ws?room=<ROOM>&role=controller
//   - Relay = Durable Object per room, broadcast doang, nol handshake balasan
//     wajib. Server nerima koneksi -> otomatis "joined". Nggak ada pesan
//     "join" terpisah yang perlu dikirim.
//   - Payload orientasi (kirim terus, max ~30Hz step ini): {"t":N,"q":[w,x,y,z],"ts":T}
//   - Payload tombol (STEP 4 nanti, BUKAN di sketch ini): {"btn":"capture","id":S,"q":[...]}
//                                                          {"btn":"mode","id":S}
//   - Ack RTT (opsional, dari display.html baris ~1155): display kirim balik
//     {"ack":N,"ts":T} tiap nerima orientasi -> controller hitung RTT = now - T.
//     Cuma nyampe kalau ADA display yang connect ke room yang sama.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient.h>

// ==================== KONFIG (placeholder — isi sebelum flash) ====================
#define WIFI_SSID   "PLACEHOLDER_SSID"
#define WIFI_PASS   "PLACEHOLDER_PASSWORD"
#define ROOM        "1234"   // samain sama room code yang dibuka di display.html

// Relay host tetap (sama seperti default relayUrl di index.html), tanpa skema.
static const char* RELAY_HOST = "tilt-ninja-relay.well65632.workers.dev";
static const uint16_t RELAY_PORT = 443;
static String wsPath;   // dibangun di setup(): "/ws?room=<ROOM>&role=controller"

// Laju kirim payload dummy. Dikunci 30Hz di step ini — arduinoWebSockets tidak
// punya API publik buat baca kedalaman antrian TX (beda dari browser WebSocket
// yang punya `bufferedAmount`), jadi satu-satunya pagar anti-bufferbloat yang
// kita punya di sisi ESP32 adalah cap laju kirim ini sendiri. JANGAN naikin
// tanpa cara baru buat ngecek antrian — pelajaran bufferbloat dari controller
// HP (lihat STATE.md project) berlaku sama di sini.
static const uint32_t SEND_INTERVAL_MS = 1000 / 30;   // ~33ms

WebSocketsClient wsClient;

bool wifiUp = false;
bool wsUp = false;
bool roomJoinedLogged = false;   // "joined" = pesan pertama sukses terkirim setelah CONNECTED
uint32_t seqCounter = 0;
uint32_t sendCounter = 0;
uint32_t lastSendMs = 0;
uint32_t lastStatusMs = 0;

// RTT dari ack echo display (opsional, lihat catatan protokol di atas).
long lastRttMs = -1;

void connectWifi() {
  Serial.printf("[wifi] connecting to \"%s\"...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiUp = true;
    Serial.printf("[wifi] connected, ip=%s\n", WiFi.localIP().toString().c_str());
  } else {
    wifiUp = false;
    Serial.println("[wifi] FAILED to connect within 20s — akan retry via loop() bawaan lib WiFi reconnect belum aktif; restart manual kalau macet.");
  }
}

// Parsing ack manual — sengaja TANPA library JSON (nol dependency tambahan).
// Payload ack dari display.html selalu bentuk sederhana {"ack":N,"ts":T},
// jadi cukup cari substring "\"ts\":" lalu atol(). Kalau format berubah nanti
// (parity frame / step berikutnya), ganti ke ArduinoJson saat itu juga.
long parseAckTs(const char* json, size_t len) {
  const char* key = "\"ts\":";
  const char* p = strstr(json, key);
  if (!p) return -1;
  p += strlen(key);
  return atol(p);
}

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsUp = true;
      roomJoinedLogged = false;
      Serial.printf("[ws] connected: %s\n", (const char*)payload);
      break;
    case WStype_DISCONNECTED:
      wsUp = false;
      Serial.println("[ws] disconnected");
      break;
    case WStype_TEXT: {
      // Kemungkinan isinya ack RTT dari display, atau broadcast lain kalau
      // ada display lain nge-relay. Cuma peduli field "ts" buat RTT step ini.
      // `payload` dari lib TIDAK dijamin null-terminated di luar `length` —
      // copy ke buffer lokal dulu biar strstr/atol aman.
      char buf[128];
      size_t n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
      memcpy(buf, payload, n);
      buf[n] = '\0';
      long echoedTs = parseAckTs(buf, n);
      if (echoedTs >= 0) {
        lastRttMs = (long)millis() - echoedTs;
      }
      break;
    }
    case WStype_ERROR:
      Serial.println("[ws] ERROR event");
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);   // waktu buat serial monitor nyambung
  Serial.println("\n=== tilt-ninja wand — STEP 1 transport ===");

  connectWifi();

  wsPath = String("/ws?room=") + ROOM + "&role=controller";
  Serial.printf("[ws] target: wss://%s%s\n", RELAY_HOST, wsPath.c_str());

  // TODO: utang keamanan — beginSSL() tanpa CA cert = ESP32 nggak verifikasi
  // sertifikat server (setara WiFiClientSecure::setInsecure() di sisi library).
  // Cukup buat de-risking transport (step ini), TAPI ganti ke beginSslWithCA()
  // + CA cert Cloudflare yang di-embed sebelum wand dipakai beneran / dibawa
  // keluar jaringan tepercaya. Jangan lupa sebelum "final push".
  wsClient.beginSSL(RELAY_HOST, RELAY_PORT, wsPath.c_str());
  wsClient.onEvent(onWsEvent);
  wsClient.setReconnectInterval(3000);

  lastStatusMs = millis();
}

void loop() {
  wsClient.loop();   // WAJIB tiap iterasi — non-blocking, ini yang nanganin reconnect/heartbeat lib

  uint32_t now = millis();

  // --- kirim payload dummy, di-gate millis() (bukan busy-loop, bukan delay()) ---
  if (wsUp && now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    seqCounter++;

    // quaternion identity dummy — belum ada IMU di step ini.
    char buf[96];
    int n = snprintf(buf, sizeof(buf),
      "{\"t\":%lu,\"q\":[1,0,0,0],\"ts\":%lu}",
      (unsigned long)seqCounter, (unsigned long)now);
    if (n > 0 && (size_t)n < sizeof(buf)) {
      wsClient.sendTXT(buf, n);
      sendCounter++;
      if (!roomJoinedLogged) {
        roomJoinedLogged = true;
        Serial.println("[ws] first payload sent -> room joined (relay diam-diam, nol ack wajib)");
      }
    }
  }

  // --- status ringkas tiap 1 detik ---
  if (now - lastStatusMs >= 1000) {
    lastStatusMs = now;
    Serial.printf(
      "[status] wifi=%s ws=%s sent=%lu rtt=%s\n",
      wifiUp ? "up" : "DOWN",
      wsUp ? "up" : "down",
      (unsigned long)sendCounter,
      lastRttMs < 0 ? "n/a (belum ada display connect)" : (String(lastRttMs) + "ms").c_str()
    );
  }
}
