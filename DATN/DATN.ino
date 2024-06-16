#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

#define RX_PIN 16
#define TX_PIN 17
#define BUZZER_PIN 26  // Sử dụng GPIO 26 cho buzzer
#define BT0 13  // Nút lên
#define BT1 12  // Nút vào menu
#define BT2 14  // Nút xuống và quét RFID
#define BT3 27  // Nút trở về

#define WIFI_SSID "GT"
#define WIFI_PASSWORD "11111144"
#define FIREBASE_HOST "phathuy-4e78b-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "b14Txzd7SpbczEKN1GEPbs6JxfwNR96TwErfTt3Q" // Thay bằng mã xác thực Firebase của bạn

HardwareSerial rfidSerial(1);
LiquidCrystal_I2C lcd(0x27, 20, 4);

FirebaseData firebaseData;
FirebaseConfig firebaseConfig;
FirebaseAuth firebaseAuth;

const uint8_t setRegionChina2[] PROGMEM = {0xBB, 0x00, 0x07, 0x00, 0x01, 0x01, 0x09, 0x7E};
const uint8_t setTxPower26dBm[] PROGMEM = {0xBB, 0x00, 0xB6, 0x00, 0x02, 0x07, 0xD0, 0x8F, 0x7E};
const uint8_t setChannel[] PROGMEM = {0xBB, 0x00, 0xAB, 0x00, 0x01, 0x01, 0xAC, 0x7E};
const uint8_t singlePollingInstruction[] PROGMEM = {0xBB, 0x00, 0x22, 0x00, 0x00, 0x22, 0x7E};
const uint8_t continuousPollingInstruction[] PROGMEM = {0xBB, 0x00, 0x27, 0x00, 0x00, 0x27, 0x7E}; // Command for continuous scanning
const uint8_t stopContinuousPollingInstruction[] PROGMEM = {0xBB, 0x00, 0x28, 0x00, 0x00, 0x28, 0x7E}; // Command to stop continuous scanning

unsigned long lastButtonPress = 0;
int selectedMenu = 2;
bool welcomeScreenDisplayed = true;
unsigned long scanStartTime = 0;
bool inSubMenu = false;
bool isScanning = false;
bool inSearchMenu = false;  // New flag to indicate search menu
int productCount = 0;

struct ProductInfo {
  String rfidData;
  int count;
};

ProductInfo scannedProducts[10];
int numberOfScannedProducts = 0;
int cursorPosition = 1;

struct ProductEntry {
  int productNumber;
  String rfidData;
  String scannedIDs[20];  // Giảm số lượng ID lưu trữ trong mỗi mục
  int scannedIDCount;
};

ProductEntry productEntries[20];  // Giảm số lượng mục sản phẩm
int productEntryCount = 0;

int menuStartIndex = 0;  // Menu start index for scrolling
int searchMenuStartIndex = 0;  // Start index for scrolling in search menu

String getProductName(int productNumber);
void updatePointer(int oldPos, int newPos);
void deleteProductID(String rfidTag);  // New function to delete ID

void setup() {
  Wire.begin(21, 22); // Initialize I2C with specified pins
  lcd.init();
  lcd.backlight();

  pinMode(BT0, INPUT_PULLUP);
  pinMode(BT1, INPUT_PULLUP);
  pinMode(BT2, INPUT_PULLUP);
  pinMode(BT3, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  rfidSerial.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  displayWelcomeScreen();

  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&firebaseConfig, &firebaseAuth);

  SendCommand(setRegionChina2, sizeof(setRegionChina2));
  delay(100);
  SendCommand(setTxPower26dBm, sizeof(setTxPower26dBm));
  delay(100);
  SendCommand(setChannel, sizeof(setChannel));
  delay(100);
}

