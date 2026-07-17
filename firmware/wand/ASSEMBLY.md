# Wand fisik — panduan rakit (STEP 1)

Status: **prep, belum ada hardware di tangan.** Panduan ini buat dipakai begitu
parts datang. Ditulis buat pemula solder/elektronik — kalau sudah pernah rakit
breadboard, banyak bagian ini bisa dilewati cepat.

Roadmap penuh ada di bagian paling bawah. Dokumen ini fokus ke **STEP 1: buktiin
ESP32 bisa connect WSS ke relay dan display.html bisa "lihat" wand** — belum ada
sensor, belum ada tombol, belum ada baterai.

---

## 1. BOM (Bill of Materials)

| Bagian | Kebutuhan | Catatan |
|---|---|---|
| Board | ESP32-C3 SuperMini | USB-C, WiFi, cukup buat I2C+JSON+WSS |
| IMU | MPU6050 (modul GY-521) | DMP onboard, quaternion 6-axis langsung dari chip |
| Baterai | LiPo 3.7V pouch, **ada kabel + ada IC proteksi bawaan** | ≥500mAh disarankan (lihat §5) |
| Charger | TP4056 modul USB-C, **versi dengan DW01A** (proteksi over-discharge) | Jangan beli versi tanpa DW01A kalau LiPo-nya juga tanpa proteksi sendiri |
| Tombol | 2× push button 6×6×11mm, 4-pin | CAPTURE + MODE |
| Breadboard | 1× ukuran 830 titik | Buat tahap tes, sebelum solder permanen |
| Jumper | Secukupnya, male-male + male-female | |
| Solder + timah | | Buat header SuperMini & MPU6050 |

Beli 1–2 tombol cadangan — gampang rusak pas belajar solder.

---

## 2. Solder header

SuperMini dan modul MPU6050 biasanya datang **tanpa header pin terpasang**
(cuma lubang kosong di PCB). Harus disolder dulu sebelum bisa masuk breadboard.

- **SuperMini:** ada 2 baris lubang di sisi kiri-kanan board. Solder header pin
  lurus (biasanya disertakan di paket) di kedua baris, **pin menghadap ke
  bawah** (arah yang sama dengan permukaan board yang mau ditancap ke
  breadboard).
- **MPU6050 (GY-521):** 1 baris header (biasanya 8 pin: VCC, GND, SCL, SDA, XDA,
  XCL, AD0, INT). Sama, pin menghadap bawah.

Kalau belum pernah solder: tempelkan ujung solder ke PAD + PIN bersamaan
2–3 detik, baru sentuhkan timah ke pertemuan keduanya (bukan ke ujung solder).
Timah harus meleleh dan mengalir rata, bukan menggumpal bulat.

---

## 3. Setup toolchain (Arduino IDE)

1. Buka Arduino IDE → **File → Preferences** → tambahkan URL board manager:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
2. **Tools → Board → Boards Manager** → cari "esp32" (by Espressif Systems) →
   install.
3. **Tools → Board** → pilih **"ESP32C3 Dev Module"** (kalau board manager versi
   baru punya profil khusus "ESP32C3 SuperMini", pakai itu — sama-sama jalan).
4. **Sketch → Include Library → Manage Libraries** → cari **"WebSockets" by
   Markus Sattler** (kadang muncul sebagai `arduinoWebSockets` / Links2004) →
   install.
5. Flash:
   - Colok USB-C ke laptop.
   - Beberapa SuperMini butuh masuk mode download manual: **tahan tombol BOOT**
     sambil colok USB-C (atau tahan BOOT lalu tekan-lepas RESET kalau ada), baru
     lepas BOOT setelah port muncul.
   - **Tools → Port** → pilih port yang muncul (kalau tidak muncul, cek driver
     USB-to-serial chip-nya — biasanya CH340 atau CP2102, cari driver sesuai OS).
   - Klik Upload.

---

## 4. Pin map (TENTATIF — cocokkan sama sablon fisik board)

Board ESP32-C3 SuperMini beredar dengan beberapa varian sablon pin yang
sedikit beda antar penjual. **Tabel ini asumsi, bukan jaminan** — sebelum
wiring, cek label yang tercetak di PCB fisik dan sesuaikan `#define` di sketch
kalau beda.

| Fungsi | Pin | Kenapa |
|---|---|---|
| I2C SDA (ke MPU6050) | GPIO6 | |
| I2C SCL (ke MPU6050) | GPIO7 | |
| MPU INT (interrupt DMP) | GPIO5 | dipakai mulai STEP 2 |
| Tombol CAPTURE | GPIO3 | dipakai mulai STEP 4 |
| Tombol MODE | GPIO4 | dipakai mulai STEP 4 |

