/*
 * ============================================================
 *   NUMORA ESP32 — BLE (Dashboard & Physical Button Toggle)
 *   Komunikasi via Bluetooth Low Energy (Web Bluetooth API)
 *   UPDATE: Penambahan Detail Audio & Tombol Arcade Fisik
 * ============================================================
 */

#include <SPI.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ==============================================================
//  UUID — harus identik dengan dashboard
// ==============================================================
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHAR_SENSOR_UUID    "12345678-1234-1234-1234-123456789001"
#define CHAR_CMD_UUID       "12345678-1234-1234-1234-123456789002"
#define CHAR_STATUS_UUID    "12345678-1234-1234-1234-123456789003"

// ==============================================================
//  PIN
// ==============================================================
#define TFT_CS      15
#define TFT_DC       2
#define TFT_RST      4
#define IR_PIN       5
#define S0          32
#define S1          25
#define S2          12
#define S3          13
#define OUT         14
#define DF_RX       26
#define DF_TX       27

// --- PIN TOMBOL ARCADE ---
#define BTN_BELAJAR 16
#define BTN_KUIS    17

// ==============================================================
//  AUDIO (Sesuai urutan file di SD Card)
// ==============================================================
#define AUDIO_OPENING            1
#define AUDIO_MERAH              2
#define AUDIO_HIJAU              3
#define AUDIO_BIRU               4
#define AUDIO_KUNING             5
#define AUDIO_INTRO_WARNA       16
#define AUDIO_INTRO_ANGKA       17

// --- AUDIO BARU SESUAI PERMINTAAN ---
#define AUDIO_BENAR             19 
#define AUDIO_SALAH             20 
#define AUDIO_INTRO_KUIS_WARNA  21
#define AUDIO_INTRO_KUIS_ANGKA  22
#define AUDIO_SOAL_MERAH        23
#define AUDIO_SOAL_HIJAU        24
#define AUDIO_SOAL_BIRU         25
#define AUDIO_SOAL_KUNING       26
#define AUDIO_SOAL_1            27
#define AUDIO_SOAL_2            28
#define AUDIO_SOAL_3            29
#define AUDIO_SOAL_4            30
#define AUDIO_SOAL_5            31
#define AUDIO_SOAL_6            32
#define AUDIO_SOAL_7            33
#define AUDIO_SOAL_8            34
#define AUDIO_SOAL_9            35
#define AUDIO_SOAL_10           36

// ==============================================================
//  WARNA LCD
// ==============================================================
#define CLR_MERAH   0xF800
#define CLR_HIJAU   0x07E0
#define CLR_BIRU    0x001F
#define CLR_KUNING  0xFFE0
#define CLR_UNGU    0x781F
#define CLR_ORANGE  0xFC60
#define CLR_HITAM   0x0000
#define CLR_PUTIH   0xFFFF
#define CLR_ABU     0x7BEF
#define CLR_GELAP   0x1082
#define BG_MERAH    0xF9A7
#define BG_HIJAU    0x87F0
#define BG_BIRU     0x8D7F
#define BG_KUNING   0xFFF0

#define SCR_W 320
#define SCR_H 240
#define SB_H   26
#define HEARTBEAT_MS 5000UL

// ==============================================================
//  OBJEK & STATE
// ==============================================================
Adafruit_ILI9341    tft(TFT_CS, TFT_DC, TFT_RST);
HardwareSerial      dfSerial(2);
DFRobotDFPlayerMini dfplayer;

BLEServer*          pServer   = nullptr;
BLECharacteristic*  pCharSensor = nullptr;
BLECharacteristic*  pCharStatus = nullptr;
bool                bleConnected = false;
bool                statusSebelumnya = false; 

volatile int        pendingScreen = 0; 

bool modeWarna    = true;
bool modeKuis     = false; 
bool isKuisWarna  = false; 
bool sesiBerjalan = false;

int  countWarna   = 0;
int  countAngka   = 0;
int  volume       = 20;
String lastWarna  = "";
int    lastAngka  = 0;

// Variabel Penahan Debounce Tombol Fisik
unsigned long lastBtnBelajarPress = 0;
unsigned long lastBtnKuisPress    = 0;

// Variabel Kuis
String targetKuisWarna  = ""; 
int    targetKuisAngka  = 0;
int    currentKuisAngka = 0;

// Variabel Trigger Audio Soal Setelah Intro
bool playSoalWarnaTarget = false;
bool playSoalAngkaTarget = false;

// Penyimpan Riwayat Terakhir Benar (Anti-kembar berturut-turut)
String terakhirBenarWarna = "";
int    terakhirBenarAngka = 0;

// Variabel Timer Senyap (Tanpa Display)
unsigned long lastKuisAngkaTick = 0;
bool          timerKuisAngkaAktif = false;
int           sisaWaktuKuis = 0;

unsigned long lastDetect    = 0;
unsigned long lastAudio     = 0;
unsigned long audioDur      = 0;
bool          audioPlaying  = false;
unsigned long lastHeartbeat = 0;
bool          dfOk          = false;

// ==============================================================
//  PROTOTYPE
// ==============================================================
void screenStatusBar();
void sendStatus(String status);
void screenMode(String mode);
void screenWarna(String warna); 
void screenAngka(int angka, int total);
void screenKuisWarna(String target, bool terjawab, bool benar);
void screenKuisAngka(int target, int current, bool terjawab, bool benar); 
void showFeedback(bool benar);
void screenSplash();
void screenSiap();
void screenTerhubung(); 
void handleModeKuis();
void handleKuisWarna();
void handleKuisAngka();
void handleModeWarna();
void handleModeAngka();
void playAudio(int track, unsigned long durMs);
void playWarna(String w);
void playSoalWarna(String w);
void playSoalAngka(int a);
String bacaWarna();
uint16_t getBgColor(String w);
uint16_t getFgColor(String w);
uint16_t getAccentColor(String w);
void flashEffect(uint16_t color);
void drawDots(int angka, int total);
void drawStars(uint16_t color, int count);

// Fungsi Pengacak Khusus Anti-Kembar
String acakWarnaBaru();
int acakAngkaBaru();

