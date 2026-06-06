#include <Arduino.h>

// --- CẤU HÌNH CHÂN NGOẠI VI ADC (ESP32) ---
const int PIN_HALL_X = 34; // Cảm biến trục X - Kênh ADC1_CH6 [cite: 191]
const int PIN_HALL_Y = 35; // Cảm biến trục Y - Kênh ADC1_CH7 [cite: 192]
const int PIN_HALL_Z = 32; // Cảm biến trục Z - Kênh ADC1_CH4 [cite: 193]

// --- THAM SỐ PHẦN CỨNG & CẢM BIẾN ---
const float V_REF = 3.3;          // Điện áp tham chiếu của ESP32 (Volt) [cite: 187, 197]
const int ADC_RESOLUTION = 4095;  // Độ phân giải lượng tử hóa 12-bit (0 - 4095) [cite: 15, 179]
const float SENSITIVITY = 1.4;    // Độ nhạy thực tế của cảm biến SS49E (1.4 mV/Gauss) [cite: 185]

// --- BIẾN LƯU TRỮ HIỆU CHUẨN ĐIỂM KHÔNG ---
float vOffset_X = 0.0;
float vOffset_Y = 0.0;
float vOffset_Z = 0.0;

// --- THAM SỐ BỘ LỌC THÔNG THẤP MŨ (EMA) ---
const float ALPHA = 0.20; // Hệ số lọc số tối ưu tại biên (0.15 - 0.25) [cite: 206]
float filtered_Bx = 0.0;
float filtered_By = 0.0;
float filtered_Bz = 0.0;

// --- QUẢN LÝ TẦN SỐ LẤY MẪU (50 Hz) ---
unsigned long lastSampleTime = 0;
const unsigned long SAMPLE_INTERVAL = 20; // Tần số 50Hz tương ứng chu kỳ 20ms [cite: 216, 230]
unsigned long sampleCounter = 0;          // Bộ đếm kiểm soát tiến độ thu thập dữ liệu
const unsigned long TARGET_SAMPLES = 50000; // Mục tiêu thu thập 50.000 điểm mẫu 

// Hàm tự động hiệu chuẩn điện áp lệch pha trong 5 giây đầu tiên khi khởi động [cite: 15, 198]
void runAutoCalibration() {
  Serial.println("\n--- [HỆ THỐNG] KHỞI ĐỘNG CHU KỲ TỰ HIỆU CHUẨN ĐIỂM KHÔNG (5 GIÂY) ---"); [cite: 198]
  long sumX = 0, sumY = 0, sumZ = 0;
  long count = 0;
  unsigned long startTime = millis();
  
  while (millis() - startTime < 5000) { [cite: 198]
    sumX += analogRead(PIN_HALL_X);
    sumY += analogRead(PIN_HALL_Y);
    sumZ += analogRead(PIN_HALL_Z);
    count++;
    delay(2); // Duy trì tính ổn định hệ thống
  }
  
  // Tính điện áp lệch pha trung bình thực tế (V_offset) [cite: 198, 199]
  vOffset_X = ((float)sumX / count) * (V_REF / ADC_RESOLUTION);
  vOffset_Y = ((float)sumY / count) * (V_REF / ADC_RESOLUTION);
  vOffset_Z = ((float)sumZ / count) * (V_REF / ADC_RESOLUTION);
  
  Serial.println("HIỆU CHUẨN HOÀN TẤT!");
  Serial.printf("Điện áp V_offset đo được X: %.4f V | Y: %.4f V | Z: %.4f V\n", vOffset_X, vOffset_Y, vOffset_Z);
  Serial.println("Bắt đầu kịch bản thu thập... Hãy di chuyển nam châm hoặc thay đổi tải dòng điện!\n");
}

void setup() {
  Serial.begin(115200);

  // Cấu hình bộ suy giảm điện áp ADC ở mức 11dB để mở rộng dải đo tuyến tính từ 0.1V - 3.1V [cite: 197]
  analogSetWidth(12); [cite: 179]
  analogSetAttenuation(ADC_ATTENUATION_DB_11); [cite: 197]

  pinMode(PIN_HALL_X, INPUT); [cite: 191]
  pinMode(PIN_HALL_Y, INPUT); [cite: 192]
  pinMode(PIN_HALL_Z, INPUT); [cite: 193]

  // Thực hiện chu kỳ hiệu chuẩn ban đầu [cite: 198]
  runAutoCalibration();
  
  lastSampleTime = millis();
}

