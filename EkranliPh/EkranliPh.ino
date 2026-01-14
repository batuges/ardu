/*
 * Arduino pH Sensörü - Nextion Ekran Entegrasyonlu
 * DÜZELTİLMİŞ KALİBRASYON
 * Nextion ile seri iletişim
 * TAM KOD - Tüm fonksiyonlar dahil
 */

#include <EEPROM.h>
#include <SoftwareSerial.h> // Nextion için

// Nextion bağlantı pinleri (RX, TX)
SoftwareSerial nextion(2, 3); // Arduino pin 2 → Nextion TX, pin 3 → Nextion RX

// Pin tanımlamaları
#define pH_PIN A0

// Kalibrasyon değerleri
float calVoltage7 = 3.646;
float calVoltage4 = 4.125;
float calVoltage10 = 3.226;

// EEPROM adresleri
#define ADDR_PH7 0
#define ADDR_PH4 4
#define ADDR_PH10 8

// Ölçüm için değişkenler
int samples = 10;
int sampleDelay = 20;
unsigned long lastUpdate = 0;
const int updateInterval = 1000; // Nextion'u 1 saniyede bir güncelle

// Kalibrasyon durumu
bool isCalibrating = false;
int calibrationStep = 0;

void setup() {
  // İki seri portu başlat
  Serial.begin(9600);     // Bilgisayar için
  nextion.begin(9600);    // Nextion için
  
  // Nextion'u sıfırla ve bekle
  delay(500);
  sendToNextion("rest");
  delay(1000);
  
  Serial.println(F("============================================="));
  Serial.println(F("pH SENSÖRÜ - NEXTION ENTEGRASYONLU"));
  Serial.println(F("pH7: 3.646V | pH4: 4.125V | pH10: 3.226V"));
  Serial.println(F("Nextion Ekran Bağlantısı Kuruluyor..."));
  Serial.println(F("============================================="));
  
  // Başlangıç mesajını Nextion'a gönder
  sendToNextion("t2.txt=\"Sistem Hazır\"");
  sendToNextion("t0.txt=\"pH: --.--\"");
  sendToNextion("t1.txt=\"Voltaj: --.--V\"");
  delay(100);
  
  // EEPROM'dan kalibrasyon değerlerini oku
  loadCalibrationFromEEPROM();
  
  // Analog referans ayarı
  analogReference(DEFAULT);
  
  // Buton renklerini sıfırla
  resetButtonColors();
  
  // İlk ekran güncellemesi
  updateNextionDisplay();
  
  // Kalibrasyon testi yap
  testCalibrationPoints();
}

void loop() {
  // Seri porttan komut kontrolü (Nextion'dan)
  if (nextion.available() > 0) {
    String command = nextion.readStringUntil('\n');
    command.trim();
    processNextionCommand(command);
  }
  
  // Bilgisayar seri portundan komut kontrolü
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    processSerialCommand(cmd);
  }
  
  // Nextion ekranını belirli aralıklarla güncelle
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    updateNextionDisplay();
  }
}

// Nextion'a komut gönder
void sendToNextion(String command) {
  nextion.print(command);
  nextion.write(0xFF);
  nextion.write(0xFF);
  nextion.write(0xFF);
  delay(10);
}

// Nextion ekranını güncelle
void updateNextionDisplay() {
  if (isCalibrating) {
    return; // Kalibrasyon sırasında ekranı güncelleme
  }
  
  float voltage = readVoltage();
  float pHValue = calculatepH(voltage);
  
  // pH değerini sınırla
  if (pHValue < 0) pHValue = 0;
  if (pHValue > 14) pHValue = 14;
  
  // Nextion'a verileri gönder
  String pHText = "t0.txt=\"pH: " + String(pHValue, 2) + "\"";
  sendToNextion(pHText);
  
  String voltText = "t1.txt=\"Voltaj: " + String(voltage, 3) + "V\"";
  sendToNextion(voltText);
  
  // pH durumuna göre renk değiştir
  if (pHValue < 6.5) {
    sendToNextion("t0.pco=63488"); // Kırmızı (asidik)
    sendToNextion("t3.txt=\"ASİDİK\""); // Durum metni
    sendToNextion("t3.pco=63488");
  } else if (pHValue > 7.5) {
    sendToNextion("t0.pco=2016"); // Mavi (bazik)
    sendToNextion("t3.txt=\"BAZİK\"");
    sendToNextion("t3.pco=2016");
  } else {
    sendToNextion("t0.pco=2047"); // Yeşil (nötr)
    sendToNextion("t3.txt=\"NÖTR\"");
    sendToNextion("t3.pco=2047");
  }
}