// ==============================================================
//  FUNGSI PENGACAK ANTI-KEMBAR
// ==============================================================
String acakWarnaBaru() {
  String warnaList[] = {"MERAH", "HIJAU", "BIRU", "KUNING"};
  String kandidat = "";
  // Cari terus secara acak sampai dapat warna yang berbeda dari warna sebelumnya
  do {
    kandidat = warnaList[random(0, 4)];
  } while (kandidat == terakhirBenarWarna);
  return kandidat;
}

int acakAngkaBaru() {
  int kandidat = 0;
  // Cari terus secara acak (1-10) sampai dapat angka yang berbeda dari angka sebelumnya
  do {
    kandidat = random(1, 11);
  } while (kandidat == terakhirBenarAngka);
  return kandidat;
}

// ==============================================================
//  BLE CALLBACKS
// ==============================================================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    bleConnected = true;
    Serial.println("[BLE] HP terhubung!");
  }
  void onDisconnect(BLEServer* s) override {
    bleConnected = false;
    sesiBerjalan = false; 
    Serial.println("[BLE] HP terputus — advertising ulang...");
    BLEDevice::startAdvertising();
  }
};

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pChar) override {
    String msg = pChar->getValue().c_str();
    Serial.println("[CMD] " + msg);

    StaticJsonDocument<128> doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;
    String cmd = doc["cmd"] | "";

    if (cmd == "belajar_warna") {
      modeWarna = true;
      modeKuis = false;
      countWarna = 0;
      lastWarna = "";
      sesiBerjalan = true;
      playAudio(AUDIO_INTRO_WARNA, 3000);
      pendingScreen = 1; 
      sendStatus("mode_warna");
    }
    else if (cmd == "belajar_angka") {
      modeWarna = false;
      modeKuis = false;
      countAngka = 0;
      lastAngka = 0;
      sesiBerjalan = true;
      playAudio(AUDIO_INTRO_ANGKA, 3000);
      pendingScreen = 2; 
      sendStatus("mode_angka");
    }
    else if (cmd == "kuis_warna") {
      modeWarna = false;
      modeKuis = true;
      isKuisWarna = true;
      sesiBerjalan = true;
      
      // Reset riwayat dan acak warna pertama
      terakhirBenarWarna = ""; 
      targetKuisWarna = acakWarnaBaru();
      
      playAudio(AUDIO_INTRO_KUIS_WARNA, 3000);
      playSoalWarnaTarget = true; // Trigger untuk memutar suara spesifik warna setelah intro
      
      pendingScreen = 3; 
      sendStatus("mode_kuis_warna");
    }
    else if (cmd == "kuis_angka") {
      modeWarna = false;
      modeKuis = true;
      isKuisWarna = false;
      sesiBerjalan = true;
      
      // Reset riwayat dan acak angka pertama
      terakhirBenarAngka = 0;
      targetKuisAngka = acakAngkaBaru(); 
      currentKuisAngka = 0;
      timerKuisAngkaAktif = false;
      sisaWaktuKuis = 0;
      
      playAudio(AUDIO_INTRO_KUIS_ANGKA, 3000);
      playSoalAngkaTarget = true; // Trigger untuk memutar suara spesifik angka setelah intro
      
      pendingScreen = 4; 
      sendStatus("mode_kuis_angka");
    }
    else if (cmd == "selesai") {
      sesiBerjalan = false;
      pendingScreen = 5; 
      sendStatus("idle");
    }
    else if (cmd == "volume") {
      int vol = doc["value"] | 20;
      vol = constrain(vol, 0, 30);
      volume = vol;
      if (dfOk) dfplayer.volume(vol);
      pendingScreen = 6; 
      sendStatus("volume_ok");
    }
  }
};

// ==============================================================
//  KIRIM DATA SENSOR & STATUS
// ==============================================================
int pendingAudio = 0;
unsigned long pendingAudioTs = 0;
// Titip nomor track di paket sensor (dipakai jika notifikasi status terpisah hilang)
void attachAudio(JsonDocument& doc) {
  if (pendingAudio > 0 && millis() - pendingAudioTs < 1500) doc["audio"] = pendingAudio;
  pendingAudio = 0;
}

void sendSensor(String mode, String value, int count) {
  if (!bleConnected || !pCharSensor) return;
  StaticJsonDocument<200> doc;
  doc["mode"]  = mode;
  doc["value"] = value;
  doc["count"] = count;
  doc["ts"]    = millis();
  attachAudio(doc);
  char buf[200];
  serializeJson(doc, buf);
  pCharSensor->setValue(buf);
  pCharSensor->notify();
}

void sendSensorAngka(int value, int count) {
  if (!bleConnected || !pCharSensor) return;
  StaticJsonDocument<200> doc;
  doc["mode"]  = "angka";
  doc["value"] = value;
  doc["count"] = count;
  doc["ts"]    = millis();
  attachAudio(doc);
  char buf[200];
  serializeJson(doc, buf);
  pCharSensor->setValue(buf);
  pCharSensor->notify();
}

void sendSensorKuis(String mode, String expected, String value, bool benar) {
  if (!bleConnected || !pCharSensor) return;
  StaticJsonDocument<200> doc;
  doc["mode"]     = mode;
  doc["expected"] = expected; // Mengirimkan target kuis
  doc["value"]    = value;
  doc["result"]   = benar ? "benar" : "salah";
  doc["correct"]  = benar;
  doc["ts"]       = millis();
  
  char buf[200];
  serializeJson(doc, buf);
  pCharSensor->setValue(buf);
  pCharSensor->notify();
}

void sendStatus(String status) {
  if (!bleConnected || !pCharStatus) return;
  StaticJsonDocument<200> doc;
  doc["status"] = status;
  if(modeKuis) {
    doc["mode"] = isKuisWarna ? "kuis_warna" : "kuis_angka";
  } else {
    doc["mode"] = modeWarna ? "warna" : "angka";
  }
  if(!sesiBerjalan && status == "idle") doc["status"] = "idle";
  
  doc["volume"] = volume;
  doc["ts"]     = millis();
  
  char buf[200];
  serializeJson(doc, buf); 
  
  pCharStatus->setValue(buf);
  pCharStatus->notify();
}