**Hindari GPIO2, GPIO8, GPIO9** — ini strapping pin (menentukan mode boot) di
ESP32-C3, dan GPIO8 sering dobel-fungsi jadi LED onboard di sebagian varian
SuperMini. Pakai untuk I/O bebas bisa bikin board gagal boot normal atau
konflik sama LED.

STEP 1 (sketch `wand_step1.ino`) **tidak pakai pin manapun di atas** — murni
WiFi + WSS. Tabel ini disiapkan buat STEP 2–4.

---

## 5. Wiring — SENSOR & TOMBOL (buat STEP 2–4, tes sekarang boleh disiapkan)

**Tes tahap ini pakai daya USB dari laptop, BUKAN baterai.** Baterai baru
masuk di §6.

- MPU6050 VCC → SuperMini **3V3**
- MPU6050 GND → SuperMini GND
- MPU6050 SDA → GPIO6
- MPU6050 SCL → GPIO7
- MPU6050 INT → GPIO5

Tombol (CAPTURE & MODE), masing-masing:
- Satu kaki → GPIO tombol tersebut (GPIO3 / GPIO4)
- Kaki lain → GND
- Firmware pakai `INPUT_PULLUP` (internal) → **tidak perlu resistor luar apapun**,
  tombol tinggal jembatani pin ke GND saat ditekan.

---

## 6. Wiring — DAYA (WAND FINAL, bukan buat fase tes breadboard)

Rangkaian ini baru relevan pas wand sudah mau dipakai lepas dari USB laptop
(dibawa jalan, dimainkan). Selama masih di breadboard buat tes STEP 1–3, pakai
USB saja — jangan rakit ini duluan.

- LiPo (+) → TP4056 **B+**
- LiPo (−) → TP4056 **B−**
- TP4056 **OUT+** → SuperMini pin **5V** (⚠️ **BUKAN 3V3** — SuperMini punya LDO
  yang nurunin 5V ke 3V3 buat chip; masuk langsung ke pin 3V3 dari sumber 5V =
  **overvolt, bisa merusak ESP32-C3**)
- TP4056 **OUT−** → SuperMini GND
- Cas: colok USB-C **di port TP4056** (bukan port SuperMini), harus diekspos
  keluar enclosure biar bisa dicas tanpa bongkar.
- Saklar SPST di jalur OUT+ → OPSIONAL, buat matiin wand tanpa cabut baterai.

### Catatan arus cas

TP4056 default cas di **1A** — ini **kegedean** buat LiPo pouch kecil (risiko
panas/stres sel). Dua opsi:

1. **Pilih baterai ≥500mAh** — 1A masih dalam batas wajar (≤2C) buat kapasitas
   segitu.
2. **Ganti resistor Rprog** di modul TP4056 kalau baterai lebih kecil dari itu:

   | Rprog | Arus cas kira-kira |
   |---|---|
   | ~2.4kΩ | ~500mA |
   | ~4.7kΩ | ~250mA |

   (Rumus modul: I_charge ≈ 1000 / R_prog(kΩ) mA — dua nilai di atas cukup buat
   kebanyakan LiPo pouch kecil.)

---

## 7. Roadmap STEP 1 → 4

1. **STEP 1 (sketch ini)** — transport only: WiFi + WSS ke relay, payload
   dummy `{t,q:[1,0,0,0],ts}` @~30Hz, tanpa sensor/tombol. Tujuan: buktiin TLS
   di ESP32 jalan & display.html bisa lihat wand sebagai controller.
2. **STEP 2** — colok MPU6050 via I2C, aktifkan DMP, baca quaternion 6-axis
   langsung dari chip (fusion di hardware, bukan di kode ESP32), forward apa
   adanya ke payload yang sama.
3. **STEP 3** — parity frame: pastikan sumbu-tunjuk fisik wand terbaca sebagai
   +Y device (biar match asumsi game), live-test di mode relative (yang
   memaafkan offset konstan — jadi nggak butuh presisi sempurna di step ini).
4. **STEP 4** — sambungkan 2 tombol fisik ke payload `{btn:"capture",...}` /
   `{btn:"mode",...}` identik dengan yang dikirim index.html, lalu enclosure
   cetak 3D.

---

## 8. Utang yang sengaja ditunda (jangan lupa sebelum "final")

- **TLS `setInsecure()`** — sketch STEP 1 connect WSS tanpa verifikasi
  sertifikat server (lihat komentar `TODO` di `wand_step1.ino`). Cukup buat
  de-risking transport sekarang; ganti ke CA cert ter-embed sebelum wand
  dipakai di luar jaringan tepercaya.
- **Pin map §4 tentatif** — cocokkan ke sablon board fisik begitu barang
  datang, sebelum wiring STEP 2.
