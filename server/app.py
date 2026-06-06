import os
import numpy as np
from flask import Flask, request, jsonify
from tensorflow.keras.models import load_model

app = Flask(__name__)

# --- CẤU HÌNH ĐƯỜNG DẪN MÔ HÌNH ---
# Định vị file mô hình .h5 nằm trong thư mục model/
MODEL_PATH = os.path.join(os.path.dirname(__file__), "../model/lstm_hall_sensor_model.h5")

# Khởi tạo biến toàn cục chứa mô hình
lstm_model = None

def get_model():
    """Hàm tải mô hình (Lazy Loading) để tránh nghẽn khi khởi động"""
    global lstm_model
    if lstm_model is None:
        if os.path.exists(MODEL_PATH):
            print(f"[SERVER] Đang nạp mô hình mạng học sâu từ: {MODEL_PATH} ...")
            lstm_model = load_model(MODEL_PATH)
            print("[SERVER] Nạp mô hình LSTM thành công!")
        else:
            print(f"[CẢNH BÁO] Không tìm thấy file mô hình tại {MODEL_PATH}.")
            print("[HƯỚNG DẪN] Bạn cần chạy file 'train_lstm.py' trước để sinh ra file .h5")
    return lstm_model

# --- BỘ ĐỆM DỮ LIỆU ĐỂ TẠO CỬA SỔ TRƯỢT (SLIDING WINDOW) ---
# Mạng LSTM yêu cầu dữ liệu quá khứ gồm TIME_STEPS bước (trong báo cáo cấu hình là 10 bước)
TIME_STEPS = 10
data_buffer = []

# --- CÁC THAM SỐ CHUẨN HÓA (MIN-MAX SCALER) ---
# Khi train model, ta đã chuẩn hóa dữ liệu về khoảng [0, 1]. 
# Để dự báo chính xác, dữ liệu từ ESP32 lên cũng phải ép về [0, 1] theo đúng Min/Max lúc huấn luyện.
# Dưới đây là giá trị giả lập mẫu (bạn có thể thay bằng Min/Max thực tế từ Dataset của bạn)
MIN_VALUES = np.array([-300.0, -300.0, -300.0]) # Giả định biên dưới của từ trường (Gauss)
MAX_VALUES = np.array([300.0, 300.0, 300.0])    # Giả định biên trên của từ trường (Gauss)

def scale_features(raw_sample):
    """Chuẩn hóa dữ liệu thô về khoảng [0, 1] (MinMaxScaler)"""
    return (np.array(raw_sample) - MIN_VALUES) / (MAX_VALUES - MIN_VALUES)

def inverse_scale_features(scaled_prediction):
    """Khôi phục dữ liệu đã dự báo từ khoảng [0, 1] về đơn vị Gauss ban đầu"""
    return scaled_prediction * (MAX_VALUES - MIN_VALUES) + MIN_VALUES


# --- API TIẾP NHẬN DỮ LIỆU VÀ DỰ BÁO ---
@app.route('/api/predict', methods=['POST'])
def predict_magnetic_field():
    global data_buffer
    
    # 1. Trích xuất dữ liệu JSON gửi từ cổng mạng của ESP32
    json_data = request.get_json()
    if not json_data:
        return jsonify({"error": "Không nhận được dữ liệu hoặc định dạng không phải JSON"}), 400
    
    try:
        # Đọc các giá trị vector thành phần của từ trường 3 trục
        bx = float(json_data.get('bx'))
        by = float(json_data.get('by'))
        bz = float(json_data.get('bz'))
        ts = json_data.get('ts') # Mã thời gian (millis)
        
        current_sample = [bx, by, bz]
        
        # 2. Thực hiện chuẩn hóa điểm mẫu hiện tại
        scaled_sample = scale_features(current_sample)
        data_buffer.append(scaled_sample)
        
        # 3. Duy trì cửa sổ trượt có kích thước đúng bằng 10 mẫu gần nhất
        if len(data_buffer) > TIME_STEPS:
            data_buffer.pop(0) # Loại bỏ mẫu cũ nhất khi vượt quá 10
            
        # 4. Kiểm tra điều kiện bộ đệm để tiến hành dự báo bằng LSTM
        if len(data_buffer) < TIME_STEPS:
            # Nếu chưa tích lũy đủ 10 mẫu (ở những giây đầu tiên hệ thống chạy)
            return jsonify({
                "status": "buffering",
                "message": f"Đang tích lũy chuỗi thời gian. Cần thêm {TIME_STEPS - len(data_buffer)} mẫu.",
                "current_length": len(data_buffer)
            }), 200
            
        # 5. Đưa dữ liệu qua mô hình AI khi bộ đệm đã đủ 10 mẫu
        model = get_model()
        if model is None:
            return jsonify({"error": "Mô hình LSTM chưa được nạp trên máy chủ"}), 500
            
        # Chuyển đổi định dạng cấu trúc mảng thành 3 chiều: (Samples=1, Time_Steps=10, Features=3)
        input_window = np.array([data_buffer])
        
        # Thực hiện dự báo bước kế tiếp (kết quả trả về ở dạng chuẩn hóa [0,1])
        scaled_prediction = model.predict(input_window, verbose=0)
        
        # Giải chuẩn hóa kết quả đầu ra về đơn vị vật lý Gauss thực tế
        prediction_gauss = inverse_scale_features(scaled_prediction[0])
        
        # 6. Phản hồi kết quả dự báo về phía Client/ESP32 hoặc Dashboard hiển thị
        response_payload = {
            "status": "success",
            "received_timestamp": ts,
            "input_current_state": {"bx": bx, "by": by, "bz": bz},
            "predicted_next_state": {
                "bx": round(float(prediction_gauss[0]), 2),
                "by": round(float(prediction_gauss[1]), 2),
                "bz": round(float(prediction_gauss[2]), 2)
            }
        }
        return jsonify(response_payload), 200

    except (ValueError, TypeError) as e:
        return jsonify({"error": f"Dữ liệu sai kiểu hoặc thiếu trường: {str(e)}"}), 400
    except Exception as e:
        return jsonify({"error": f"Lỗi hệ thống máy chủ: {str(e)}"}), 500


# --- ROUTE KIỂM TRA TRẠNG THÁI SERVER ---
@app.route('/api/health', methods=['GET'])
def health_check():
    model_status = "Đã sẵn sàng" if get_model() is not None else "Chưa tìm thấy file .h5"
    return jsonify({
        "server_status": "Chạy ổn định",
        "model_status": model_status,
        "config": {
            "time_steps": TIME_STEPS,
            "features": ["bx", "by", "bz"]
        }
    }), 200

if __name__ == '__main__':
    # Chạy Flask Server lắng nghe ở cổng 5000, host '0.0.0.0' cho phép các thiết bị ngoại vi cùng lớp mạng WiFi (như ESP32) truy cập vào máy tính.
    app.run(host='0.0.0.0', port=5000, debug=True)