void loop() {
  // Dừng thu thập nếu đã đạt đủ mục tiêu 50.000 điểm mẫu 
  if (sampleCounter >= TARGET_SAMPLES) {
    if (sampleCounter == TARGET_SAMPLES) {
      Serial.println("\n--- ĐÃ THU THẬP ĐỦ TẬP DỮ LIỆU 50.000 ĐIỂM MẪU THÀNH CÔNG! ---");
      sampleCounter++; // Tăng cấu trúc để tránh lặp lại thông báo này
    }
    return; 
  }

  unsigned long currentTime = millis();

  // Đảm bảo khoảng thời gian lấy mẫu đồng bộ tuyệt đối 20ms (Tần số 50Hz) [cite: 216, 230]
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = currentTime;

    // 1. LẤY MẪU ADC THÔ TỪ CẢM BIẾN [cite: 15, 179]
    int rawX = analogRead(PIN_HALL_X); [cite: 191]
    int rawY = analogRead(PIN_HALL_Y); [cite: 192]
    int rawZ = analogRead(PIN_HALL_Z); [cite: 193]

    // 2. CHUYỂN ĐỔI GIÁ TRỊ SANG ĐIỆN ÁP THỰC (VOLT)
    float vOut_X = ((float)rawX / ADC_RESOLUTION) * V_REF;
    float vOut_Y = ((float)rawY / ADC_RESOLUTION) * V_REF;
    float vOut_Z = ((float)rawZ / ADC_RESOLUTION) * V_REF;

    // 3. KHỬ ĐIỆN ÁP LỆCH PHA & QUY ĐỔI SANG ĐƠN VỊ GAUSS
    // Công thức: B = (V_out - V_offset) / SENSITIVITY. Do SENSITIVITY ở dạng mV/Gauss, ta nhân 1000.0 để chuẩn hóa sang Volt. [cite: 134]
    float raw_Bx = ((vOut_X - vOffset_X) / SENSITIVITY) * 1000.0; [cite: 134]
    float raw_By = ((vOut_Y - vOffset_Y) / SENSITIVITY) * 1000.0; [cite: 134]
    float raw_Bz = ((vOut_Z - vOffset_Z) / SENSITIVITY) * 1000.0; [cite: 134]

    // 4. THỰC HIỆN THUẬT TOÁN LỌC SỐ THÔNG THẤP MŨ (EMA) TẠI BIÊN [cite: 15, 202]
    // Công thức toán học: Y[t] = alpha * X[t] + (1 - alpha) * Y[t-1] [cite: 202]
    filtered_Bx = ALPHA * raw_Bx + (1.0 - ALPHA) * filtered_Bx; [cite: 202]
    filtered_By = ALPHA * raw_By + (1.0 - ALPHA) * filtered_By; [cite: 202]
    filtered_Bz = ALPHA * raw_Bz + (1.0 - ALPHA) * filtered_Bz; [cite: 202]

    // 5. ĐÓNG GÓI CHUỖI DỮ LIỆU ĐỊNH DẠNG JSON [cite: 16, 208]
    // Định dạng cấu trúc: {"ts": timestamp, "bx": Bx, "by": By, "bz": Bz} 
    String jsonString = "{";
    jsonString += "\"ts\":" + String(currentTime) + ","; [cite: 211]
    jsonString += "\"bx\":" + String(filtered_Bx, 2) + ","; [cite: 212]
    jsonString += "\"by\":" + String(filtered_By, 2) + ","; [cite: 213]
    jsonString += "\"bz\":" + String(filtered_Bz, 2); [cite: 214]
    jsonString += "}";

    // Xuất ra dữ liệu dạng Stream liên tục [cite: 216]
    Serial.println(jsonString);

    sampleCounter++; // Tăng số lượng mẫu đã thu thập
  }
}