// Nextion'dan gelen komutları işle
void processNextionCommand(String cmd) {
  Serial.print("Nextion Komut: ");
  Serial.println(cmd);
  
  if (cmd == "k7") {
    startCalibration(7);
  } else if (cmd == "k4") {
    startCalibration(4);
  } else if (cmd == "k10") {
    startCalibration(10);
  } else if (cmd == "t") {
    sendToNextion("t2.txt=\"Test Modu\"");
    testCalibrationPoints();
  } else if (cmd == "d") {
    sendToNextion("t2.txt=\"Debug Modu\"");
    debugMode();
  } else if (cmd == "show") {
    showCalibration();
  } else if (cmd == "reset") {
    resetButtonColors();
    sendToNextion("t2.txt=\"Hazır\"");
  }
}

// Seri porttan gelen komutları işle
void processSerialCommand(char cmd) {
  if (cmd == '7') {
    startCalibration(7);
  } else if (cmd == '4') {
    startCalibration(4);
  } else if (cmd == '0') {
    startCalibration(10);
  } else if (cmd == 'r') {
    measureAndDisplay();
  } else if (cmd == 't') {
    testCalibrationPoints();
  } else if (cmd == 's') {
    showCalibration();
  } else if (cmd == 'd') {
    debugMode();
  } else if (cmd != '\n' && cmd != '\r') {
    Serial.print(F("Geçersiz komut: "));
    Serial.println(cmd);
  }
}

// DÜZELTİLMİŞ pH HESAPLAMA FONKSİYONU
float calculatepH(float voltage) {
  float pH = 0;
  
  // Eğer voltaj pH7 değerinden BÜYÜKSE (asidik bölge)
  if (voltage >= calVoltage7) {
    // Asidik bölge: pH 0-7 arası
    float slope = 3.0 / (calVoltage4 - calVoltage7);
    pH = 7.0 - ((voltage - calVoltage7) * slope);
  }
  // Eğer voltaj pH7 değerinden KÜÇÜKSE (bazik bölge)
  else {
    // Bazik bölge: pH 7-14 arası
    float slope = 3.0 / (calVoltage7 - calVoltage10);
    pH = 7.0 + ((calVoltage7 - voltage) * slope);
  }
  
  return pH;
}

// Voltaj okuma (ortalama ile)
float readVoltage() {
  int sum = 0;
  
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pH_PIN);
    delay(sampleDelay);
  }
  
  float average = sum / (float)samples;
  return average * (5.0 / 1023.0);
}

// Kalibrasyon başlat
void startCalibration(int pHPoint) {
  isCalibrating = true;
  
  // İlgili butonun rengini değiştir
  resetButtonColors();
  switch(pHPoint) {
    case 7: sendToNextion("b0.bco=63488"); break;
    case 4: sendToNextion("b1.bco=63488"); break;
    case 10: sendToNextion("b2.bco=63488"); break;
  }
  
  calibratePoint(pHPoint);
  isCalibrating = false;
}

