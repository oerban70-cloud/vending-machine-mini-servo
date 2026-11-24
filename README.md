# Vending Machine Mini Servo — ESP32 + Penerima Koin + Servo SG90

Kotak kecil yang menjual camilan sendiri tanpa penjaga. Masukkan koin,
kreditmu tercatat di LCD; tekan tombol produk, servo SG90 memutar pintu
keluar dan camilanmu jatuh ke bawah. Kredit sisa tetap tersimpan untuk
pembelian berikutnya.

Video penjelasan (60 detik):
https://www.youtube.com/shorts/4sXb2D2RcRk

## Komponen

| Bagian | Keterangan |
|---|---|
| ESP32 DevKit | Otak alat |
| Penerima koin (coin acceptor) | Mengirim 1 pulsa per Rp1000, sinyal pulsa ke GPIO4 |
| Tombol A | Produk A, harga Rp2000 |
| Tombol B | Produk B, harga Rp5000 |
| Servo SG90 5V | Memutar pintu keluar 90 derajat selama 1,5 detik |
| LCD 16x2 I2C (alamat 0x27) | Menampilkan kredit + harga kedua produk |
| Buzzer aktif | Bunyi sekali saat beli sukses, dua kali saat kredit kurang |
| LED hijau + resistor 220 ohm | Menyala saat kredit cukup untuk produk termurah |
| LED merah + resistor 220 ohm | Menyala 2 detik saat kredit kurang |
| Catu USB 5V | Adaptor atau port laptop |

## Wiring

| Dari | Ke |
|---|---|
| Penerima koin SIG | ESP32 GPIO4 (interupsi, aktif LOW) |
| Penerima koin VCC | 5V |
| Penerima koin GND | GND |
| Tombol A (kaki 1) | GND |
| Tombol A (kaki 2) | ESP32 GPIO32 (INPUT_PULLUP) |
| Tombol B (kaki 1) | GND |
| Tombol B (kaki 2) | ESP32 GPIO33 (INPUT_PULLUP) |
| Servo SIG | ESP32 GPIO25 |
| Servo VCC | 5V |
| Servo GND | GND |
| LCD SDA | ESP32 GPIO21 |
| LCD SCL | ESP32 GPIO22 |
| LCD VCC | 5V |
| LCD GND | GND |
| Buzzer + | ESP32 GPIO26 (aktif HIGH) |
| Buzzer - | GND |
| LED hijau anoda | ESP32 GPIO14, lewat resistor 220 ohm ke GND |
| LED merah anoda | ESP32 GPIO12, lewat resistor 220 ohm ke GND |
| USB 5V | ESP32 VIN (lewat konektor USB board) |
| Semua GND | Digabung (koin, tombol, servo, LCD, buzzer, LED, USB -) |

Skema lengkap: [schematic.png](schematic.png)

Perhatian daya: servo SG90 menarik arus sampai 500 mA saat memutar —
catu servo dari rel 5V, bukan dari pin 3V3 ESP32. Satu catu USB 5V
yang kuat (adaptor 2A) cukup untuk ESP32 + LCD + servo + buzzer.

## Cara kerja

Penerima koin mengirim satu pulsa pendek setiap kali koin Rp1000 masuk.
Pulsa itu ditangkap ESP32 lewat interupsi di GPIO4 (debounce 30 ms
supaya satu koin tidak terhitung dua kali), lalu kredit bertambah
Rp1000 dan LCD diperbarui. Dua tombol memilih produk: tombol A seharga
Rp2000, tombol B seharga Rp5000.

Saat tombol ditekan, sketch memeriksa kredit. Bila cukup, harga produk
dipotong dari kredit, servo memutar pintu keluar ke 90 derajat selama
1,5 detik (camilan jatuh), lalu pintu menutup lagi; buzzer berbunyi
sekali dan LED hijau menyala. Bila kredit kurang, tidak ada yang
dipotong: buzzer berbunyi dua kali dan LED merah menyala 2 detik.
Selama servo bekerja, tombol diabaikan supaya satu pembelian tidak
menjadi dua.

Tombol memakai INPUT_PULLUP (aktif LOW) dengan debounce 50 ms, dan
sketch menunggu tombol dilepas sebelum menerima tekanan berikutnya —
satu tekanan selalu berarti satu pembelian. Semua angka (harga produk,
nilai per pulsa, sudut servo, waktu buka pintu) ada sebagai konstanta
bernama di awal sketch.

## Konfigurasi (di awal sketch)

- `NILAI_PER_PULSA_RP` (1000) — nilai satu pulsa koin, dalam rupiah.
- `HARGA_PRODUK_A_RP` (2000) — harga produk A (tombol A).
- `HARGA_PRODUK_B_RP` (5000) — harga produk B (tombol B).
- `DEBOUNCE_PULSA_MS` (30) — jeda anti pantulan pulsa koin.
- `DEBOUNCE_TOMBOL_MS` (50) — jeda anti pantulan tombol.
- `WAKTU_PENGELUARAN_MS` (1500) — lamanya pintu servo terbuka.
- `SUDUT_SERVO_TUTUP` (0) / `SUDUT_SERVO_BUKA` (90) — sudut pintu.
- `ALAMAT_LCD` (0x27) — alamat I2C LCD 16x2.

Kalau penerima koinmu mengirim pulsa dengan nilai lain (mis. 1 pulsa
per Rp500), ubah `NILAI_PER_PULSA_RP`; kalau harga produk berubah,
ubah `HARGA_PRODUK_A_RP` dan `HARGA_PRODUK_B_RP`. Pantau lewat Serial
Monitor (115200 baud): setiap koin masuk dan setiap produk keluar
dicetak beserta kredit saat itu.

## Library yang dibutuhkan

- LiquidCrystal_I2C 1.1.2 (LCD 16x2 via backpack PCF8574).
- ESP32Servo 3.2.1 (servo SG90, PWM 50 Hz).

Kedua library ada di folder `libraries/` (file ZIP, folder library di
top-level zip) — pasang lewat Arduino IDE (Sketch > Include Library >
Add .ZIP Library) atau salin ke folder libraries Arduino.

## Compile

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 VendingMachineMiniServo
```

Terverifikasi 24 Nov 2026: sketch 329.339 byte (25% flash),
RAM 25.508 byte (7%), ESP32 core 4.0.0, arduino-cli 1.6.0.

## Lisensi

MIT — lihat [LICENSE](LICENSE).
