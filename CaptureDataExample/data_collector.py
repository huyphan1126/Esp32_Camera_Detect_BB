import os
import time
import requests
from flask import Flask, render_template_string, request, jsonify

app = Flask(__name__)

# Cấu hình IP của Camera ESP32 (Sẽ điền trên giao diện Web)
ESP32_IP = "192.168.1.xxx"

# Cấu hình 6 loại nhãn cho biển báo
LABELS = {
    "bat_buoc_re_trai": "Bắt buộc rẽ Trái",
    "bat_buoc_re_phai": "Bắt buộc rẽ Phải",
    "bat_buoc_di_thang": "Bắt buộc đi Thẳng",
    "cam_re_trai": "Cấm rẽ Trái",
    "cam_re_phai": "Cấm rẽ Phải",
    "cam_di_thang": "Cấm đi Thẳng"
}

# Tạo thư mục lưu dataset
DATASET_DIR = "dataset"
if not os.path.exists(DATASET_DIR):
    os.makedirs(DATASET_DIR)
for label in LABELS.keys():
    label_dir = os.path.join(DATASET_DIR, label)
    if not os.path.exists(label_dir):
        os.makedirs(label_dir)

# HTML Giao diện Web (Mở trên điện thoại)
HTML_UI = """
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Thu thập Biển báo</title>
    <style>
        body { font-family: Arial, sans-serif; text-align: center; background: #1a1a2e; color: white; margin: 0; padding: 10px; }
        .ip-box { margin-bottom: 10px; }
        input[type="text"] { padding: 10px; width: 200px; font-size: 16px; border-radius: 5px; border: none; text-align: center; }
        .camera-box { position: relative; display: inline-block; background: #000; border: 3px solid #e94560; border-radius: 10px; overflow: hidden; width: 100%; max-width: 400px; height: auto; aspect-ratio: 4/3; }
        img { width: 100%; height: 100%; object-fit: cover; }
        .btn-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 15px; max-width: 400px; margin-left: auto; margin-right: auto; }
        .btn { padding: 15px 5px; font-size: 16px; font-weight: bold; color: white; border: none; border-radius: 8px; cursor: pointer; text-transform: uppercase; }
        .btn:active { transform: scale(0.95); opacity: 0.8; }
        
        /* Màu cho từng nhóm biển báo */
        .btn-bat-buoc { background-color: #3498db; }
        .btn-cam { background-color: #e74c3c; }

        .toast { position: fixed; top: 10px; left: 50%; transform: translateX(-50%); background: #2ecc71; color: white; padding: 10px 20px; border-radius: 5px; display: none; font-weight: bold; z-index: 1000; }
    </style>
</head>
<body>
    <div class="toast" id="toast">Đã chụp!</div>

    <h3>Trạm Thu Thập Data</h3>
    <div class="ip-box">
        <input type="text" id="esp_ip" value="{{ default_ip }}" placeholder="Nhập IP ESP32...">
    </div>

    <div class="camera-box">
        <img id="stream" src="" alt="Video Stream">
    </div>

    <div class="btn-grid">
        {% for key, name in labels.items() %}
            <button class="btn {% if 'cam_' in key %}btn-cam{% else %}btn-bat-buoc{% endif %}" onclick="capture('{{ key }}')">
                {{ name }}
            </button>
        {% endfor %}
    </div>

    <script>
        const stream = document.getElementById('stream');
        const ipInput = document.getElementById('esp_ip');
        const toast = document.getElementById('toast');

        let isCapturing = false;

        // Vòng lặp tải ảnh từ ESP32 để làm video mượt mà
        function updateFrame() {
            if (isCapturing) return; // Dừng stream nếu đang chụp ảnh
            
            let ip = ipInput.value.trim();
            if (ip) {
                stream.src = "http://" + ip + "/capture?t=" + Date.now();
                // Giảm tốc độ lấy ảnh xuống 2-3 hình/giây để ESP32 không bị nghẽn mạng
                stream.onload = () => setTimeout(updateFrame, 400);
                stream.onerror = () => setTimeout(updateFrame, 1000);
            } else {
                setTimeout(updateFrame, 1000);
            }
        }
        updateFrame();

        // Hàm gọi lên Python Server để lưu ảnh
        function capture(label) {
            let ip = ipInput.value.trim();
            if (!ip) { alert("Vui lòng nhập IP của ESP32 trước!"); return; }

            // Hiển thị thông báo và TẠM DỪNG STREAM
            isCapturing = true; 
            toast.innerText = "Đang chụp: " + label + "...";
            toast.style.background = "#e67e22";
            toast.style.display = "block";

            // Gửi lệnh qua Python
            fetch("/save", {
                method: "POST",
                headers: { "Content-Type": "application/json" },
                body: JSON.stringify({ label: label, esp_ip: ip })
            }).then(res => res.json()).then(data => {
                // Chụp xong -> Tiếp tục stream
                toast.innerText = "Đã lưu thành công!";
                toast.style.background = "#2ecc71";
                setTimeout(() => { 
                    toast.style.display = "none"; 
                    isCapturing = false;
                    updateFrame(); // Gọi stream chạy lại
                }, 800);
            }).catch(err => {
                toast.innerText = "Lỗi kết nối!";
                toast.style.background = "#e74c3c";
                setTimeout(() => { toast.style.display = "none"; isCapturing = false; updateFrame(); }, 1500);
            });
        }
    </script>
</body>
</html>
"""

@app.route("/")
def index():
    return render_template_string(HTML_UI, default_ip=ESP32_IP, labels=LABELS)

@app.route("/save", methods=["POST"])
def save_image():
    data = request.json
    label = data.get("label")
    esp_ip = data.get("esp_ip")

    if not label or not esp_ip:
        return jsonify({"status": "error", "message": "Thiếu tham số!"})

    # Lấy ảnh chất lượng cao từ ESP32
    url = f"http://{esp_ip}/capture"
    try:
        response = requests.get(url, timeout=3)
        if response.status_code == 200:
            # Tạo tên file: label_timestamp.jpg
            filename = f"{label}_{int(time.time()*1000)}.jpg"
            filepath = os.path.join(DATASET_DIR, label, filename)
            
            with open(filepath, "wb") as f:
                f.write(response.content)
            
            print(f"[OK] Đã lưu: {filepath}")
            return jsonify({"status": "success", "file": filename})
        else:
            return jsonify({"status": "error", "message": "Lỗi camera"})
    except Exception as e:
        print(f"[LỖI] Không thể kết nối tới ESP32: {e}")
        return jsonify({"status": "error", "message": str(e)})

if __name__ == "__main__":
    print("\n=======================================================")
    print("🚀 MÁY CHỦ THU THẬP DỮ LIỆU ĐÃ CHẠY!")
    print("1. Hãy đảm bảo Máy tính và Điện thoại kết nối chung WiFi.")
    print("2. Mở trình duyệt trên Điện thoại và truy cập vào IP của MÁY TÍNH.")
    print("   (Thường là http://<IP_Máy_Tính>:5000)")
    print("=======================================================\n")
    app.run(host="0.0.0.0", port=5000)
