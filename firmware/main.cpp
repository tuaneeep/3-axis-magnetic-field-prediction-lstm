#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --- THÔNG TIN CẤU HÌNH WIFI ---
const char* ssid = "TEN_WIFI_NHA_BAN";       // Thay bằng tên WiFi nhà bạn
const char* password = "MAT_KHAU_WIFI";     // Thay bằng mật khẩu WiFi

// --- ĐỊA CHỈ IP CỦA SERVER PYTHON (HTTP) ---
// Thay "192.168.1.X" thành địa chỉ IP thực tế của máy tính chạy server/app.py
const char* serverUrl = "http://192.168.1.X:5000/api/predict"; 

// --- CẤU HÌNH CHÂN NGOẠI VI ADC ---
const int PIN_HALL_X = 34; 
const int PIN_HALL_Y = 35; 
const int PIN_HALL_Z = 32; 

// --- THAM SỐ ĐIỆN ÁP & CẢM BIẾN ---
const float V_REF = 3.3;          
const int ADC_RESOLUTION = 4095;  
const float SENSITIVITY = 1.4;    // mV/Gauss

// --- BIẾN HIỆU CHUẨN ĐIỂM KHÔNG ---
float vOffset_X = 0.0;
float vOffset_Y = 0.0;
float vOffset_Z = 0.0;

// --- THAM SỐ BỘ LỌC THÔNG THẤP MŨ (EMA) ---
const float ALPHA = 0.20; 
float filtered_Bx = 0.0;  
float filtered_By = 0.0;  
float filtered_Bz = 0.0;  

// --- QUẢN LÝ THỜI GIAN LẤY MẪU ---
const unsigned long SAMPLE_INTERVAL = 20; // 20ms tương ứng 50Hz

// --- CẤU TRÚC DỮ LIỆU & HÀNG ĐỢI FREERTOS ---
struct MagneticData {
  unsigned long timestamp;
  float bx;
  float by;
  float bz;
};

// Khởi tạo hàng đợi để chuyển tiếp dữ liệu giữa 2 Core của ESP32 mà không gây nghẽn
QueueHandle_t dataQueue;

// Khai báo các Task đa nhiệm
void TaskSensorRead(void *pvParameters);
void TaskNetworkSend(void *pvParameters);

// Hàm tự động hiệu chuẩn điểm không (giữ nguyên từ bản cũ)
void autoCalibration() {
  Serial.println("\n====== BẮT ĐẦU TỰ HIỆU CHUẨN ĐIỂM KHÔNG (5 GIÂY) ======");
  long sumX = 0, sumY = 0, sumZ = 0;
  long sampleCount = 0;
  unsigned long startTime = millis();
  
  while (millis() - startTime < 5000) {
    sumX += analogRead(PIN_HALL_X);
    sumY += analogRead(PIN_HALL_Y);
    sumZ += analogRead(PIN_HALL_Z);
    sampleCount++;
    delay(2); 
  }
  
  vOffset_X = ((float)sumX / sampleCount) * (V_REF / ADC_RESOLUTION);
  vOffset_Y = ((float)sumY / sampleCount) * (V_REF / ADC_RESOLUTION);
  vOffset_Z = ((float)sumZ / sampleCount) * (V_REF / ADC_RESOLUTION);
  Serial.println("====== HIỆU CHUẨN HOÀN TẤT ======");
}