// ==============================================================
//  SETUP & BLE INIT
// ==============================================================
void setupBLE() {
  tft.fillRect(20, 155, 280, 50, CLR_HITAM); 
  tft.setTextColor(CLR_GELAP); tft.setTextSize(2);
  tft.setCursor(92, 172); tft.println("Setup BLE..."); 
  tft.setTextColor(CLR_KUNING); tft.setTextSize(2);
  tft.setCursor(90, 170); tft.println("Setup BLE...");

  BLEDevice::init("NUMORA BLE");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharSensor = pService->createCharacteristic(
    CHAR_SENSOR_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharSensor->addDescriptor(new BLE2902());

  BLECharacteristic* pCharCmd = pService->createCharacteristic(
    CHAR_CMD_UUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pCharCmd->setCallbacks(new CmdCallbacks());

  pCharStatus = pService->createCharacteristic(
    CHAR_STATUS_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pCharStatus->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID);
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  BLEDevice::startAdvertising();

  Serial.println("[BLE] Advertising: NUMORA BLE");
  
  tft.fillRect(20, 160, 280, 45, CLR_HITAM);
  tft.setTextColor(CLR_GELAP); tft.setTextSize(1);
  tft.setCursor(69, 177); tft.println("BLE OK! Scan 'NUMORA-BLE' di HP"); 
  tft.setTextColor(CLR_HIJAU); tft.setTextSize(1);
  tft.setCursor(67, 175); tft.println("BLE OK! Scan 'NUMORA-BLE' di HP"); 
  
  screenStatusBar();
  delay(2200);
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0)); 
  
  tft.begin();
  tft.setRotation(1);
  screenSplash(); 

  // Inisialisasi Tombol Fisik
  pinMode(BTN_BELAJAR, INPUT_PULLUP);
  pinMode(BTN_KUIS, INPUT_PULLUP);

  pinMode(IR_PIN, INPUT_PULLUP);
  pinMode(S0, OUTPUT); pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT); pinMode(S3, OUTPUT);
  pinMode(OUT, INPUT);
  digitalWrite(S0, HIGH); digitalWrite(S1, LOW);

  dfSerial.begin(9600, SERIAL_8N1, DF_RX, DF_TX);
  delay(500);
  if (dfplayer.begin(dfSerial)) {
    dfOk = true;
    dfplayer.volume(volume);
    dfplayer.EQ(DFPLAYER_EQ_NORMAL);
    Serial.println("[DF] OK");
  } else {
    Serial.println("[DF] GAGAL");
  }

  setupBLE();

  playAudio(AUDIO_OPENING, 3500);
  screenSiap(); 
}

// ==============================================================
//  LOOP UTAMA 
// ==============================================================
void loop() {
  if (bleConnected != statusSebelumnya) {
    statusSebelumnya = bleConnected;
    if (bleConnected) {
      delay(2000); 
      screenTerhubung();
      sendStatus("connected"); 
    }
    else {
      delay(1000); 
      screenSiap();
    }
  }

  // -------------------------------------------------------------
  // EVENT: TOMBOL FISIK 1 - MODE BELAJAR (GPIO 16)
  // -------------------------------------------------------------
  if (digitalRead(BTN_BELAJAR) == LOW && millis() - lastBtnBelajarPress > 400) {
    lastBtnBelajarPress = millis();
    
    // Toggle/Ganti antara Belajar Warna & Belajar Angka
    if (!modeKuis && modeWarna) {
      // Pindah ke Belajar Angka
      modeWarna = false;
      modeKuis = false;
      countAngka = 0;
      lastAngka = 0;
      sesiBerjalan = true;
      playAudio(AUDIO_INTRO_ANGKA, 3000);
      pendingScreen = 2;
      sendStatus("mode_angka");
    } else {
      // Pindah ke Belajar Warna
      modeWarna = true;
      modeKuis = false;
      countWarna = 0;
      lastWarna = "";
      sesiBerjalan = true;
      playAudio(AUDIO_INTRO_WARNA, 3000);
      pendingScreen = 1;
      sendStatus("mode_warna");
    }
  }

  // -------------------------------------------------------------
  // EVENT: TOMBOL FISIK 2 - MODE KUIS (GPIO 17)
  // -------------------------------------------------------------
  if (digitalRead(BTN_KUIS) == LOW && millis() - lastBtnKuisPress > 400) {
    lastBtnKuisPress = millis();
    
    // Toggle/Ganti antara Kuis Warna & Kuis Angka
    if (modeKuis && isKuisWarna) {
      // Pindah ke Kuis Angka
      modeWarna = false;
      modeKuis = true;
      isKuisWarna = false;
      sesiBerjalan = true;
      
      terakhirBenarAngka = 0;
      targetKuisAngka = acakAngkaBaru();
      currentKuisAngka = 0;
      timerKuisAngkaAktif = false;
      sisaWaktuKuis = 0;
      
      playAudio(AUDIO_INTRO_KUIS_ANGKA, 3000);
      playSoalAngkaTarget = true;
      
      pendingScreen = 4;
      sendStatus("mode_kuis_angka");
    } else {
      // Pindah ke Kuis Warna
      modeWarna = false;
      modeKuis = true;
      isKuisWarna = true;
      sesiBerjalan = true;
      
      terakhirBenarWarna = "";
      targetKuisWarna = acakWarnaBaru();
      
      playAudio(AUDIO_INTRO_KUIS_WARNA, 3000);
      playSoalWarnaTarget = true;
      
      pendingScreen = 3;
      sendStatus("mode_kuis_warna");
    }
  }

  if (pendingScreen != 0) {
    int activeScreen = pendingScreen;
    pendingScreen = 0; 
    
    if (activeScreen == 1) screenMode("belajar_warna");
    else if (activeScreen == 2) screenMode("belajar_angka");
    else if (activeScreen == 3) screenKuisWarna(targetKuisWarna, false, false);
    else if (activeScreen == 4) screenKuisAngka(targetKuisAngka, currentKuisAngka, false, false);
    else if (activeScreen == 5) screenTerhubung();
    else if (activeScreen == 6) screenStatusBar();
  }

  if (bleConnected && millis() - lastHeartbeat > HEARTBEAT_MS) {
    lastHeartbeat = millis();
    sendStatus("heartbeat");
  }

  if (audioPlaying && millis() - lastAudio >= audioDur) {
    audioPlaying = false;
    
    // Putar suara soal persis setelah suara intro kuis selesai
    if (playSoalWarnaTarget) {
      playSoalWarnaTarget = false;
      playSoalWarna(targetKuisWarna);
    } 
    else if (playSoalAngkaTarget) {
      playSoalAngkaTarget = false;
      playSoalAngka(targetKuisAngka);
    }
  }

  // -------------------------------------------------------------
  // EVENT: TIMER SENYAP (KUIS ANGKA)
  // -------------------------------------------------------------
  if (sesiBerjalan && modeKuis && !isKuisWarna && timerKuisAngkaAktif && !audioPlaying) {
    if (millis() - lastKuisAngkaTick >= 1000) {
      lastKuisAngkaTick += 1000;
      sisaWaktuKuis--;
      
      if (sisaWaktuKuis <= 0) {
        timerKuisAngkaAktif = false;
        
        bool benar = (currentKuisAngka == targetKuisAngka);
        Serial.println("[KUIS ANGKA] Waktu Habis. Hasil: " + String(benar ? "BENAR" : "SALAH"));
        
        sendSensorKuis("kuis_angka", String(targetKuisAngka), String(currentKuisAngka), benar);
        screenKuisAngka(targetKuisAngka, currentKuisAngka, true, benar);
        
        if (benar) {
          playAudio(AUDIO_BENAR, 2000);
          delay(2500);
          
          terakhirBenarAngka = targetKuisAngka;
          targetKuisAngka = acakAngkaBaru();
          
          // Putar soal angka target yang baru
          playSoalAngka(targetKuisAngka);
        } else {
          playAudio(AUDIO_SALAH, 2000);
          delay(2500);
        }
        
        currentKuisAngka = 0;
        screenKuisAngka(targetKuisAngka, currentKuisAngka, false, false);
      }
    }
  }

  // -------------------------------------------------------------
  // EVENT: PEMBACAAN SENSOR IR
  // -------------------------------------------------------------
  if (sesiBerjalan && !audioPlaying) {
    if (digitalRead(IR_PIN) == LOW && millis() - lastDetect > 900) {
      lastDetect = millis();
      if (modeKuis) handleModeKuis();
      else if (modeWarna) handleModeWarna();
      else handleModeAngka();
    }
  }
}

