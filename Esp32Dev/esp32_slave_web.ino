#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// =================== CẤU HÌNH WIFI ===================
const char *ssid = "Esp32Cam";
const char *password = "";       

// =================== I2C PIN  ==============
#define I2C_SDA 21
#define I2C_SCL 22
#define SLAVE_ADDR 8

// =================== LED PIN ==============
#define LED_TRAI 4
#define LED_PHAI 2

WebServer server(80);

volatile int current_sign_id = 0; 
volatile unsigned long last_receive_time = 0;

// Hàm ngắt chạy khi nhận I2C
void receiveEvent(int howMany) {
    if (Wire.available()) {
        current_sign_id = Wire.read(); 
        last_receive_time = millis();
    }
}

const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Mắt Thần Robot</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background: #2c3e50; color: white; padding-top: 50px; }
        .box { width: 300px; height: 300px; background: #34495e; border-radius: 20px; margin: 0 auto; display: flex; flex-direction: column; justify-content: center; align-items: center; box-shadow: 0 10px 20px rgba(0,0,0,0.5); }
        .icon { font-size: 100px; margin-bottom: 20px; }
        .text { font-size: 24px; font-weight: bold; text-transform: uppercase; }
        
        .bg-trai { background: #3498db; }
        .bg-phai { background: #e67e22; }
        .bg-none { background: #7f8c8d; }
    </style>
</head>
<body>
    <h1>TRẠM ĐIỀU KHIỂN ROBOT</h1>
    <p>Đang nhận tín hiệu từ ESP32-S3 AI...</p>
    
    <div class="box bg-none" id="status-box">
        <div class="icon" id="icon">❓</div>
        <div class="text" id="label">Đang chờ tín hiệu</div>
    </div>

    <script>
        function updateData() {
            fetch("/data").then(res => res.text()).then(id => {
                let box = document.getElementById("status-box");
                let icon = document.getElementById("icon");
                let label = document.getElementById("label");

                box.className = "box"; // Reset class

                if (id === "1") {
                    box.classList.add("bg-trai");
                    icon.innerText = "⬅️";
                    label.innerText = "Bắt buộc RẼ TRÁI";
                } else if (id === "2") {
                    box.classList.add("bg-phai");
                    icon.innerText = "➡️";
                    label.innerText = "Bắt buộc RẼ PHẢI";
                } else {
                    box.classList.add("bg-none");
                    icon.innerText = "❓";
                    label.innerText = "Không thấy gì";
                }
            });
        }
        
        // Tự động hỏi ESP32 10 lần mỗi giây
        setInterval(updateData, 100);
    </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);

    
    pinMode(LED_TRAI, OUTPUT);
    pinMode(LED_PHAI, OUTPUT);

    // I2C INIT
    Wire.onReceive(receiveEvent);
    Wire.begin((uint8_t)SLAVE_ADDR, I2C_SDA, I2C_SCL, 100000);
    Serial.println("I2C Slave đã khởi tạo ở địa chỉ 0x08");

    // ACCESS POINT INIT
    WiFi.softAP(ssid, password);
    Serial.print("Mạng WiFi Đã Phát: ");
    Serial.println(ssid);
    Serial.print("Vui lòng truy cập IP: ");
    Serial.println(WiFi.softAPIP()); 

    // WEB SERVER INIT
    server.on("/", []() {
        server.send(200, "text/html", html_page);
    });

    server.on("/data", []() {
        // Return về ID biển báo
        server.send(200, "text/plain", String(current_sign_id));
        
    });

    server.begin();
    Serial.println("Web Server đang chạy...");
}

void loop() {
    server.handleClient();
    
    static int last_id = -1;
    if (current_sign_id != 0 && current_sign_id != last_id) {
        Serial.print("Đã nhận tín hiệu MỚI: ID = ");
        Serial.println(current_sign_id);
        last_id = current_sign_id;
    }
    
    // Nếu đang giữ một biển báo, nhưng đã quá 2 giây không gửi thêm tín hiệu nào
    if (current_sign_id != 0 && (millis() - last_receive_time > 2000)) {
        Serial.println("Quá 2 giây không thấy lại biển báo -> Trở về trạng thái Không Có Gì");
        current_sign_id = 0; // Reset
    }
    
    if (current_sign_id == 0) last_id = -1; 
    
    static unsigned long previousMillis = 0;
    static bool ledState = false;
    unsigned long currentMillis = millis();
    
    if (currentMillis - previousMillis >= 500) {
        previousMillis = currentMillis;
        ledState = !ledState;
    }
    
    if (current_sign_id == 1) { // RẼ TRÁI
        digitalWrite(LED_TRAI, ledState); // Chớp LED trái
        digitalWrite(LED_PHAI, LOW);      // Tắt LED phải
    } 
    else if (current_sign_id == 2) { // RẼ PHẢI
        digitalWrite(LED_TRAI, LOW);      // Tắt LED trái
        digitalWrite(LED_PHAI, ledState); // Chớp LED phải
    } 
    else { // CHƯA THẤY GÌ HOẶC ID = 0
        digitalWrite(LED_TRAI, LOW);
        digitalWrite(LED_PHAI, LOW);
    }
}
