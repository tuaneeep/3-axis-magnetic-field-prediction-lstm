import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from sklearn.preprocessing import MinMaxScaler
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import LSTM, Dense, Dropout
from tensorflow.keras.callbacks import EarlyStopping

# 1. ĐỌC DỮ LIỆU TỪ FILE CSV
df = pd.read_csv('hall_sensor_dataset.csv')

# Chỉ lấy 3 cột tính năng từ trường: Bx, By, Bz (Bỏ qua cột ts)
data = df[['bx', 'by', 'bz']].values

# 2. CHUẨN HÓA DỮ LIỆU VỀ KHOẢNG [0, 1]
scaler = MinMaxScaler(feature_range=(0, 1))
data_scaled = scaler.fit_transform(data)

# 3. TẠO CẤU TRÚC DỮ LIỆU CHUỖI THỜI GIAN (SLIDING WINDOW)
# Định nghĩa: Dùng TIME_STEPS mẫu quá khứ để dự báo 1 mẫu tương lai
TIME_STEPS = 10 
X, y = [], []

for i in range(TIME_STEPS, len(data_scaled)):
    X.append(data_scaled[i-TIME_STEPS:i, :]) # 10 bước quá khứ (t-10 đến t-1)
    y.append(data_scaled[i, :])               # 1 bước hiện tại/tương lai (t)

X, y = np.array(X), np.array(y)
print(f"Kích thước tập dữ liệu đầu vào LSTM (X): {X.shape}") # (Samples, Time Steps, Features)
print(f"Kích thước tập nhãn dự báo (y): {y.shape}")         # (Samples, Features)

# 4. CHIA TẬP DỮ LIỆU: 80% HUẤN LUYỆN (TRAIN), 20% KIỂM THỬ (TEST)
train_size = int(len(X) * 0.8)
X_train, X_test = X[:train_size], X[train_size:]
y_train, y_test = y[:train_size], y[train_size:]

# 5. XÂY DỰNG KIẾN TRÚC MẠNG LSTM
model = Sequential([
    # Lớp LSTM thứ nhất
    LSTM(units=50, return_sequences=True, input_shape=(X_train.shape[1], X_train.shape[2])),
    Dropout(0.2), # Chống quá khớp (Overfitting)
    
    # Lớp LSTM thứ hai
    LSTM(units=50, return_sequences=False),
    Dropout(0.2),
    
    # Lớp đầu ra (Dense Layer) dự báo đồng thời 3 trục [Bx, By, Bz]
    Dense(units=3)
])

# Biên dịch mô hình với bộ tối ưu Adam và hàm mất mát MSE
model.compile(optimizer='adam', loss='mean_squared_error')
model.summary()

# 6. HUẤN LUYỆN MÔ HÌNH
early_stop = EarlyStopping(monitor='val_loss', patience=5, restore_best_weights=True)

print("\n--- BẮT ĐẦU HUẤN LUYỆN MÔ HÌNH LSTM ---")
history = model.fit(
    X_train, y_train,
    epochs=20,             
    batch_size=64,         
    validation_data=(X_test, y_test),
    callbacks=[early_stop],
    verbose=1
)

# 7. ĐÁNH GIÁ MÔ HÌNH TRÊN TẬP KIỂM THỬ
print("\n--- ĐÁNH GIÁ VÀ ĐỒ THỊ ---")
predictions_scaled = model.predict(X_test)

predictions = scaler.inverse_transform(predictions_scaled)
y_test_original = scaler.inverse_transform(y_test)

plt.figure(figsize=(12, 6))
plt.plot(y_test_original[:200, 0], label='Giá trị Thực tế (True Bx)', color='blue')
plt.plot(predictions[:200, 0], label='Giá trị Dự báo (Predicted Bx)', color='orange', linestyle='--')
plt.title('So sánh Cường độ từ trường trục X: Thực tế vs Dự báo bởi LSTM')
plt.xlabel('Điểm mẫu (Mỗi bước 20ms)')
plt.ylabel('Cường độ từ trường (Gauss)')
plt.legend()
plt.grid(True)
plt.show()

# 8. LƯU MÔ HÌNH ĐÃ TRAIN THÀNH FILE
model.save('lstm_hall_sensor_model.h5')
print("Đã lưu mô hình thành file: lstm_hall_sensor_model.h5")