// ==============================================================
//  BACA WARNA & HANDLER
// ==============================================================
String bacaWarna() {
  delay(250);
  digitalWrite(S2, LOW);  digitalWrite(S3, LOW);
  long r = pulseIn(OUT, LOW, 50000); delay(20);
  digitalWrite(S2, HIGH); digitalWrite(S3, HIGH);
  long g = pulseIn(OUT, LOW, 50000); delay(20);
  digitalWrite(S2, LOW);  digitalWrite(S3, HIGH);
  long b = pulseIn(OUT, LOW, 50000); delay(20);

  if (r < b && g < b && abs(r - g) < 60) return "KUNING";
  if (b < r && b < g)                    return "BIRU";
  if (g < r && g < b)                    return "HIJAU";
  if (r < g && r < b)                    return "MERAH";
  
  return "KUNING";
}

void handleModeWarna() {
  String warna = bacaWarna();
  countWarna++; 
  lastWarna = warna;
  flashEffect(getBgColor(warna));
  screenWarna(warna); 
  playWarna(warna);
  sendSensor("warna", warna, countWarna);
}

void handleModeAngka() {
  countAngka++;
  lastAngka = countAngka;
  
  flashEffect(CLR_HIJAU);
  screenAngka(countAngka, 10);
  
  if (countAngka >= 1 && countAngka <= 10) {
    playAudio(5 + countAngka, 2500);
  }
  
  sendSensorAngka(countAngka, countAngka);

  if (countAngka >= 10) countAngka = 0;
}

void handleModeKuis() {
  if (isKuisWarna) handleKuisWarna();
  else handleKuisAngka();
}

void handleKuisWarna() {
  String warnaTerdeteksi = bacaWarna();
  Serial.println("[KUIS WARNA] Benda: " + warnaTerdeteksi + " (Target: " + targetKuisWarna + ")");
  
  bool benar = (warnaTerdeteksi == targetKuisWarna);
  screenKuisWarna(targetKuisWarna, true, benar);
  
  sendSensorKuis("kuis_warna", targetKuisWarna, warnaTerdeteksi, benar);

  if (benar) {
    playAudio(AUDIO_BENAR, 2000);
    delay(2500);
    
    terakhirBenarWarna = targetKuisWarna;
    targetKuisWarna = acakWarnaBaru();
    
    // Putar soal warna target yang baru
    playSoalWarna(targetKuisWarna);
    
    screenKuisWarna(targetKuisWarna, false, false);
  } else {
    playAudio(AUDIO_SALAH, 2000);
    delay(2500);
    screenKuisWarna(targetKuisWarna, false, false); 
  }
}

void handleKuisAngka() {
  currentKuisAngka++;
  Serial.println("[KUIS ANGKA] Scan ke-" + String(currentKuisAngka) + " (Target: " + String(targetKuisAngka) + ")");
  
  if (currentKuisAngka > targetKuisAngka) {
    timerKuisAngkaAktif = false; 
    
    sendSensorKuis("kuis_angka", String(targetKuisAngka), String(currentKuisAngka), false);
    screenKuisAngka(targetKuisAngka, currentKuisAngka, true, false);
    
    playAudio(AUDIO_SALAH, 2000);
    delay(2500);
    
    currentKuisAngka = 0;
    sisaWaktuKuis = 0;
    screenKuisAngka(targetKuisAngka, currentKuisAngka, false, false);
    
  } else {
    timerKuisAngkaAktif = true;
    sisaWaktuKuis = 7;
    lastKuisAngkaTick = millis();
    
    screenKuisAngka(targetKuisAngka, currentKuisAngka, false, false);
  }
}

