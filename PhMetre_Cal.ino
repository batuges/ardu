/*
 * Arduino pH Sensörü - DÜZELTİLMİŞ KALİBRASYON
 * pH7: 3.646V | pH4: 4.125V | pH10: 3.226V
 * DÜZELTME: pH4'te doğru değer göstermesi için formül düzeltildi
 */

#include <EEPROM.h>

// Pin tanımlamaları
#define pH_PIN A0

// SİZİN KALİBRASYON DEĞERLERİNİZ
float calVoltage7 = 3.646;    // pH7'de ölçtüğünüz voltaj: 3.646V
float calVoltage4 = 4.125;    // pH4'te ölçtüğünüz voltaj: 4.125V
float calVoltage10 = 3.226;   // pH10'da ölçtüğünüz voltaj: 3.226V

// EEPROM adresleri
#define ADDR_PH7 0
#define ADDR_PH4 4
#define ADDR_PH10 8

// Ölçüm için değişkenler
int samples = 10;
int sampleDelay = 20;

void setup() {
  Serial.begin(9600);
  Serial.println(F("============================================="));
  Serial.println(F("pH SENSÖRÜ - DÜZELTİLMİŞ KALİBRASYON"));
  Serial.println(F("pH7: 3.646V | pH4: 4.125V | pH10: 3.226V"));
  Serial.println(F("============================================="));
  Serial.println(F("ÖNEMLİ: Sensörünüz ters çalışıyor!"));
  Serial.println(F("pH arttıkça voltaj DÜŞÜYOR"));
  Serial.println(F("pH azaldıkça voltaj ARTıYOR"));
  Serial.println(F("============================================="));
  Serial.println(F("Komutlar:"));
  Serial.println(F("  'r'   - Tek ölçüm yap"));
  Serial.println(F("  't'   - Test modu (pH4,7,10 için hesapla)"));
  Serial.println(F("  'k7'  - pH7 kalibrasyonu"));
  Serial.println(F("  'k4'  - pH4 kalibrasyonu"));
  Serial.println(F("  'k10' - pH10 kalibrasyonu"));
  Serial.println(F("  's'   - Kalibrasyon değerlerini göster"));
  Serial.println(F("  'd'   - Debug modu (detaylı bilgi)"));
  Serial.println(F("============================================="));
  
  // EEPROM'dan kalibrasyon değerlerini oku
  loadCalibrationFromEEPROM();
  
  // Analog referans ayarı
  analogReference(DEFAULT);
  
  // Test: Kalibrasyon noktalarında pH değerlerini hesapla
  testCalibrationPoints();
}

void loop() {
  // Seri porttan komut kontrolü
  if (Serial.available() > 0) {
    char command = Serial.read();
    processCommand(command);
  }
  
  // Sürekli ölçüm (her 3 saniyede bir)
  static unsigned long lastRead = 0;
  if (millis() - lastRead >= 3000) {
    lastRead = millis();
    measureAndDisplay();
  }
}