void setup() {
  Serial.begin(115200);

  // Cấu hình ADC
  analogSetWidth(12);
  analogSetAttenuation(ADC_ATTENUATION_DB_11);

  pinMode(PIN_HALL_X, INPUT);
  pinMode(PIN_HALL_Y, INPUT);
  pinMode(PIN_HALL_Z, INPUT);

  // Kết nối WiFi ban đầu
  Serial.print("Đang kết nối WiFi: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Chạy quy trình hiệu chuẩn
  autoCalibration();

  // Khởi tạo hàng đợi chứa tối đa 50 phần tử dữ liệu từ trường
  dataQueue = xQueueCreate(50, sizeof(MagneticData));

  if (dataQueue != NULL) {
    // Tạo tác vụ Đọc cảm biến chạy trên Core 1 (Ưu tiên cao hơn để không lệch tần số 50Hz)
    xTaskCreatePinnedToCore(TaskSensorRead, "SensorReadTask", 4096, NULL, 2, NULL, 1);
    
    // Tạo tác vụ Gửi dữ liệu mạng chạy trên Core 0 (Ưu tiên thấp hơn, xử lý tác vụ truyền thông)
    xTaskCreatePinnedToCore(TaskNetworkSend, "NetworkSendTask", 8192, NULL, 1, NULL, 0);
  } else {
    Serial.println("Lỗi khởi tạo Hàng đợi (Queue Failed)!");
  }
}

void loop() {
  // Hàm loop để trống hoàn toàn vì chúng ta đã chuyển sang cơ chế đa nhiệm FreeRTOS
  vTaskDelete(NULL);
}

// ==================== CHI TIẾT CÁC TÁC VỤ ĐA NHIỆM ====================

// TASK 1: Đọc cảm biến, Khử điểm không, Lọc EMA (Chạy độc lập trên Core 1)
void TaskSensorRead(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(SAMPLE_INTERVAL); // Đổi 20ms sang đơn vị Tick hệ thống

  for (;;) {
    // Đảm bảo Task này thức dậy chính xác chu kỳ 20ms một lần (Đúng chuẩn 50Hz thực nghiệm)
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

    int rawX = analogRead(PIN_HALL_X);
    int rawY = analogRead(PIN_HALL_Y);
    int rawZ = analogRead(PIN_HALL_Z);

    float vOut_X = ((float)rawX / ADC_RESOLUTION) * V_REF;
    float vOut_Y = ((float)rawY / ADC_RESOLUTION) * V_REF;
    float vOut_Z = ((float)rawZ / ADC_RESOLUTION) * V_REF;

    float raw_Bx = ((vOut_X - vOffset_X) / SENSITIVITY) * 1000.0;
    float raw_By = ((vOut_Y - vOffset_Y) / SENSITIVITY) * 1000.0;
    float raw_Bz = ((vOut_Z - vOffset_Z) / SENSITIVITY) * 1000.0;

    // Áp dụng bộ lọc thông thấp số mũ EMA tại biên
    filtered_Bx = ALPHA * raw_Bx + (1.0 - ALPHA) * filtered_Bx;
    filtered_By = ALPHA * raw_By + (1.0 - ALPHA) * filtered_By;
    filtered_Bz = ALPHA * raw_Bz + (1.0 - ALPHA) * filtered_Bz;

    // Tạo struct dữ liệu tạm thời
    MagneticData dataToSend;
    dataToSend.timestamp = millis();
    dataToSend.bx = filtered_Bx;
    dataToSend.by = filtered_By;
    dataToSend.bz = filtered_Bz;

    // Đẩy dữ liệu vào hàng đợi (Nếu hàng đợi đầy, bỏ qua mẫu cũ để không nghẽn)
    xQueueSend(dataQueue, &dataToSend, 0);
  }
}

// TASK 2: Đóng gói JSON và phát HTTP POST không dây lên Server (Chạy độc lập trên Core 0)
void TaskNetworkSend(void *pvParameters) {
  MagneticData receivedData;

  for (;;) {
    // Đợi cho đến khi có dữ liệu mới xuất hiện trong hàng đợi từ Task 1 truyền sang
    if (xQueueReceive(dataQueue, &receivedData, portMAX_DELAY) == pdPASS) {
      
      // Kiểm tra kết nối WiFi trước khi bắn HTTP
      if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");

        // Đóng gói sang định dạng chuỗi JSON đồng bộ chuẩn
        String jsonPayload = "{";
        jsonPayload += "\"ts\":" + String(receivedData.timestamp) + ",";
        jsonPayload += "\"bx\":" + String(receivedData.bx, 1) + ",";
        jsonPayload += "\"by\":" + String(receivedData.by, 1) + ",";
        jsonPayload += "\"bz\":" + String(receivedData.bz, 1);
        jsonPayload += "}";

        // Thực hiện gửi HTTP POST
        int httpResponseCode = http.POST(jsonPayload);

        if (httpResponseCode > 0) {
          String response = http.getString();
          Serial.print("[HTTP] Thành công! Dự báo từ Server: ");
          Serial.println(response);
        } else {
          Serial.print("[HTTP] Lỗi kết nối gửi dữ liệu: ");
          Serial.println(httpResponseCode);
        }
        http.end();
      } else {
        Serial.println("[WIFI] Mất kết nối! Đang tự động kết nối lại...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Đợi 2 giây trước khi thử lại mạng
      }
    }
  }
}