// Kirim nomor track ke web agar web ikut memutar audio yang sama
void sendAudioEvent(int track) {
  Serial.printf("[AUDIO->WEB] track=%d bleConnected=%d\n", track, (int)bleConnected);
  if (!bleConnected || !pCharStatus) return;
  StaticJsonDocument<64> doc;
  doc["audio"] = track;
  doc["ts"]    = millis();
  char buf[64];
  serializeJson(doc, buf);
  // Jeda agar notifikasi BLE sebelumnya (sensor/status) sempat terkirim,
  // kalau beruntun tanpa jeda notifikasi ini bisa hilang.
  delay(100);
  pCharStatus->setValue(buf);
  pCharStatus->notify();
  delay(100);
}

void playAudio(int track, unsigned long durMs) {
  pendingAudio = track; pendingAudioTs = millis();
  sendAudioEvent(track);
  if (!dfOk) return;
  dfplayer.play(track); audioPlaying = true; lastAudio = millis(); audioDur = durMs;
}

void playWarna(String w) {
  if      (w == "MERAH") playAudio(AUDIO_MERAH,  2500);
  else if (w == "HIJAU") playAudio(AUDIO_HIJAU,  2500);
  else if (w == "BIRU")  playAudio(AUDIO_BIRU,   2500);
  else                   playAudio(AUDIO_KUNING, 2500);
}

// Fungsi Bantuan untuk Putar Soal Kuis
void playSoalWarna(String w) {
  if      (w == "MERAH")  playAudio(AUDIO_SOAL_MERAH, 2500);
  else if (w == "HIJAU")  playAudio(AUDIO_SOAL_HIJAU, 2500);
  else if (w == "BIRU")   playAudio(AUDIO_SOAL_BIRU, 2500);
  else if (w == "KUNING") playAudio(AUDIO_SOAL_KUNING, 2500);
}

void playSoalAngka(int a) {
  if (a >= 1 && a <= 10) {
    // Memanfaatkan urutan: AUDIO_SOAL_1 adalah 27, AUDIO_SOAL_2 adalah 28, dsb.
    playAudio(AUDIO_SOAL_1 + (a - 1), 2500);
  }
}

// ==============================================================
//  GRAPHICS HELPERS & SCREENS
// ==============================================================
uint16_t getBgColor(String w) {
  if (w == "MERAH") return BG_MERAH;
  if (w == "HIJAU") return BG_HIJAU;
  if (w == "BIRU")  return BG_BIRU;
  return BG_KUNING;
}
uint16_t getFgColor(String w) { return w == "KUNING" ? CLR_HITAM : CLR_PUTIH; }
uint16_t getAccentColor(String w) {
  if (w == "MERAH") return CLR_MERAH;
  if (w == "HIJAU") return CLR_HIJAU;
  if (w == "BIRU")  return CLR_BIRU;
  return CLR_ORANGE; 
}

void flashEffect(uint16_t color) {
  tft.fillScreen(color); delay(60);
  tft.fillScreen(CLR_PUTIH); delay(40);
  tft.fillScreen(color); delay(40);
}

void drawDots(int angka, int total) {
  int dotR = 10, gap = 12, cols = 5;
  int startX = (SCR_W - (cols*(dotR*2+gap) - gap)) / 2;
  int startY = 135; 
  for (int i = 0; i < total; i++) {
    int col = i % cols, row = i / cols;
    int cx = startX + col*(dotR*2+gap) + dotR;
    int cy = startY + row*(dotR*2+gap) + dotR;
    
    if (i < angka) { 
       tft.fillCircle(cx, cy, dotR, CLR_ORANGE); 
       tft.drawCircle(cx, cy, dotR, CLR_PUTIH); 
       tft.fillCircle(cx - 3, cy - 3, 3, CLR_PUTIH); 
    } else {             
       tft.fillCircle(cx, cy, dotR, CLR_GELAP);
       tft.drawCircle(cx, cy, dotR, CLR_ABU);
    }
  }
}

void drawStars(uint16_t color, int count) {
  uint16_t pos[][2] = {{15,15},{300,20},{30,50},{280,40},{15,80},{305,90},{25,180},{295,180},{50,15},{260,10}};
  for (int i = 0; i < min(count, 10); i++) {
    tft.fillCircle(pos[i][0], pos[i][1], 2, color);
    tft.drawPixel(pos[i][0], pos[i][1]-3, color);
    tft.drawPixel(pos[i][0], pos[i][1]+3, color);
    tft.drawPixel(pos[i][0]-3, pos[i][1], color);
    tft.drawPixel(pos[i][0]+3, pos[i][1], color);
  }
}

void screenStatusBar() {
  int sy = SCR_H - SB_H;
  tft.fillRect(0, sy, SCR_W, SB_H, CLR_GELAP);
  tft.drawFastHLine(0, sy, SCR_W, CLR_ABU);
  tft.setTextSize(1);

  tft.setCursor(6, sy + 8);
  if (bleConnected) { tft.setTextColor(CLR_HIJAU); tft.print("BLE:KONEK"); }
  else              { tft.setTextColor(CLR_KUNING); tft.print("BLE:SCAN.."); }

  tft.setCursor(SCR_W - 52, sy + 8);
  tft.setTextColor(CLR_ABU);
  tft.print("VOL:"); tft.setTextColor(CLR_PUTIH); tft.print(volume);
}

