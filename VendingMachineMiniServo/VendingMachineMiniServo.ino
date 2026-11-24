// VendingMachineMiniServo.ino — Vending machine mini berbasis ESP32.
//
// Cara kerja:
//   1. Masukkan koin ke penerima koin (coin acceptor). Setiap pulsa yang
//      dikirim penerima koin dihitung lewat interupsi dan menambah kredit
//      sebesar NILAI_PER_PULSA.
//   2. LCD menampilkan kredit saat ini dan harga kedua produk.
//   3. Tekan tombol A (Rp2000) atau tombol B (Rp5000). Bila kredit cukup,
//      servo SG90 memutar pintu keluar selama WAKTU_PENGELUARAN_MS,
//      camilan jatuh, kredit berkurang, buzzer berbunyi sekali.
//      Bila kredit kurang, buzzer berbunyi dua kali dan LED merah menyala.
//   4. LED hijau menyala bila kredit cukup untuk produk termurah.
//
// Pinout (HARUS sama dengan skema di schematic.png):
//   GPIO4  <- pulsa penerima koin (interupsi, aktif LOW, 1 pulsa = Rp1000)
//   GPIO32 <- tombol produk A (Rp2000, INPUT_PULLUP, aktif LOW)
//   GPIO33 <- tombol produk B (Rp5000, INPUT_PULLUP, aktif LOW)
//   GPIO25 -> servo SG90 (PWM 50 Hz, 0-180 derajat)
//   GPIO21 -> LCD I2C SDA, GPIO22 -> LCD I2C SCL (alamat 0x27)
//   GPIO26 -> buzzer aktif HIGH
//   GPIO14 -> LED hijau (kredit cukup) lewat resistor 220 ohm ke GND
//   GPIO12 -> LED merah (kredit kurang) lewat resistor 220 ohm ke GND
//
// Library yang dibutuhkan (lihat folder libraries/ di repo):
//   - LiquidCrystal_I2C 1.1.2
//   - ESP32Servo 3.2.1
//
// Catu daya: ESP32 + LCD + servo dari USB 5V. Servo SG90 menarik arus
// sampai ~500 mA saat memutar — JANGAN catu servo dari pin 3V3 ESP32,
// pakai rel 5V (VIN) dan GND yang sama.

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// -------------------- pin --------------------
const uint8_t PIN_PULSA_KOIN   = 4;   // interupsi, aktif LOW
const uint8_t PIN_TOMBOL_A     = 32;  // produk A, INPUT_PULLUP
const uint8_t PIN_TOMBOL_B     = 33;  // produk B, INPUT_PULLUP
const uint8_t PIN_SERVO        = 25;
const uint8_t PIN_BUZZER       = 26;
const uint8_t PIN_LED_HIJAU    = 14;
const uint8_t PIN_LED_MERAH    = 12;
const uint8_t PIN_SDA_LCD      = 21;
const uint8_t PIN_SCL_LCD      = 22;

// -------------------- konfigurasi --------------------
const unsigned long NILAI_PER_PULSA_RP   = 1000;  // 1 pulsa koin = Rp1000
const unsigned long HARGA_PRODUK_A_RP    = 2000;  // produk A
const unsigned long HARGA_PRODUK_B_RP    = 5000;  // produk B
const unsigned long DEBOUNCE_PULSA_MS    = 30;    // anti pantulan pulsa koin
const unsigned long DEBOUNCE_TOMBOL_MS   = 50;    // anti pantulan tombol
const unsigned long WAKTU_PENGELUARAN_MS = 1500;  // servo menahan pintu terbuka
const uint8_t       ALAMAT_LCD           = 0x27;
const uint8_t       KOLOM_LCD            = 16;
const uint8_t       BARIS_LCD            = 2;
const int           SUDUT_SERVO_TUTUP    = 0;     // pintu tertutup
const int           SUDUT_SERVO_BUKA     = 90;    // pintu terbuka
const unsigned long WAKTU_LED_MERAH_MS   = 2000;  // LED merah menyala 2 detik

// -------------------- objek --------------------
LiquidCrystal_I2C lcd(ALAMAT_LCD, KOLOM_LCD, BARIS_LCD);
Servo servoPintu;

// -------------------- state --------------------
// Dihitung di ISR — wajib volatile.
volatile unsigned long jumlahPulsa = 0;
volatile unsigned long waktuPulsaTerakhirMs = 0;

unsigned long kreditRp = 0;
unsigned long waktuLedMerahMatiMs = 0;
bool sedangMengeluarkan = false;
unsigned long waktuMulaiKeluarkanMs = 0;

// Debounce tombol (non-blocking, per tombol).
struct Tombol {
  uint8_t pin;
  bool keadaanTerakhir;
  unsigned long waktuPerubahanTerakhirMs;
};

Tombol tombolA = {PIN_TOMBOL_A, HIGH, 0};
Tombol tombolB = {PIN_TOMBOL_B, HIGH, 0};

// -------------------- interupsi pulsa koin --------------------
void IRAM_ATTR saatPulsaKoin() {
  unsigned long sekarang = millis();
  if (sekarang - waktuPulsaTerakhirMs >= DEBOUNCE_PULSA_MS) {
    jumlahPulsa++;
    waktuPulsaTerakhirMs = sekarang;
  }
}