void loop() {
  if (!welcomeScreenDisplayed) {
    if (millis() - lastButtonPress > 200) {
      if (!inSubMenu && !inSearchMenu) {
        if (digitalRead(BT0) == LOW && millis() - lastButtonPress > 200) {
          if (selectedMenu > 2) {
            updatePointer(selectedMenu, selectedMenu - 1);
          }
          lastButtonPress = millis();
        } else if (digitalRead(BT1) == LOW && millis() - lastButtonPress > 200) {
          enterSubMenu();
          lastButtonPress = millis();
        } else if (digitalRead(BT2) == LOW && millis() - lastButtonPress > 200) {
          if (selectedMenu < 4) {
            updatePointer(selectedMenu, selectedMenu + 1);
          }
          lastButtonPress = millis();
        } else if (digitalRead(BT3) == LOW && millis() - lastButtonPress > 200) {
          leaveSubMenu();
          lastButtonPress = millis();
        }
      } else if (inSubMenu && !inSearchMenu) {
        if (digitalRead(BT3) == LOW && millis() - lastButtonPress > 200) {
          leaveSubMenu();
          lastButtonPress = millis();
        } else if (selectedMenu == 2) { // Quét và xóa RFID ở menu 2
          if (digitalRead(BT2) == LOW && millis() - lastButtonPress > 200) {
            isScanning = true;
            scanStartTime = millis();
            processRFID(true); // Thực hiện xóa ID
            lastButtonPress = millis();
          }
        } else if (selectedMenu == 3) { // Quét RFID ở menu 1
          if (digitalRead(BT2) == LOW && millis() - lastButtonPress > 200) {
            isScanning = true;
            scanStartTime = millis();
            processRFID(false); // Không thực hiện xóa ID
            lastButtonPress = millis();
          }
        } else if (selectedMenu == 4) {
          if (digitalRead(BT0) == LOW && millis() - lastButtonPress > 200) {
            cursorPosition--;
            if (cursorPosition < 1) {
              cursorPosition = 1;
              if (menuStartIndex > 0) {
                menuStartIndex--;
              }
            }
            displaySubMenu();
            lastButtonPress = millis();
          } else if (digitalRead(BT2) == LOW && millis() - lastButtonPress > 200) {
            cursorPosition++;
            if (cursorPosition > 3) {
              cursorPosition = 3;
              if (menuStartIndex + 3 < productEntryCount) {
                menuStartIndex++;
              }
            }
            displaySubMenu();
            lastButtonPress = millis();
          } else if (digitalRead(BT1) == LOW && millis() - lastButtonPress > 200) {
            enterSearchMenu();
            lastButtonPress = millis();
          }
        }
      } else if (inSearchMenu) {
        if (digitalRead(BT3) == LOW && millis() - lastButtonPress > 200) {
          leaveSearchMenu();
          lastButtonPress = millis();
        } else if (digitalRead(BT0) == LOW && millis() - lastButtonPress > 200) {
          searchMenuStartIndex--;
          if (searchMenuStartIndex < 0) searchMenuStartIndex = 0;
          displaySearchMenu();
          lastButtonPress = millis();
        } else if (digitalRead(BT2) == LOW && millis() - lastButtonPress > 200) {
          int productIndex = menuStartIndex + cursorPosition - 1;
          if (searchMenuStartIndex < productEntries[productIndex].scannedIDCount - 3) {
            searchMenuStartIndex++;
          }
          displaySearchMenu();
          lastButtonPress = millis();
        }
      }
    }

    if (isScanning && (millis() - scanStartTime < 1000)) {
      processRFID(selectedMenu == 2); // Pass true if in menu 2, else false
      isScanning = false;
    }

    if (inSearchMenu) {
      continuousRFIDScan();
    }
  }
}

void displayWelcomeScreen() {
  lcd.setCursor(2, 0);
  lcd.print("DO AN TOT NGHIEP");
  lcd.setCursor(0, 1);
  lcd.print("HE THONG QUAN LY KHO");
  lcd.setCursor(4, 2);
  lcd.print("DINH PHI HO");
  lcd.setCursor(4, 3);
  lcd.print("LUU PHAT HUY");

  delay(2000);
  
  displayWiFiStatus();
}

void displayWiFiStatus() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Da ket noi WiFi");
  delay(2000);

  displayMainMenu();
  welcomeScreenDisplayed = false;
}

void displayMainMenu() {
  lcd.setCursor(0, 0);
  lcd.print("HE THONG QUAN LY KHO");
  lcd.setCursor(1, 1);
  lcd.print("SAN PHAM XUAT KHO");
  lcd.setCursor(1, 2);
  lcd.print("SAN PHAM NHAP KHO");
  lcd.setCursor(1, 3);
  lcd.print("SAN PHAM TRONG KHO");
  lcd.setCursor(0, selectedMenu - 1);
  lcd.print(">");
}

void updatePointer(int oldPos, int newPos) {
  lcd.setCursor(0, oldPos - 1);
  lcd.print(" ");
  lcd.setCursor(0, newPos - 1);
  lcd.print(">");
  selectedMenu = newPos;
}