// Kalibrasyon noktası ayarla
void calibratePoint(int pHPoint) {
  String pointName = "pH" + String(pHPoint);
  sendToNextion("t2.txt=\"" + pointName + " Kalibrasyonu\"");
  Serial.println(pointName + " kalibrasyonu için sensörü çözeltiye daldırın...");
  Serial.println(F("10 saniye bekleyin..."));
  
  // 10 saniye geri sayım
  for (int i = 10; i > 0; i--) {
    String countdown = "t2.txt=\"" + pointName + ": " + String(i) + "sn\"";
    sendToNextion(countdown);
    Serial.print(String(i) + "... ");
    delay(1000);
  }
  Serial.println();
  
  // 20 ölçüm al ve ortalama hesapla
  sendToNextion("t2.txt=\"Ölçüm yapılıyor\"");
  Serial.print(F("Ölçüm yapılıyor"));
  float totalVoltage = 0;
  for (int i = 0; i < 20; i++) {
    totalVoltage += readVoltage();
    delay(200);
    Serial.print(F("."));
  }
  Serial.println();
  
  float measuredVoltage = totalVoltage / 20.0;
  
  // Değeri güncelle
  switch(pHPoint) {
    case 7:
      calVoltage7 = measuredVoltage;
      EEPROM.put(ADDR_PH7, calVoltage7);
      Serial.print(F("pH7 = "));
      Serial.print(calVoltage7, 3);
      Serial.println(F("V olarak ayarlandı"));
      sendToNextion("t2.txt=\"pH7: " + String(calVoltage7, 3) + "V\"");
      break;
    case 4:
      calVoltage4 = measuredVoltage;
      EEPROM.put(ADDR_PH4, calVoltage4);
      Serial.print(F("pH4 = "));
      Serial.print(calVoltage4, 3);
      Serial.println(F("V olarak ayarlandı"));
      sendToNextion("t2.txt=\"pH4: " + String(calVoltage4, 3) + "V\"");
      break;
    case 10:
      calVoltage10 = measuredVoltage;
      EEPROM.put(ADDR_PH10, calVoltage10);
      Serial.print(F("pH10 = "));
      Serial.print(calVoltage10, 3);
      Serial.println(F("V olarak ayarlandı"));
      sendToNextion("t2.txt=\"pH10: " + String(calVoltage10, 3) + "V\"");
      break;
  }
  
  delay(2000);
  sendToNextion("t2.txt=\"Kalibrasyon Tamam\"");
  Serial.println(F("Kalibrasyon tamamlandı!"));
  
  // Buton renklerini sıfırla
  resetButtonColors();
  
  // Test et
  testCalibrationPoints();
}

// EEPROM'dan kalibrasyon değerlerini yükle
void loadCalibrationFromEEPROM() {
  float temp7, temp4, temp10;
  
  EEPROM.get(ADDR_PH7, temp7);
  EEPROM.get(ADDR_PH4, temp4);
  EEPROM.get(ADDR_PH10, temp10);
  
  // EEPROM'da geçerli değerler varsa kullan
  if (temp7 > 1.0 && temp7 < 5.0) {
    calVoltage7 = temp7;
    Serial.print(F("EEPROM'dan pH7: "));
    Serial.print(calVoltage7, 3);
    Serial.println(F("V"));
  }
  
  if (temp4 > 1.0 && temp4 < 5.0) {
    calVoltage4 = temp4;
    Serial.print(F("EEPROM'dan pH4: "));
    Serial.print(calVoltage4, 3);
    Serial.println(F("V"));
  }
  
  if (temp10 > 1.0 && temp10 < 5.0) {
    calVoltage10 = temp10;
    Serial.print(F("EEPROM'dan pH10: "));
    Serial.print(calVoltage10, 3);
    Serial.println(F("V"));
  }
  
  Serial.println(F("EEPROM'dan kalibrasyon değerleri yüklendi."));
  sendToNextion("t2.txt=\"EEPROM Yüklendi\"");
}

// Kalibrasyon noktalarını test et
void testCalibrationPoints() {
  Serial.println(F("\n=== KALİBRASYON TESTİ ==="));
  sendToNextion("t2.txt=\"Kalibrasyon Testi\"");
  
  // pH4 için hesapla
  float pH4_calculated = calculatepH(calVoltage4);
  Serial.print(F("pH4 ("));
  Serial.print(calVoltage4, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH4_calculated, 2);
  bool test4 = pH4_calculated >= 3.9 && pH4_calculated <= 4.1;
  Serial.println(test4 ? F(" ✓") : F(" ✗ HATA!"));
  
  // pH7 için hesapla
  float pH7_calculated = calculatepH(calVoltage7);
  Serial.print(F("pH7 ("));
  Serial.print(calVoltage7, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH7_calculated, 2);
  bool test7 = pH7_calculated >= 6.9 && pH7_calculated <= 7.1;
  Serial.println(test7 ? F(" ✓") : F(" ✗ HATA!"));
  
  // pH10 için hesapla
  float pH10_calculated = calculatepH(calVoltage10);
  Serial.print(F("pH10 ("));
  Serial.print(calVoltage10, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH10_calculated, 2);
  bool test10 = pH10_calculated >= 9.9 && pH10_calculated <= 10.1;
  Serial.println(test10 ? F(" ✓") : F(" ✗ HATA!"));
  
  Serial.println(F("==========================\n"));
  
  // Nextion'a test sonuçlarını gönder
  String testResult = "t2.txt=\"Test: ";
  testResult += String(pH4_calculated, 1) + "/" + String(pH7_calculated, 1) + "/" + String(pH10_calculated, 1);
  if (test4 && test7 && test10) {
    testResult += " OK\"";
    sendToNextion("t2.pco=2047"); // Yeşil
  } else {
    testResult += " HATA\"";
    sendToNextion("t2.pco=63488"); // Kırmızı
  }
  sendToNextion(testResult);
  
  delay(3000);
  sendToNextion("t2.txt=\"Hazır\"");
  sendToNextion("t2.pco=65535"); // Beyaz
}