// -------------------- buzzer --------------------
void bunyiBuzzer(unsigned int durasiMs) {
  digitalWrite(PIN_BUZZER, HIGH);
  delay(durasiMs);
  digitalWrite(PIN_BUZZER, LOW);
}

void bunyiSukses() {
  bunyiBuzzer(150);
}

void bunyiGagal() {
  bunyiBuzzer(120);
  delay(120);
  bunyiBuzzer(120);
}

// -------------------- LCD --------------------
void tampilkanLcd() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Kredit Rp");
  lcd.print(kreditRp);
  lcd.setCursor(0, 1);
  lcd.print("A:2000 B:5000");
}

// -------------------- pengeluaran produk --------------------
void mulaiKeluarkanProduk(unsigned long hargaRp) {
  kreditRp -= hargaRp;
  sedangMengeluarkan = true;
  waktuMulaiKeluarkanMs = millis();
  servoPintu.write(SUDUT_SERVO_BUKA);
  bunyiSukses();
  tampilkanLcd();
}

void cobaBeli(unsigned long hargaRp) {
  if (sedangMengeluarkan) {
    return;  // servo masih bekerja, abaikan tombol
  }
  if (kreditRp >= hargaRp) {
    mulaiKeluarkanProduk(hargaRp);
  } else {
    // Kredit kurang: bunyi dua kali + LED merah 2 detik.
    bunyiGagal();
    digitalWrite(PIN_LED_MERAH, HIGH);
    waktuLedMerahMatiMs = millis() + WAKTU_LED_MERAH_MS;
  }
}

// -------------------- tombol --------------------
// Mengembalikan true tepat sekali saat tombol ditekan (transisi HIGH->LOW).
bool tombolDitekan(Tombol &tombol) {
  bool keadaan = digitalRead(tombol.pin);
  unsigned long sekarang = millis();
  if (keadaan != tombol.keadaanTerakhir) {
    tombol.waktuPerubahanTerakhirMs = sekarang;
    tombol.keadaanTerakhir = keadaan;
  }
  if (sekarang - tombol.waktuPerubahanTerakhirMs >= DEBOUNCE_TOMBOL_MS
      && keadaan == LOW) {
    // Tunggu tombol dilepas supaya satu tekanan = satu pembelian.
    while (digitalRead(tombol.pin) == LOW) {
      delay(10);
    }
    return true;
  }
  return false;
}

// -------------------- setup / loop --------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_PULSA_KOIN, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_A, INPUT_PULLUP);
  pinMode(PIN_TOMBOL_B, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_HIJAU, OUTPUT);
  pinMode(PIN_LED_MERAH, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_HIJAU, LOW);
  digitalWrite(PIN_LED_MERAH, LOW);

  attachInterrupt(digitalPinToInterrupt(PIN_PULSA_KOIN),
                  saatPulsaKoin, FALLING);

  Wire.begin(PIN_SDA_LCD, PIN_SCL_LCD);
  lcd.init();
  lcd.backlight();
  tampilkanLcd();

  servoPintu.attach(PIN_SERVO);
  servoPintu.write(SUDUT_SERVO_TUTUP);

  Serial.println("Vending machine mini siap. Masukkan koin.");
}

void loop() {
  unsigned long sekarang = millis();

  // 1. Pulsa koin baru -> tambah kredit.
  static unsigned long pulsaTerproses = 0;
  if (jumlahPulsa != pulsaTerproses) {
    unsigned long pulsaBaru = jumlahPulsa - pulsaTerproses;
    pulsaTerproses = jumlahPulsa;
    kreditRp += pulsaBaru * NILAI_PER_PULSA_RP;
    Serial.print("Koin masuk, kredit Rp");
    Serial.println(kreditRp);
    tampilkanLcd();
  }

  // 2. Tombol produk.
  if (tombolDitekan(tombolA)) {
    cobaBeli(HARGA_PRODUK_A_RP);
  }
  if (tombolDitekan(tombolB)) {
    cobaBeli(HARGA_PRODUK_B_RP);
  }

  // 3. Tutup pintu servo setelah waktu pengeluaran habis.
  if (sedangMengeluarkan
      && sekarang - waktuMulaiKeluarkanMs >= WAKTU_PENGELUARAN_MS) {
    servoPintu.write(SUDUT_SERVO_TUTUP);
    sedangMengeluarkan = false;
    Serial.println("Produk keluar. Pintu tertutup.");
  }

  // 4. LED hijau = kredit cukup untuk produk termurah.
  digitalWrite(PIN_LED_HIJAU,
               (kreditRp >= HARGA_PRODUK_A_RP && !sedangMengeluarkan)
                   ? HIGH : LOW);

  // 5. Matikan LED merah setelah waktunya habis.
  if (waktuLedMerahMatiMs != 0 && sekarang >= waktuLedMerahMatiMs) {
    digitalWrite(PIN_LED_MERAH, LOW);
    waktuLedMerahMatiMs = 0;
  }

  // 6. LCD hanya diperbarui saat ada perubahan (koin masuk / pembelian)
  // supaya tidak berkedip karena clear() berulang.
}
