# RetoCarModuleCamExpress - Hệ thống Nhận diện Biển báo Giao thông

Dự án này sử dụng ESP32-CAM để nhận diện biển báo giao thông (bằng AI Model từ Edge Impulse) và gửi tín hiệu điều khiển sang một mạch ESP32 Slave qua chuẩn giao tiếp I2C. ESP32 Slave sẽ hiển thị trạng thái lên giao diện Web và điều khiển LED.

Bên cạnh đó, dự án cũng cung cấp các công cụ Python để hỗ trợ quá trình thu thập dữ liệu (data collection) trong trường hợp bạn muốn tự train lại model AI.

---

## 1. Hướng dẫn sử dụng trực tiếp (Test hệ thống nhận diện)

Sử dụng để chạy thử mô hình nhận diện biển báo đã được train.

### A. Chuẩn bị phần cứng và Nạp code
1. **Mạch ESP32-CAM (Master - Xử lý AI):**
   - Mở thư mục `Esp32Cam` bằng PlatformIO (hoặc Arduino IDE với thư viện tương ứng).
   - Nạp code (`src/main.cpp`) vào ESP32-CAM.
   - *Lưu ý:* Code này đã tích hợp sẵn thư viện Edge Impulse (`nhan_dien_bb_inferencing.h`) để phân tích hình ảnh và nhận diện "rẽ trái", "rẽ phải".
2. **Mạch ESP32 Dev / ESP32-S3 (Slave - Web UI & LED):**
   - Mở thư mục `Esp32Dev` và nạp file `esp32_slave_web.ino`.


### B. Theo dõi và Test
1. Sau khi cấp nguồn, ESP32 Slave sẽ phát ra một mạng WiFi có tên là **`Esp32Cam`** (không mật khẩu).
2. Dùng điện thoại kết nối vào WiFi **`Esp32Cam`**.
3. Mở trình duyệt web trên điện thoại, truy cập vào IP của mạch Slave (thường là `192.168.4.1`).
4. Giao diện Web "Mắt Thần Robot" sẽ hiển thị trực tiếp. Khi đưa biển báo (rẽ trái/phải) vào trước ESP32-CAM, trên Web sẽ cập nhật trạng thái ngay lập tức và đèn LED tương ứng sẽ chớp nháy.

---

## 2. Hướng dẫn thu thập dữ liệu (Nếu muốn Train lại Model)

Nếu bạn muốn tạo dataset mới để train lại model nhận diện biển báo, bạn có thể dùng điện thoại làm màn hình và điều khiển để thu thập ảnh từ ESP32-CAM.

### A. Chuẩn bị ESP32-CAM để chụp ảnh
Để chụp ảnh, bạn **không thể** dùng file `main.cpp` nhận diện AI hiện tại. Bạn cần nạp một đoạn code **CameraWebServer** cơ bản vào ESP32-CAM (code phát WiFi và có đường dẫn `http://<IP_ESP32>/capture` để lấy ảnh). Khi ESP32-CAM đã kết nối WiFi và có IP, hãy ghi lại địa chỉ IP này.

### B. Sử dụng Tool Python (trên Máy tính)
Đảm bảo máy tính và điện thoại của bạn đang kết nối **cùng một mạng WiFi**.

Trong thư mục `CaptureDataExample` có 2 công cụ:

#### Cách 1: Chụp qua giao diện Web bằng Điện thoại (Khuyên dùng)
Tool này sẽ tạo ra một server trên máy tính. Bạn dùng điện thoại truy cập vào server này để xem camera và ấn nút phân loại để chụp. Ảnh sẽ được tự động lưu về máy tính và chia sẵn thư mục theo nhãn (Label).

1. Mở terminal, đi tới thư mục `CaptureDataExample`.
2. Cài đặt các thư viện cần thiết:
   ```bash
   pip install requests flask
   ```
3. Chạy server:
   ```bash
   python data_collector.py
   ```
4. Terminal sẽ in ra địa chỉ IP của máy tính (Ví dụ: `http://192.168.1.50:5000`).
5. Lấy điện thoại truy cập vào địa chỉ trên.
6. Điền IP của ESP32-CAM vào ô nhập IP trên giao diện điện thoại. Bạn sẽ thấy video stream hiện lên.
7. Đưa camera qua các góc độ của biển báo và bấm các nút tương ứng trên màn hình điện thoại (Bắt buộc rẽ trái, Cấm rẽ phải...). Ảnh sẽ tự động được chụp và lưu vào thư mục `dataset/` trên máy tính.



### C. Train Model
Sau khi thu thập đủ hình ảnh, bạn nén thư mục dataset lại và tải lên [Edge Impulse](https://edgeimpulse.com/) (hoặc Google Colab) để huấn luyện mô hình Object Detection (FOMO). Khi hoàn tất, export model dưới dạng thư viện Arduino (C++), giải nén và ghi đè vào thư mục `Esp32Cam` để sử dụng.