// Kalibrasyon değerlerini göster
void showCalibration() {
  Serial.println(F("\n--- KALİBRASYON DEĞERLERİ ---"));
  Serial.print(F("pH7  (nötr):   "));
  Serial.print(calVoltage7, 3);
  Serial.println(F("V"));
  
  Serial.print(F("pH4  (asidik): "));
  Serial.print(calVoltage4, 3);
  Serial.println(F("V"));
  
  Serial.print(F("pH10 (bazik):  "));
  Serial.print(calVoltage10, 3);
  Serial.println(F("V"));
  
  // Eğimleri hesapla
  float slope_acidic = 3.0 / (calVoltage4 - calVoltage7);  // pH7 → pH4
  float slope_basic = 3.0 / (calVoltage7 - calVoltage10);  // pH7 → pH10
  
  Serial.print(F("Asidik eğim: "));
  Serial.print(slope_acidic, 3);
  Serial.println(F(" pH/V"));
  
  Serial.print(F("Bazik eğim:  "));
  Serial.print(slope_basic, 3);
  Serial.println(F(" pH/V"));
  Serial.println(F("------------------------------\n"));
  
  // Nextion'a gönder
  String calValues = "t2.txt=\"Cal: ";
  calValues += String(calVoltage4, 2) + "/" + String(calVoltage7, 2) + "/" + String(calVoltage10, 2) + "\"";
  sendToNextion(calValues);
}

// Debug modu
void debugMode() {
  Serial.println(F("\n=== DEBUG MODU ==="));
  Serial.println(F("Voltaj ve ham değerler:"));
  sendToNextion("t2.txt=\"Debug Modu\"");
  
  for (int i = 0; i < 5; i++) {
    int raw = analogRead(pH_PIN);
    float voltage = raw * (5.0 / 1023.0);
    float pH = calculatepH(voltage);
    
    Serial.print(F("Raw: "));
    Serial.print(raw);
    Serial.print(F(" | Voltaj: "));
    Serial.print(voltage, 3);
    Serial.print(F("V | pH: "));
    Serial.println(pH, 2);
    
    // Nextion'a gönder
    String debugInfo = "t2.txt=\"Raw: " + String(raw) + " pH: " + String(pH, 2) + "\"";
    sendToNextion(debugInfo);
    
    delay(500);
  }
  
  Serial.println(F("==================\n"));
  sendToNextion("t2.txt=\"Debug Tamam\"");
  delay(1000);
  sendToNextion("t2.txt=\"Hazır\"");
}

// Buton renklerini sıfırla
void resetButtonColors() {
  sendToNextion("b0.bco=65535"); // Beyaz
  sendToNextion("b1.bco=65535");
  sendToNextion("b2.bco=65535");
  sendToNextion("b0.pco=0"); // Siyah yazı
  sendToNextion("b1.pco=0");
  sendToNextion("b2.pco=0");
}

// Ölçüm yap ve göster (seri monitör için)
void measureAndDisplay() {
  float voltage = readVoltage();
  float pHValue = calculatepH(voltage);
  
  // 0-14 aralığında sınırla
  if (pHValue < 0) pHValue = 0;
  if (pHValue > 14) pHValue = 14;
  
  // Ekrana yazdır
  Serial.print(F("Zaman: "));
  Serial.print(millis() / 1000);
  Serial.print(F("s | Voltaj: "));
  Serial.print(voltage, 3);
  Serial.print(F("V | pH: "));
  Serial.print(pHValue, 2);
  
  // pH durumu
  if (pHValue < 6.5) {
    Serial.print(F(" (ASİDİK)"));
  } else if (pHValue > 7.5) {
    Serial.print(F(" (BAZİK)"));
  } else {
    Serial.print(F(" (NÖTR)"));
  }
  
  // Analog değer
  Serial.print(F(" | A0: "));
  Serial.println(analogRead(pH_PIN));
}