void showFeedback(bool benar) {
  int16_t x1, y1;
  uint16_t w, h;
  String txt = benar ? "BENAR" : "SALAH"; 
  
  if (benar) {
    tft.fillScreen(CLR_HIJAU);
    drawStars(CLR_KUNING, 10);
    tft.fillCircle(SCR_W/2, 95, 45, CLR_PUTIH);
    for(int i = -3; i <= 3; i++) {
      tft.drawLine(140+i, 95, 155+i, 115, CLR_HIJAU);
      tft.drawLine(155+i, 115, 185+i, 75, CLR_HIJAU);
    }
    tft.setTextColor(CLR_PUTIH);
  } else {
    tft.fillScreen(CLR_MERAH);
    drawStars(CLR_KUNING, 10);
    tft.fillCircle(SCR_W/2, 95, 45, CLR_PUTIH);
    for(int i = -3; i <= 3; i++) {
      tft.drawLine(140+i, 75, 180+i, 115, CLR_MERAH);
      tft.drawLine(180+i, 75, 140+i, 115, CLR_MERAH);
    }
    tft.setTextColor(CLR_PUTIH);
  }
  
  tft.setTextSize(4);
  tft.getTextBounds(txt, 0, 160, &x1, &y1, &w, &h);
  
  int textX = (SCR_W - w) / 2; 
  tft.setCursor(textX, 160);
  tft.println(txt);
}

void screenSiap() {
  tft.fillScreen(CLR_HITAM);
  drawStars(CLR_ABU, 12);
  
  tft.drawRect(4, 4, SCR_W-8, SCR_H-SB_H-8, CLR_UNGU);
  tft.drawRect(6, 6, SCR_W-12, SCR_H-SB_H-12, CLR_UNGU);
  tft.fillRect(6, 6, 8, 8, CLR_ORANGE);
  tft.fillRect(SCR_W-14, 6, 8, 8, CLR_ORANGE);
  tft.fillRect(6, SCR_H-SB_H-14, 8, 8, CLR_ORANGE);
  tft.fillRect(SCR_W-14, SCR_H-SB_H-14, 8, 8, CLR_ORANGE);
  
  tft.fillRoundRect(50, 25, SCR_W-100, 40, 8, CLR_GELAP);
  tft.drawRoundRect(50, 25, SCR_W-100, 40, 8, CLR_UNGU);

  tft.setTextColor(CLR_MERAH); tft.setTextSize(3);
  tft.setCursor(72, 35); tft.println("NUMORA BLE"); 
  tft.setTextColor(CLR_KUNING); tft.setTextSize(3);
  tft.setCursor(70, 33); tft.println("NUMORA BLE"); 
  
  tft.drawFastHLine(40, 75, 240, CLR_UNGU);
  
  tft.setTextColor(CLR_PUTIH); tft.setTextSize(1);
  tft.setCursor(50, 90);  tft.println("1. Buka Chrome di HP Anda");
  tft.setCursor(50, 110); tft.println("2. Scan Barcode NUMORA BLE");
  tft.setCursor(50, 130); tft.println("3. Klik Hubungkan BLE");
  tft.setCursor(50, 150); tft.println("4. Pilih 'NUMORA BLE'");
  
  tft.drawFastHLine(40, 170, 240, CLR_UNGU);
  
  tft.setTextColor(CLR_GELAP); tft.setTextSize(2);
  tft.setCursor(92, 182); tft.println("Menunggu..."); 
  tft.setTextColor(CLR_KUNING); tft.setTextSize(2);
  tft.setCursor(90, 180); tft.println("Menunggu..."); 
  
  screenStatusBar();
}

void screenTerhubung() {
  tft.fillScreen(CLR_HITAM);
  drawStars(CLR_ABU, 12); 
  
  tft.drawRect(4, 4, SCR_W-8, SCR_H-SB_H-8, CLR_HIJAU);
  tft.drawRect(6, 6, SCR_W-12, SCR_H-SB_H-12, CLR_HIJAU);
  tft.fillRect(6, 6, 8, 8, CLR_KUNING);
  tft.fillRect(SCR_W-14, 6, 8, 8, CLR_KUNING);
  tft.fillRect(6, SCR_H-SB_H-14, 8, 8, CLR_KUNING);
  tft.fillRect(SCR_W-14, SCR_H-SB_H-14, 8, 8, CLR_KUNING);

  tft.fillRoundRect(30, 45, SCR_W-60, 45, 8, CLR_GELAP);
  tft.drawRoundRect(30, 45, SCR_W-60, 45, 8, CLR_HIJAU);
  
  tft.setTextColor(CLR_MERAH); tft.setTextSize(3);
  tft.setCursor(54, 57); tft.println("SIAP BERMAIN");  
  tft.setTextColor(CLR_KUNING); tft.setTextSize(3);
  tft.setCursor(52, 55); tft.println("SIAP BERMAIN");  
  
  tft.drawFastHLine(60, 105, 200, CLR_UNGU);
  
  tft.setTextColor(CLR_PUTIH); tft.setTextSize(1);
  tft.setCursor(85, 117); 
  tft.println("Pilih mode dengan menekan");
  tft.setCursor(85, 131); 
  tft.println("tombol mode untuk memulai");
  
  tft.drawFastHLine(60, 150, 200, CLR_UNGU);

  uint16_t cc[] = {CLR_MERAH, CLR_KUNING, CLR_HIJAU, CLR_BIRU};
  int startX = (SCR_W - (4 * 40)) / 2 + 20; 
  
  for (int i = 0; i < 4; i++) {
    tft.fillCircle(startX + i*40 + 2, 178 + 2, 12, CLR_GELAP);
    tft.fillCircle(startX + i*40, 178, 12, cc[i]);
    tft.drawCircle(startX + i*40, 178, 12, CLR_PUTIH);
    tft.fillCircle(startX + i*40 - 4, 178 - 4, 3, CLR_PUTIH);
  }
  
  screenStatusBar();
}