void enterSubMenu() {
  inSubMenu = true;
  lcd.clear();
  if (selectedMenu == 2) {
    displayMenu1();
  } else if (selectedMenu == 3) {
    displayMenu2();
    // Increase product count and store new product entry
    productCount++;
    productEntries[productEntryCount].productNumber = productCount;
    productEntries[productEntryCount].scannedIDCount = 0;
    productEntryCount++;
  } else if (selectedMenu == 4) {
    displayMenu3();
  }
}

void displayMenu1() {
  lcd.setCursor(0, 0);
  lcd.print("SAN PHAM XUAT KHO");
  lcd.setCursor(0, 1);
  lcd.print("MA: ");
}

void displayMenu2() {
  lcd.setCursor(0, 0);
  lcd.print("SAN PHAM NHAP KHO");
  lcd.setCursor(0, 1);
  lcd.print("MA: ");
}

void displayMenu3() {
  lcd.setCursor(0, 0);
  lcd.print("SAN PHAM TRONG KHO");
  displaySubMenu();
}

void displaySubMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SAN PHAM TRONG KHO");
  int lcdRow = 1;
  for (int i = menuStartIndex; i < productEntryCount && lcdRow < 4; i++) {
    lcd.setCursor(0, lcdRow);
    if (i == menuStartIndex + cursorPosition - 1) {
      lcd.print(">");
    } else {
      lcd.print(" ");
    }
    lcd.setCursor(1, lcdRow);
    lcd.print("SAN PHAM ");
    lcd.print(productEntries[i].productNumber);
    lcdRow++;
  }
}

void enterSearchMenu() {
  inSearchMenu = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TIM KIEM SAN PHAM");
  searchMenuStartIndex = 0; // Reset the start index for scrolling
  displaySearchMenu();
}

void displaySearchMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("TIM KIEM SAN PHAM");
  int productIndex = menuStartIndex + cursorPosition - 1;
  int lcdRow = 1;
  for (int i = searchMenuStartIndex; i < productEntries[productIndex].scannedIDCount && lcdRow < 4; i++) {
    lcd.setCursor(0, lcdRow);
    lcd.print(productEntries[productIndex].scannedIDs[i]);
    lcdRow++;
  }
}

void leaveSubMenu() {
  inSubMenu = false;
  lcd.clear();
  displayMainMenu();
}

void leaveSearchMenu() {
  inSearchMenu = false;
  lcd.clear();
  displaySubMenu();
  // Send command to stop continuous scanning
  SendCommand(stopContinuousPollingInstruction, sizeof(stopContinuousPollingInstruction));
  digitalWrite(BUZZER_PIN, LOW); // Turn off buzzer
}

void SendCommand(const uint8_t *cmd, size_t len) {
  rfidSerial.write(cmd, len);
  rfidSerial.flush();
  delay(10);
}

String getFixedRFIDTag(uint8_t *response, size_t len) {
  String rfidHex = "";
  for (size_t i = 7; i < len - 2; i++) {
    if (response[i] < 0x10) {
      rfidHex += "0";
    }
    rfidHex += String(response[i], HEX);
  }
  return rfidHex;
}

