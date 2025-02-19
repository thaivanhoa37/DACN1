#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESPDash.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Định nghĩa kích thước của màn hình OLED
#define SCREEN_WIDTH 128 // Chiều rộng màn hình OLED, tính bằng pixel
#define SCREEN_HEIGHT 64 // Chiều cao màn hình OLED, tính bằng pixel
#define OLED_RESET -1 // Chân reset (hoặc -1 nếu chia sẻ chân reset của Arduino)
#define SCREEN_ADDRESS 0x3C // Địa chỉ I2C của màn hình OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Thông tin Wi-Fi
const char* ssid = "DACN1"; // Tên Wi-Fi (SSID)
const char* password = "22222222"; // Mật khẩu Wi-Fi

// Giá trị đặt và độ trễ (tính bằng độ C)
int setpoint = 0;
int hysteresis = 0;

// Số chân cho relay điều khiển sưởi và làm mát
const int HEATING_PIN = 18; // Relay1
const int COOLING_PIN = 19; // Relay2

#define ONE_WIRE_BUS 23 // Chân cho cảm biến nhiệt độ OneWire
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// Chế độ điều khiển (tự động hoặc thủ công)
bool automaticMode = 0;
bool heaterbtn = 0;
bool coolerbtn = 0;

// Khởi tạo web server
AsyncWebServer server(80);

// Kết nối ESP-DASH với AsyncWebServer
ESPDash dashboard(&server);

// Các thẻ trên Dashboard
Card temperature(&dashboard, TEMPERATURE_CARD, "Nhiệt độ hiện tại", "°C");
Card setTemp(&dashboard, SLIDER_CARD, "Nhiệt độ", "", 0, 50);
// Card setHyss(&dashboard, SLIDER_CARD, "Độ trễ", "", 0, 5);
Card autoMode(&dashboard, BUTTON_CARD, "Auto Mode");
Card ModeStatus(&dashboard, STATUS_CARD, "Trạng thái Auto Mode");
Card Heater(&dashboard, BUTTON_CARD, "Quạt");
Card Cooler(&dashboard, BUTTON_CARD, "Máy sưởi");
Card Heat(&dashboard, BUTTON_CARD, "Quạt");
Card Cool(&dashboard, BUTTON_CARD, "Máy sưởi");

void setup() {
  Serial.begin(115200);

  // Khởi tạo relay và cảm biến nhiệt độ
  pinMode(HEATING_PIN, OUTPUT);
  pinMode(COOLING_PIN, OUTPUT);

  // Khởi tạo màn hình OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Khởi tạo SSD1306 thất bại"));
    for (;;); // Dừng lại, lặp vô hạn
  }
  display.clearDisplay();

  // Thiết lập Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);
  Serial.print("Địa chỉ IP: ");
  Serial.println(WiFi.softAPIP());

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.println("IP: ");
  display.setCursor(22, 2);
  display.println(WiFi.softAPIP());
  display.display();

  // Tắt các relay ban đầu
  digitalWrite(HEATING_PIN, HIGH);
  Serial.println("Máy sưởi TẮT");
  digitalWrite(COOLING_PIN, HIGH);
  Serial.println("Máy lạnh TẮT");

  // Khởi động WebServer
  server.begin();

  // Khởi động thư viện cảm biến nhiệt độ
  sensors.begin();
}