void screenMode(String mode) {
  mode.trim();
  mode.toLowerCase();

  if (mode == "belajar_warna") {
    tft.fillScreen(BG_BIRU);
    drawStars(CLR_KUNING, 12);
    tft.drawRect(4, 4, SCR_W-8, SCR_H-SB_H-8, CLR_PUTIH);
    tft.drawRect(6, 6, SCR_W-12, SCR_H-SB_H-12, CLR_PUTIH);
    
    tft.fillRoundRect(20, 15, 280, 45, 8, CLR_PUTIH);
    tft.drawRoundRect(20, 15, 280, 45, 8, CLR_BIRU);

    tft.setTextSize(3);
    tft.setTextColor(CLR_BIRU); tft.setCursor(43, 27); tft.println("BELAJAR WARNA");
    
    uint16_t warnas[4] = {CLR_MERAH, CLR_ORANGE, CLR_HIJAU, CLR_BIRU};
    int xpos[4] = {85, 135, 185, 235};
    for(int i = 0; i < 4; i++){
      tft.fillCircle(xpos[i], 110, 22, warnas[i]);
      tft.drawCircle(xpos[i], 110, 22, CLR_PUTIH);
      tft.fillCircle(xpos[i] - 8, 110 - 8, 6, CLR_PUTIH); 
      tft.fillCircle(xpos[i] - 12, 110 - 2, 2, CLR_PUTIH); 
    }
    
    tft.setTextSize(2); tft.setTextColor(CLR_HITAM);
    tft.setCursor(16, 160); tft.println("Mari kita mengenal warna");
  } 
  else if (mode == "belajar_angka") {
    tft.fillScreen(BG_KUNING);
    drawStars(CLR_ORANGE, 12);
    tft.drawRect(4, 4, SCR_W-8, SCR_H-SB_H-8, CLR_MERAH);
    tft.drawRect(6, 6, SCR_W-12, SCR_H-SB_H-12, CLR_MERAH);
    
    tft.fillRoundRect(20, 15, 280, 45, 8, CLR_PUTIH);
    tft.drawRoundRect(20, 15, 280, 45, 8, CLR_MERAH);

    tft.setTextSize(3);
    tft.setTextColor(CLR_MERAH); tft.setCursor(43, 27); tft.println("BELAJAR ANGKA");
    
    tft.setTextSize(5); 
    tft.setTextColor(CLR_MERAH);  tft.setCursor(105, 95); tft.print("1");
    tft.setTextColor(CLR_ORANGE); tft.setCursor(145, 95); tft.print("2");
    tft.setTextColor(CLR_BIRU);   tft.setCursor(185, 95); tft.print("3");
    
    tft.setTextSize(2); tft.setTextColor(CLR_HITAM);
    tft.setCursor(70, 150); tft.println("Mari menghitung");
    tft.setCursor(88, 170); tft.println("jumlah angka");
  } 
  
  screenStatusBar();
}

void screenWarna(String warna) {
  uint16_t bgColor = getBgColor(warna);
  uint16_t fgColor = getFgColor(warna);
  uint16_t accent  = getAccentColor(warna);
  
  tft.fillScreen(bgColor);
  
  tft.drawRect(8, 8, SCR_W-16, SCR_H-SB_H-16, fgColor);
  tft.drawRect(10, 10, SCR_W-20, SCR_H-SB_H-20, fgColor);
  drawStars(fgColor, 8);
  
  int radius = 55;
  int cx = 80; 
  int cy = (SCR_H - SB_H) / 2;
  
  tft.fillCircle(cx, cy, radius, accent);
  tft.drawCircle(cx, cy, radius, fgColor);
  tft.drawCircle(cx, cy, radius - 1, fgColor); 
  
  tft.fillCircle(cx - 20, cy - 20, 12, CLR_PUTIH);
  tft.fillCircle(cx - 28, cy - 10, 4, CLR_PUTIH); 
  
  tft.setTextSize(4);
  int textX = 155;
  int textY = cy - 14;
  
  uint16_t textShadow = CLR_GELAP;
  uint16_t textMain = fgColor;
  
  if (warna == "KUNING" || warna == "BIRU" || warna == "HIJAU") {
    textMain = CLR_HITAM;
    textShadow = CLR_PUTIH;
  }
  
  tft.setTextColor(textShadow); 
  tft.setCursor(textX + 3, textY + 3); 
  tft.print(warna);
  
  tft.setTextColor(textMain); 
  tft.setCursor(textX, textY);
  tft.print(warna);
  
  tft.fillCircle(textX + 15, textY - 25, 2, fgColor);
  tft.drawCircle(textX + 15, textY - 25, 4, fgColor);
  tft.fillCircle(textX + 80, textY + 45, 3, CLR_PUTIH);
  tft.drawCircle(textX + 80, textY + 45, 6, fgColor);
  
  screenStatusBar();
}

void screenAngka(int angka, int total) {
  tft.fillScreen(BG_BIRU); 
  
  tft.drawRect(8, 8, SCR_W-16, SCR_H-SB_H-16, CLR_PUTIH);
  tft.drawRect(10, 10, SCR_W-20, SCR_H-SB_H-20, CLR_PUTIH);
  drawStars(CLR_KUNING, 8);
  
  int cx = SCR_W / 2;
  int cy = 70;
  int r = 45;
  
  tft.fillCircle(cx, cy, r, CLR_KUNING); 
  tft.drawCircle(cx, cy, r, CLR_PUTIH);
  tft.drawCircle(cx, cy, r - 1, CLR_PUTIH);
  tft.drawCircle(cx, cy, r + 1, CLR_ORANGE); 
  
  tft.fillCircle(cx - 18, cy - 20, 10, CLR_PUTIH);
  tft.fillCircle(cx - 26, cy - 8, 4, CLR_PUTIH);
  
  tft.setTextSize(7);
  int offsetX = (angka >= 10) ? 40 : 20; 
  int offsetY = cy - 25; 
  
  tft.setTextColor(CLR_GELAP); 
  tft.setCursor(cx - offsetX + 4, offsetY + 4); 
  tft.print(angka);
  
  tft.setTextColor(CLR_MERAH); 
  tft.setCursor(cx - offsetX, offsetY); 
  tft.print(angka);
  
  tft.fillCircle(cx + 50, cy - 25, 3, CLR_PUTIH);
  tft.drawCircle(cx + 50, cy - 25, 6, CLR_KUNING);
  tft.fillCircle(cx - 55, cy + 15, 2, CLR_PUTIH);
  tft.drawCircle(cx - 55, cy + 15, 4, CLR_KUNING);
  
  drawDots(angka, total);
  screenStatusBar();
}