// ALTERNATİF: Daha basit komut işleme
void processCommand(char cmd) {
  // Komutları tek karakter olarak işle
  if (cmd == '7') {
    Serial.println(F("pH7 kalibrasyonu"));
    calibratePoint(7);
  } else if (cmd == '4') {
    Serial.println(F("pH4 kalibrasyonu"));
    calibratePoint(4);
  } else if (cmd == '0') {  // pH10 için '0' kullan
    Serial.println(F("pH10 kalibrasyonu"));
    calibratePoint(10);
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
  /*
   * DÜZELTME: Sensörünüz TERS çalışıyor!
   * Normalde: pH arttıkça voltaj DÜŞER
   * Sizinkinde: pH arttıkça voltaj DÜŞÜYOR ama kalibrasyon değerleri ters sırada
   * 
   * Sizin değerleriniz:
   * pH4  = 4.125V (EN YÜKSEK)
   * pH7  = 3.646V (ORTA)
   * pH10 = 3.226V (EN DÜŞÜK)
   * 
   * Formül: pH = 7.0 - ((voltage - calVoltage7) * 3.0) / (calVoltage4 - calVoltage7)
   */
  
  float pH = 0;
  
  // Eğer voltaj pH7 değerinden BÜYÜKSE (asidik bölge)
  if (voltage >= calVoltage7) {
    // Asidik bölge: pH 0-7 arası
    // voltage: 3.646V (pH7) ile 4.125V (pH4) arasında
    // pH: 7'den 4'e doğru AZALACAK
    float slope = 3.0 / (calVoltage4 - calVoltage7);  // pH7'den pH4'e 3 birim
    pH = 7.0 - ((voltage - calVoltage7) * slope);
  }
  // Eğer voltaj pH7 değerinden KÜÇÜKSE (bazik bölge)
  else {
    // Bazik bölge: pH 7-14 arası
    // voltage: 3.226V (pH10) ile 3.646V (pH7) arasında
    // pH: 7'den 10'a doğru ARTACAK
    float slope = 3.0 / (calVoltage7 - calVoltage10);  // pH7'den pH10'a 3 birim
    pH = 7.0 + ((calVoltage7 - voltage) * slope);
  }
  
  return pH;
}

// Ölçüm yap ve göster
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

// Kalibrasyon noktası ayarla
void calibratePoint(int pHPoint) {
  Serial.print(F("pH"));
  Serial.print(pHPoint);
  Serial.println(F(" kalibrasyonu için sensörü çözeltiye daldırın..."));
  Serial.println(F("10 saniye bekleyin..."));
  
  delay(10000);  // 10 saniye bekle
  
  // 20 ölçüm al ve ortalama hesapla
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
      break;
    case 4:
      calVoltage4 = measuredVoltage;
      EEPROM.put(ADDR_PH4, calVoltage4);
      Serial.print(F("pH4 = "));
      Serial.print(calVoltage4, 3);
      Serial.println(F("V olarak ayarlandı"));
      break;
    case 10:
      calVoltage10 = measuredVoltage;
      EEPROM.put(ADDR_PH10, calVoltage10);
      Serial.print(F("pH10 = "));
      Serial.print(calVoltage10, 3);
      Serial.println(F("V olarak ayarlandı"));
      break;
  }
  
  Serial.println(F("Kalibrasyon tamamlandı!"));
  testCalibrationPoints();  // Test et
}

// Kalibrasyon noktalarını test et
void testCalibrationPoints() {
  Serial.println(F("\n=== KALİBRASYON TESTİ ==="));
  
  // pH4 için hesapla
  float pH4_calculated = calculatepH(calVoltage4);
  Serial.print(F("pH4 ("));
  Serial.print(calVoltage4, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH4_calculated, 2);
  Serial.println(pH4_calculated >= 3.9 && pH4_calculated <= 4.1 ? F(" ✓") : F(" ✗ HATA!"));
  
  // pH7 için hesapla
  float pH7_calculated = calculatepH(calVoltage7);
  Serial.print(F("pH7 ("));
  Serial.print(calVoltage7, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH7_calculated, 2);
  Serial.println(pH7_calculated >= 6.9 && pH7_calculated <= 7.1 ? F(" ✓") : F(" ✗ HATA!"));
  
  // pH10 için hesapla
  float pH10_calculated = calculatepH(calVoltage10);
  Serial.print(F("pH10 ("));
  Serial.print(calVoltage10, 3);
  Serial.print(F("V) → pH: "));
  Serial.print(pH10_calculated, 2);
  Serial.println(pH10_calculated >= 9.9 && pH10_calculated <= 10.1 ? F(" ✓") : F(" ✗ HATA!"));
  
  Serial.println(F("==========================\n"));
}

// EEPROM'dan kalibrasyon değerlerini yükle
void loadCalibrationFromEEPROM() {
  float temp7, temp4, temp10;
  
  EEPROM.get(ADDR_PH7, temp7);
  EEPROM.get(ADDR_PH4, temp4);
  EEPROM.get(ADDR_PH10, temp10);
  
  // EEPROM'da geçerli değerler varsa kullan
  if (temp7 > 1.0 && temp7 < 5.0) calVoltage7 = temp7;
  if (temp4 > 1.0 && temp4 < 5.0) calVoltage4 = temp4;
  if (temp10 > 1.0 && temp10 < 5.0) calVoltage10 = temp10;
  
  Serial.println(F("EEPROM'dan kalibrasyon değerleri yüklendi."));
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
}

// Debug modu
void debugMode() {
  Serial.println(F("\n=== DEBUG MODU ==="));
  Serial.println(F("Voltaj ve ham değerler:"));
  
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
    
    delay(500);
  }
  
  Serial.println(F("==================\n"));
}
