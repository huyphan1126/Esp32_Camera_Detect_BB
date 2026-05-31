#include <Arduino.h>
#include <WiFi.h>
#include "esp_camera.h"
#include <Wire.h>

// THƯ VIỆN AI EDGE IMPULSE BIỂN BÁO
#include <nhan_dien_bb_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"

// =================== PINOUT ESP32-S3 ===================
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// =================== I2C ===================
#define I2C_SDA 2
#define I2C_SCL 1
#define SLAVE_ADDR 8

static uint8_t *snapshot_buf = nullptr;

void setup() {
    Serial.begin(115200);

    // I2C Master Init
    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("I2C Master đã khởi tạo.");

    // Camera Init
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_QVGA; 
    config.jpeg_quality = 12;
    config.fb_count = 1;

    if (psramFound()) {
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_LATEST;
    }

    if (esp_camera_init(&config) != ESP_OK) {
        Serial.println("Lỗi khởi tạo camera!");
        return;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_hmirror(s, 0); 
        s->set_saturation(s, 2); 
        s->set_contrast(s, 1);   
    }

    if (psramFound()) {
        snapshot_buf = (uint8_t*)ps_malloc(320 * 240 * 3);
    } else {
        snapshot_buf = (uint8_t*)malloc(320 * 240 * 3);
    }
    if (!snapshot_buf) {
        Serial.println("Không đủ bộ nhớ để cấp phát buffer ảnh");
        return;
    }

    Serial.println("--Bắt đầu detect biển báo--");
}

void loop() {
    // 1. Chụp ảnh
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Lỗi chụp ảnh");
        return;
    }

    // 2. Chuyển đổi màu JPEG sang RGB888
    bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);
    esp_camera_fb_return(fb);

    if (!converted) {
        Serial.println("Lỗi chuyển đổi màu");
        return;
    }

    // 3. Cắt và thu nhỏ ảnh về kích thước 160x160 
    ei::image::processing::crop_and_interpolate_rgb888(
        snapshot_buf, 320, 240, 
        snapshot_buf, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);

    // 4. Chạy AI FOMO Model để nhận diện biển báo
    ei_impulse_result_t result = { 0 };
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = [](size_t offset, size_t length, float *out_ptr) -> int {
        size_t pixel_ix = offset * 3;
        for (size_t i = 0; i < length; i++) {
            // SỬA LỖI MÙ MÀU: Edge Impulse mong đợi kênh R nằm ở byte thứ 3 (pixel_ix + 2)
            out_ptr[i] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix + 0];
            pixel_ix += 3;
        }
        return 0;
    };

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        Serial.printf("LỖI NGHIÊM TRỌNG: AI chạy thất bại (Mã lỗi: %d)\n", res);
        delay(1000);
        return;
    }

    // 5. Tìm biển báo có độ tin cậy cao nhất
    int best_id = 0; // 0 = Không thấy gì
    float max_value = 0.0f;

    for (size_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) continue;

        if (bb.value > 0.50f && bb.value > max_value) {
            max_value = bb.value;
            String label = String(bb.label);
            label.toLowerCase(); 
            
            if (label.indexOf("trai") >= 0) {
                best_id = 1;
            } else if (label.indexOf("phai") >= 0) {
                best_id = 2;
            }
        }
    }

    bool found_anything = false;
    for (size_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value > 0.30f) {
            Serial.printf(" '%s' (Chắc chắn: %d%%) tại X:%d Y:%d\n", bb.label, (int)(bb.value * 100), bb.x, bb.y);
            found_anything = true;
        }
    }
    
    if (!found_anything) {
        Serial.println("--- Đang quét... Không thấy biển báo nào ---");
    }

    // 6. Gửi ID qua I2C sang ESP32 Dev
    // CHỈ GỬI KHI THẤY BIỂN BÁO. Nếu không detect được thì để bên kia giữ nguyên trạng thái cũ
    if (best_id != 0) {
        Wire.beginTransmission(SLAVE_ADDR);
        Wire.write(best_id);
        uint8_t error = Wire.endTransmission();
        
        if (error != 0) {
            Serial.printf("Lỗi truyền I2C (Mã lỗi: %d) - Kiểm tra lại dây SDA, SCL, GND\n", error);
        }
    }
}