void screenKuisWarna(String target, bool terjawab, bool benar) {
  if (!terjawab) {
    tft.fillScreen(BG_BIRU);
    tft.drawRect(8, 8, SCR_W-16, SCR_H-SB_H-16, CLR_PUTIH);
    drawStars(CLR_KUNING, 10);

    tft.fillRoundRect(20, 15, 280, 45, 8, CLR_PUTIH);
    tft.drawRoundRect(20, 15, 280, 45, 8, CLR_BIRU);
    tft.setTextColor(CLR_BIRU); tft.setTextSize(3);
    tft.setCursor(70, 27); tft.println("KUIS WARNA");

    tft.setTextColor(CLR_HITAM); tft.setTextSize(2);
    tft.setCursor(22, 75); tft.println("Masukkan bola berwarna:");

    uint16_t cardColor = getAccentColor(target); 
    uint16_t textColor = getFgColor(target); 

    tft.fillRoundRect(60, 105, SCR_W-120, 60, 10, cardColor);
    tft.drawRoundRect(60, 105, SCR_W-120, 60, 10, CLR_PUTIH);
    tft.drawRoundRect(62, 107, SCR_W-124, 56, 8, CLR_PUTIH);

    tft.fillCircle(75, 120, 4, CLR_PUTIH);
    tft.fillCircle(85, 120, 2, CLR_PUTIH);

    int tx = 110;
    if (target == "MERAH") tx = 115;
    else if (target == "BIRU") tx = 125;
    else if (target == "HIJAU") tx = 115;
    else if (target == "KUNING") tx = 105;

    tft.setTextColor(textColor); tft.setTextSize(3);
    tft.setCursor(tx, 123); tft.println(target);

  } else {
    showFeedback(benar);
  }
  screenStatusBar();
}

void screenKuisAngka(int target, int current, bool terjawab, bool benar) {
  if (!terjawab) {
    tft.fillScreen(BG_KUNING);
    tft.drawRect(8, 8, SCR_W-16, SCR_H-SB_H-16, CLR_MERAH);
    drawStars(CLR_ORANGE, 10);

    tft.fillRoundRect(20, 15, 280, 45, 8, CLR_PUTIH);
    tft.drawRoundRect(20, 15, 280, 45, 8, CLR_MERAH);
    tft.setTextColor(CLR_MERAH); tft.setTextSize(3);
    tft.setCursor(70, 27); tft.println("KUIS ANGKA");

    tft.setTextColor(CLR_HITAM); tft.setTextSize(2);
    tft.setCursor(22, 70); tft.println("Masukkan bola sebanyak:");

    int cx = 160; int cy = 125; int r = 35;
    tft.fillCircle(cx, cy, r, CLR_MERAH);
    tft.drawCircle(cx, cy, r, CLR_PUTIH);
    tft.drawCircle(cx, cy, r-1, CLR_PUTIH);

    int offset = (target >= 10) ? 28 : 14; 
    tft.setTextColor(CLR_PUTIH); tft.setTextSize(5); 
    tft.setCursor(cx - offset, cy - 17); tft.println(target);

    // Gambar Box Utama
    int boxX = 50;
    int boxW = 220;
    tft.fillRoundRect(boxX, 175, boxW, 32, 10, CLR_PUTIH);
    tft.drawRoundRect(boxX, 175, boxW, 32, 10, CLR_MERAH);
    
    String tkStr = "Terkumpul: " + String(current);
    
    int16_t tkX1, tkY1;
    uint16_t tkW, tkH;
    tft.setTextSize(2);
    tft.getTextBounds(tkStr, 0, 175, &tkX1, &tkY1, &tkW, &tkH);
    
    int tkTxtX = boxX + (boxW - tkW) / 2;
    
    tft.setTextColor(CLR_GELAP); 
    tft.setCursor(tkTxtX, 184);
    tft.print(tkStr);

  } else {
    showFeedback(benar); 
  }
  screenStatusBar();
}

void screenSplash() {
  tft.fillScreen(CLR_HITAM);
  drawStars(CLR_KUNING, 12);
  
  tft.drawRect(4, 4, SCR_W-8, SCR_H-SB_H-8, CLR_UNGU);
  tft.drawRect(6, 6, SCR_W-12, SCR_H-SB_H-12, CLR_UNGU);
  tft.fillRect(6, 6, 8, 8, CLR_ORANGE);
  tft.fillRect(SCR_W-14, 6, 8, 8, CLR_ORANGE);
  tft.fillRect(6, SCR_H-SB_H-14, 8, 8, CLR_ORANGE);
  tft.fillRect(SCR_W-14, SCR_H-SB_H-14, 8, 8, CLR_ORANGE);
  
  tft.fillRoundRect(50, 40, SCR_W-100, 45, 8, CLR_GELAP);
  tft.drawRoundRect(50, 40, SCR_W-100, 45, 8, CLR_UNGU);
  
  tft.setTextColor(CLR_MERAH); tft.setTextSize(4); 
  tft.setCursor(90, 50); tft.println("NUMORA"); 
  tft.setTextColor(CLR_KUNING); tft.setTextSize(4);
  tft.setCursor(88, 48); tft.println("NUMORA"); 
  
  tft.setTextSize(1); tft.setTextColor(CLR_PUTIH);
  tft.setCursor(97, 95); tft.println("Belajar Warna & Angka");
  
  uint16_t cc[] = {CLR_MERAH, CLR_KUNING, CLR_HIJAU, CLR_BIRU};
  int startX = (SCR_W - (4 * 40)) / 2 + 20;
  for (int i = 0; i < 4; i++) {
    tft.fillCircle(startX + i*40 + 2, 135 + 2, 12, CLR_GELAP); 
    tft.fillCircle(startX + i*40, 135, 12, cc[i]); 
    tft.drawCircle(startX + i*40, 135, 12, CLR_PUTIH); 
    tft.fillCircle(startX + i*40 - 4, 135 - 4, 3, CLR_PUTIH); 
  }
  
  tft.setTextColor(CLR_ABU); tft.setTextSize(1);
  tft.setCursor(135, 165); tft.println("BLE v1.0");
  tft.drawRect(40, 180, 240, 12, CLR_ABU);
  for (int i = 0; i <= 238; i += 8) { 
    tft.fillRect(41, 181, i, 10, CLR_UNGU); 
    delay(14); 
  }
  
  screenStatusBar();
  delay(300);
}