void processRFID(bool deleteMode) {
  SendCommand(singlePollingInstruction, sizeof(singlePollingInstruction));
  delay(1000);

  while (rfidSerial.available()) {
    uint8_t response[64];
    size_t idx = 0;

    while (rfidSerial.available() && idx < sizeof(response)) {
      response[idx++] = rfidSerial.read();
    }

    Serial.print("Raw response: ");
    for (size_t i = 0; i < idx; i++) {
      Serial.print(response[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    if (idx > 7 && response[0] == 0xBB && response[idx - 1] == 0x7E) {
      String rfidTagHex = getFixedRFIDTag(response, idx);
      unsigned long rfidTag = 0;
      for (size_t i = 0; i < rfidTagHex.length(); i += 2) {
        String byteString = rfidTagHex.substring(i, i + 2);
        rfidTag <<= 8;
        rfidTag |= strtoul(byteString.c_str(), NULL, 16);
      }
      String rfidTagStr = String(rfidTag);

      if (rfidTagStr != "0") { // Kiểm tra nếu ID không bằng 0
        String productName = getProductName(productCount);

        Serial.print("RFID Tag ID: ");
        Serial.println(rfidTagStr);

        lcd.setCursor(0, 1);
        lcd.print("MA:                ");
        lcd.setCursor(4, 1);
        lcd.print(rfidTagStr);

        lcd.setCursor(0, 2);
        lcd.print("TEN:               ");
        lcd.setCursor(4, 2);
        lcd.print(productName);

        // Kích hoạt buzzer
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200); // Buzzer kêu trong 200ms
        digitalWrite(BUZZER_PIN, LOW);

        if (deleteMode) {
          deleteProductID(rfidTagStr); // Xóa ID nếu ở menu 2 và deleteMode là true
        } else {
          updateProductCount(rfidTagStr);

          // Store the RFID data in the product entries
          if (selectedMenu == 3) {
            productEntries[productEntryCount - 1].rfidData = rfidTagStr;
            productEntries[productEntryCount - 1].scannedIDs[productEntries[productEntryCount - 1].scannedIDCount++] = rfidTagStr;
          }
          sendDataToFirebase(); // Gửi dữ liệu lên Firebase khi quét sản phẩm ở menu 1
        }
      }
    } else {
      Serial.println("Invalid response received");
      lcd.setCursor(0, 1);
      lcd.print("MA: 0");
    }
  }
  Serial.println();
}

void continuousRFIDScan() {
  SendCommand(continuousPollingInstruction, sizeof(continuousPollingInstruction));

  while (rfidSerial.available()) {
    uint8_t response[64];
    size_t idx = 0;

    while (rfidSerial.available() && idx < sizeof(response)) {
      response[idx++] = rfidSerial.read();
    }

    Serial.print("Raw response: ");
    for (size_t i = 0; i < idx; i++) {
      Serial.print(response[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    if (idx > 7 && response[0] == 0xBB && response[idx - 1] == 0x7E) {
      String rfidTagHex = getFixedRFIDTag(response, idx);
      unsigned long rfidTag = 0;
      for (size_t i = 0; i < rfidTagHex.length(); i += 2) {
        String byteString = rfidTagHex.substring(i, i + 2);
        rfidTag <<= 8;
        rfidTag |= strtoul(byteString.c_str(), NULL, 16);
      }
      String rfidTagStr = String(rfidTag);
      int productIndex = menuStartIndex + cursorPosition - 1;

      for (int i = 0; i < productEntries[productIndex].scannedIDCount; i++) {
        if (rfidTagStr == productEntries[productIndex].scannedIDs[i]) {
          // Kích hoạt buzzer liên tục khi tìm thấy sản phẩm
          digitalWrite(BUZZER_PIN, HIGH);
          break;
        } else {
          digitalWrite(BUZZER_PIN, LOW);
        }
      }
    } else {
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("Invalid response received");
    }
  }
}

void updateProductCount(String rfidTag) {
  bool found = false;
  for (int i = 0; i < numberOfScannedProducts; i++) {
    if (scannedProducts[i].rfidData == rfidTag) {
      scannedProducts[i].count++;
      found = true;
      break;
    }
  }
  if (!found && numberOfScannedProducts < 10) {
    scannedProducts[numberOfScannedProducts].rfidData = rfidTag;
    scannedProducts[numberOfScannedProducts].count = 1;
    numberOfScannedProducts++;
  }
}

// Hàm xóa ID sản phẩm
void deleteProductID(String rfidTag) {
  for (int i = 0; i < productEntryCount; i++) {
    for (int j = 0; j < productEntries[i].scannedIDCount; j++) {
      if (productEntries[i].scannedIDs[j] == rfidTag) {
        for (int k = j; k < productEntries[i].scannedIDCount - 1; k++) {
          productEntries[i].scannedIDs[k] = productEntries[i].scannedIDs[k + 1];
        }
        productEntries[i].scannedIDCount--;
        break;
      }
    }
  }
  sendDataToFirebase(); // Gửi dữ liệu lên Firebase khi xóa sản phẩm ở menu 2
}

// Định nghĩa hàm getProductName
String getProductName(int productNumber) {
  return "SAN PHAM " + String(productNumber);
}

void sendDataToFirebase() {
  for (int i = 0; i < productEntryCount; i++) {
    String path = "/tttt/Masanpham" + String(productEntries[i].productNumber);
    Firebase.setString(firebaseData, path + "/rfidData", productEntries[i].rfidData);
    for (int j = 0; j < productEntries[i].scannedIDCount; j++) {
      Firebase.setString(firebaseData, path + "/scannedIDs/" + String(j), productEntries[i].scannedIDs[j]);
    }
  }
}
