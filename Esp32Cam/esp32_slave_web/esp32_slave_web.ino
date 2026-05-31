#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// =================== CẤU HÌNH WIFI ===================
const char *ssid = "Esp32Cam"; // Tên WiFi do ESP phát ra
const char *password = "";        // Không cần mật khẩu

// =================== CẤU HÌNH I2C SLAVE ==============
#define I2C_SDA 21
#define I2C_SCL 22
#define SLAVE_ADDR 8

// =================== CẤU HÌNH LED XI NHAN ==============
#define LED_TRAI 4
#define LED_PHAI 2

WebServer server(80);

// Biến lưu trữ ID biển báo nhận được từ ESP32-S3
volatile int current_sign_id = 0; 
volatile unsigned long last_receive_time = 0; // Lưu lại thời điểm cuối cùng nhận được tín hiệu

// Hàm ngắt (Interrupt) chạy mỗi khi nhận được I2C
void receiveEvent(int howMany) {
    if (Wire.available()) {
        current_sign_id = Wire.read(); 
        last_receive_time = millis(); // Cập nhật lại đồng hồ đếm giờ ngay lập tức
    }
}

// Giao diện HTML
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

    // Cấu hình chân LED Xi nhan
    pinMode(LED_TRAI, OUTPUT);
    pinMode(LED_PHAI, OUTPUT);

    // 1. Cấu hình I2C SLAVE
    Wire.onReceive(receiveEvent);
    Wire.begin((uint8_t)SLAVE_ADDR, I2C_SDA, I2C_SCL, 100000);
    Serial.println("I2C Slave đã khởi tạo ở địa chỉ 0x08");

    // 2. Phát WiFi (Access Point)
    WiFi.softAP(ssid, password);
    Serial.print("Mạng WiFi Đã Phát: ");
    Serial.println(ssid);
    Serial.print("Vui lòng truy cập IP: ");
    Serial.println(WiFi.softAPIP()); // Mặc định là 192.168.4.1

    // 3. Cấu hình Web Server
    server.on("/", []() {
        server.send(200, "text/html", html_page);
    });

    server.on("/data", []() {
        // Trả về ID dưới dạng chữ Text (0, 1 hoặc 2)
        server.send(200, "text/plain", String(current_sign_id));
        
        // ĐÃ XÓA LỆNH RESET: Để web và hệ thống tự động nhớ trạng thái mãi mãi cho đến khi có biển báo mới
    });

    server.begin();
    Serial.println("Web Server đang chạy...");
}

void loop() {
    server.handleClient();
    
    // In ra màn hình Serial để debug (mỗi khi có tín hiệu mới)
    static int last_id = -1;
    if (current_sign_id != 0 && current_sign_id != last_id) {
        Serial.print("Đã nhận tín hiệu MỚI: ID = ");
        Serial.println(current_sign_id);
        last_id = current_sign_id;
    }
    
    // =================== LOGIC TIMEOUT (BỘ NHỚ 2 GIÂY) ===================
    // Nếu đang giữ một biển báo, nhưng đã quá 2 giây (2000ms) mà ông S3 không gửi thêm tín hiệu nào
    if (current_sign_id != 0 && (millis() - last_receive_time > 2000)) {
        Serial.println("Quá 2 giây không thấy lại biển báo -> Trở về trạng thái Không Có Gì");
        current_sign_id = 0; // Reset
    }
    
    if (current_sign_id == 0) last_id = -1; // Reset biến log
    
    // =================== LOGIC CHỚP LED 500ms ===================
    static unsigned long previousMillis = 0;
    static bool ledState = false;
    unsigned long currentMillis = millis();
    
    // Tạo bộ đếm nhịp 500ms độc lập (không dùng delay để không block I2C và Web)
    if (currentMillis - previousMillis >= 500) {
        previousMillis = currentMillis;
        ledState = !ledState;
    }
    
    // Bật/Tắt LED theo trạng thái hiện tại
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