void loop() {
  // Gửi lệnh để lấy nhiệt độ
  sensors.requestTemperatures(); 
  float currentTemp = sensors.getTempCByIndex(0);
  
  // Thiết lập xác thực cho Dashboard
  dashboard.setAuthentication("admin", "");

  // Hiển thị thông tin trên màn hình OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 2);
  display.println("IP: ");
  display.setCursor(22, 2);
  display.println(WiFi.softAPIP());
  display.display();
  display.setTextSize(3);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 17);
  display.println(currentTemp);
  display.println(" ");
  display.drawRect(90, 17, 5, 5, WHITE); // Ký hiệu độ (°)
  display.setCursor(97, 17);
  display.println("C");
  display.display();

  // Cập nhật giá trị thẻ trên Dashboard
  temperature.update((int)(currentTemp));

  // Callback cho thẻ nhiệt độ
  setTemp.attachCallback([&](int value) {
    Serial.println("[setTemp] Slider Callback Triggered: " + String(value));
    setpoint = value;
    setTemp.update(value);
    dashboard.sendUpdates();
  });

  // Callback cho thẻ độ trễ
  // setHyss.attachCallback([&](int value) {
  //   Serial.println("[setHyss] Slider Callback Triggered: " + String(value));
  //   hysteresis = value;
  //   setHyss.update(value);
  //   dashboard.sendUpdates();
  // });

  Serial.println(hysteresis);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 55);
  display.print("Hys: ");
  display.print(hysteresis);
  display.display();
  Serial.println(setpoint);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(70, 55);
  display.print("Temp: ");
  display.print(setpoint);
  display.display();

  // Callback cho chế độ tự động
  autoMode.attachCallback([&](int value) {
    Serial.println("[autoMode] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    automaticMode = value;
    autoMode.update(value);
    dashboard.sendUpdates();
  });

  // Hiển thị trạng thái chế độ tự động trên Dashboard
  if (automaticMode == 1) {
    Serial.println("Chế độ tự động bật");
    ModeStatus.update("Enabled", "success");
    dashboard.sendUpdates();
  } else {
    Serial.println("Chế độ tự động tắt");
    ModeStatus.update("Disabled", "danger");
    dashboard.sendUpdates();
  }

  // Callback cho nút máy sưởi
  Heater.attachCallback([&](int value) {
    Serial.println("[Heater] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    heaterbtn = value;
    if (automaticMode == 0 && heaterbtn == 1) {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Máy sưởi BẬT");
    } else if (automaticMode == 0 && heaterbtn == 0) {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Máy sưởi TẮT");
    }
    Heater.update(value);
    dashboard.sendUpdates();
  });

  // Callback cho nút máy lạnh
  Cooler.attachCallback([&](int value) {
    Serial.println("[Cooler] Button Callback Triggered: " + String((value == 1) ? "true" : "false"));
    coolerbtn = value;
    if (automaticMode == 0 && coolerbtn == 1) {
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Máy lạnh BẬT");
    } else if (automaticMode == 0 && coolerbtn == 0) {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Máy lạnh TẮT");
    }
    Cooler.update(value);
    dashboard.sendUpdates();
  });

  // Điều khiển relay sưởi và làm mát trong chế độ tự động
  if (automaticMode == 1) {
    if (currentTemp > setpoint + hysteresis) {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Máy sưởi TẮT");
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Máy lạnh BẬT");
    } else if (currentTemp < setpoint - hysteresis) {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Máy sưởi BẬT");
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Máy lạnh TẮT");
    } else {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Máy sưởi TẮT");
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Máy lạnh TẮT");
    }
  } else if (automaticMode == 0) {
    if (heaterbtn == 1) {
      digitalWrite(HEATING_PIN, LOW);
      Serial.println("Máy sưởi BẬT");
    } else {
      digitalWrite(HEATING_PIN, HIGH);
      Serial.println("Máy sưởi TẮT");
    }
    if (coolerbtn == 1) {
      digitalWrite(COOLING_PIN, LOW);
      Serial.println("Máy lạnh BẬT");
    } else {
      digitalWrite(COOLING_PIN, HIGH);
      Serial.println("Máy lạnh TẮT");
    }
  }

  // Gửi cập nhật đến Dashboard (theo thời gian thực)
  dashboard.sendUpdates();

  // Dùng delay chỉ để minh họa, trong dự án thực tế nên thay bằng 'millis'
  delay(3000);
}
