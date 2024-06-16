#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

#define RX_PIN 2
#define TX_PIN 3

SoftwareSerial rfidSerial(RX_PIN, TX_PIN);

LiquidCrystal_I2C lcd(0x27, 20, 4);

const int BT0 = A0;
const int BT1 = A1;
const int BT2 = A2;
const int BT3 = A3;

unsigned long lastButtonPress = 0;
int selectedMenu = 2;
bool welcomeScreenDisplayed = true;

bool inSubMenu = false;
bool readRFIDAllowed = false;

struct ProductInfo {
  long rfidData;
  int count;
};

ProductInfo scannedProducts[10];
int numberOfScannedProducts = 0;
int cursorPosition = 1;

void setup() {
  lcd.init();
  lcd.backlight();

  pinMode(BT0, INPUT_PULLUP);
  pinMode(BT1, INPUT_PULLUP);
  pinMode(BT2, INPUT_PULLUP);
  pinMode(BT3, INPUT_PULLUP);

  rfidSerial.begin(9600);
  Serial.begin(9600);

  displayWelcomeScreen();
}

void loop() {
  if (!welcomeScreenDisplayed) {
    if (millis() - lastButtonPress > 200) {
      if (!inSubMenu) {
        if (digitalRead(BT0) == LOW && lastButtonPress != BT0) {
          if (selectedMenu > 2) {
            updatePointer(selectedMenu, selectedMenu - 1);
            lastButtonPress = BT0;
          }
          lastButtonPress = millis();
        } else if (digitalRead(BT1) == LOW && lastButtonPress != BT1) {
          enterSubMenu();
          lastButtonPress = BT1;
          lastButtonPress = millis();
        } else if (digitalRead(BT2) == LOW && lastButtonPress != BT2) {
          if (selectedMenu < 4) {
            updatePointer(selectedMenu, selectedMenu + 1);
            lastButtonPress = BT2;
          }
          lastButtonPress = millis();
        } else if (digitalRead(BT3) == LOW && lastButtonPress != BT3) {
          leaveSubMenu();
          lastButtonPress = BT3;
          lastButtonPress = millis();
        }
      } else {
        if (digitalRead(BT3) == LOW && lastButtonPress != BT3) {
          leaveSubMenu();
          lastButtonPress = BT3;
          lastButtonPress = millis();
        } else if (selectedMenu == 4) {
          if (digitalRead(BT0) == LOW && cursorPosition > 1 && lastButtonPress != BT0) {
            cursorPosition--;
            displaySubMenu();
            lastButtonPress = BT0;
            lastButtonPress = millis();
          } else if (digitalRead(BT2) == LOW && cursorPosition < numberOfScannedProducts && lastButtonPress != BT2) {
            cursorPosition++;
            displaySubMenu();
            lastButtonPress = BT2;
            lastButtonPress = millis();
          }
        }
      }
    }

    if (selectedMenu == 2 && readRFIDAllowed) {
      if (rfidSerial.available() > 0) {
        long rfidData = readRFID();
        Serial.print("RFID Decoded: ");
        Serial.println(rfidData);

        bool found = false;
        for (int i = 0; i < numberOfScannedProducts; i++) {
          if (scannedProducts[i].rfidData == rfidData) {
            scannedProducts[i].count++;
            found = true;
            break;
          }
        }

        if (!found) {
          scannedProducts[numberOfScannedProducts].rfidData = rfidData;
          scannedProducts[numberOfScannedProducts].count = 1;
          numberOfScannedProducts++;
        }

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SAN PHAM NHAP KHO");
        lcd.setCursor(0, 1);
        lcd.print("MA: ");
        lcd.print(rfidData);

        lcd.setCursor(0, 2);
        lcd.print("Ten: ");
        lcd.print(getProductName(rfidData));
      }
    } else if (selectedMenu == 3 && readRFIDAllowed) {
      if (rfidSerial.available() > 0 || numberOfScannedProducts == 0) {
        long rfidData = readRFID();
        Serial.print("RFID Decoded: ");
        Serial.println(rfidData);

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("SAN PHAM XUAT KHO");
        lcd.setCursor(0, 1);
        lcd.print("MA: ");
        lcd.print(rfidData);

        lcd.setCursor(0, 2);
        lcd.print("Ten: ");
        lcd.print(getProductName(rfidData));

        // Trừ số lần lặp lại từ sản phẩm đã quét ở selectedMenu == 2
        bool productFound = false;
        for (int i = 0; i < numberOfScannedProducts; i++) {
          if (scannedProducts[i].rfidData == rfidData) {
            productFound = true;
            if (scannedProducts[i].count > 0) {
              scannedProducts[i].count--;
            } else {
              lcd.setCursor(2, 3);
              lcd.print("SAN PHAM DA HET");
            }
            break;
          }
        }
        if (!productFound) {
          lcd.setCursor(3, 3);
          lcd.print("KHONG TON TAI");
        }
      }
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

  lcd.clear();
  displayMenu();
  welcomeScreenDisplayed = false;
}

void displayMenu() {
  lcd.setCursor(0, 0);
  lcd.print("HE THONG QUAN LY KHO");
  lcd.setCursor(1, 1);
  lcd.print("SAN PHAM NHAP KHO");
  lcd.setCursor(1, 2);
  lcd.print("SAN PHAM XUAT KHO");
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
    lcd.setCursor(0, 0);
    lcd.print("SAN PHAM NHAP KHO");
    lcd.setCursor(0, 1);
    lcd.print("MA: ");
    lcd.setCursor(0, 2);
    lcd.print("Ten: ");
    readRFIDAllowed = true;
  } else if (selectedMenu == 3) {
    lcd.setCursor(0, 0);
    lcd.print("SAN PHAM XUAT KHO");
    lcd.setCursor(0, 1);
    lcd.print("MA: ");
    lcd.setCursor(0, 2);
    lcd.print("Ten: ");
    readRFIDAllowed = true;
  } else if (selectedMenu == 4) {
    lcd.setCursor(0, 0);
    lcd.print("SAN PHAM TRONG KHO");
    displaySubMenu();
    readRFIDAllowed = false;
  }
}

void displaySubMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SAN PHAM TRONG KHO");
  int startIndex = max(0, cursorPosition - 2);
  int endIndex = min(numberOfScannedProducts, cursorPosition + 1);
  int lcdRow = 1;
  for (int i = startIndex; i < endIndex; i++) {
    lcd.setCursor(0, lcdRow);
    if (i == cursorPosition - 1) {
      lcd.print(">");
    } else {
      lcd.print(" ");
    }
    lcd.setCursor(1, lcdRow);

    lcd.print(i + 1); // Số thứ tự sản phẩm
    lcd.print(". "); // Dấu chấm sau số thứ tự
    lcd.print(getProductName(scannedProducts[i].rfidData)); // Tên sản phẩm
    lcd.setCursor(17, lcdRow);

    // Hiển thị số lần lặp lại là số 0 nếu là số âm
    if (scannedProducts[i].count < 0) {
      lcd.print(0);
    } else {
      lcd.print(scannedProducts[i].count); // Số lần xuất hiện
    }

    lcdRow++;
    if (lcdRow >= 3 && i == endIndex - 1 && cursorPosition < numberOfScannedProducts) {
      lcdRow--;
    }
  }
}

void leaveSubMenu() {
  inSubMenu = false;
  lcd.clear();
  displayMenu();
}

long readRFID() {
  long value = 0;
  for (int j = 0; j < 4; j++) {
    while (!rfidSerial.available()) {};
    int i = rfidSerial.read();
    value += ((long)i << (24 - (j * 8)));
  }
  return value;
}

String getProductName(long rfidData) {
  if (rfidData == 12928233) {
    return "CoCa CoLa";
  } else if (rfidData == 7678152) {
    return "Pesi";
  } else if (rfidData == 12043585) {
    return "Mirinda";
  } else if (rfidData == 12481416) {
    return "Bo Hut";
  } else if (rfidData == 4408930) {
    return "Sting";
  } else {
    return "KHAC";
  }
}
