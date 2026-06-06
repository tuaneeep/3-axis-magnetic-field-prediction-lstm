# 3-Axis Magnetic Field Dynamics Prediction using LSTM

## Overview

This project presents an end-to-end sensing and prediction system for real-time magnetic field monitoring and forecasting. A custom 3-axis Hall sensor platform acquires magnetic field measurements along the X, Y, and Z axes, while a Long Short-Term Memory (LSTM) neural network predicts future magnetic field states from historical sensor observations.

The system combines embedded sensing, signal processing, wireless communication, and deep learning to demonstrate how time-series forecasting can be applied to physical sensor data.

---

## Features

* Real-time 3-axis magnetic field measurement
* Embedded data acquisition using ESP32
* Edge-side signal filtering and calibration
* Wireless telemetry transmission
* Time-series forecasting using LSTM networks
* Real-time prediction pipeline
* Experimental data logging and visualization

---

## System Architecture

```text
Hall Sensors (X, Y, Z)
            │
            ▼
      ESP32 Controller
            │
            ▼
 Signal Processing Layer
 (Calibration + EMA Filter)
            │
            ▼
      Data Collection
            │
            ▼
      Time-Series Dataset
            │
            ▼
        LSTM Network
            │
            ▼
 Future Magnetic State Prediction
```

---

## Hardware Components

### Embedded Platform

* ESP32 Development Board

### Sensors

* Three Linear Hall-Effect Sensors (SS49E)
* Orthogonal arrangement for 3-axis measurement

### Power Supply

* 5V DC Supply

---

## Magnetic Field Measurement

The magnetic field components are measured independently:

[
B_x,\quad B_y,\quad B_z
]

The total magnetic field magnitude is calculated as:

[
B = \sqrt{B_x^2 + B_y^2 + B_z^2}
]

The sensing platform provides continuous magnetic field monitoring and generates time-series data for machine learning applications.

---

## Signal Processing

### Offset Calibration

An automatic startup calibration procedure removes sensor offsets and ADC bias errors.

### Exponential Moving Average Filter

To suppress environmental noise and measurement fluctuations, an Exponential Moving Average (EMA) filter is applied:

[
y_t=\alpha x_t + (1-\alpha)y_{t-1}
]

where:

* (x_t) is the current measurement
* (y_t) is the filtered output
* (\alpha) is the smoothing factor

---

## Data Acquisition Pipeline

1. Read Hall sensor voltages
2. Convert voltages into magnetic field values
3. Apply offset compensation
4. Apply EMA filtering
5. Transmit data wirelessly
6. Store measurements in CSV format
7. Prepare time-series sequences for LSTM training

---

## LSTM-Based Prediction Model

### Objective

Predict future magnetic field states using historical measurements.

Input sequence:

[
[B_x,B_y,B_z]_{t-n:t}
]

Output:

[
[B_x,B_y,B_z]_{t+1}
]

### Network Architecture

* Input Layer
* LSTM Layer 1
* Dropout Layer
* LSTM Layer 2
* Dense Output Layer

### Prediction Targets

* Future (B_x)
* Future (B_y)
* Future (B_z)

---

## Repository Structure

```text
3-axis-magnetic-field-prediction-lstm
│
├── README.md
├── requirements.txt
│
├── firmware/
│   └── main.cpp
│
├── data/
│   ├── raw/
│   └── processed/hall_sensor_dataset.csv
│
├── model/
│   ├── train_lstm.py
│   ├── predict.py
│   └── saved_model/
│
├── results/
│   ├── loss_chart.png
│   ├── prediction_results.png
│   └── evaluation_metrics.txt
│
└── docs/
    └── Bao_cao_Hall_Sensor.pdf
```

---

## Software Stack

### Embedded

* C++
* Arduino Framework
* FreeRTOS

### Data Processing

* Python
* NumPy
* Pandas

### Machine Learning

* TensorFlow / Keras
* LSTM Networks

### Visualization

* Matplotlib
* Plotly

---

## Experimental Workflow

1. Collect magnetic field measurements.
2. Build a time-series dataset.
3. Normalize sensor values.
4. Create sliding-window sequences.
5. Train the LSTM model.
6. Evaluate forecasting performance.
7. Deploy the prediction model.

---

## Results

Evaluation metrics may include:

* Mean Absolute Error (MAE)
* Root Mean Square Error (RMSE)
* Mean Absolute Percentage Error (MAPE)

Visualization outputs:

* Training loss curves
* True vs Predicted magnetic field states
* Forecasting error analysis

---

## Future Improvements

* Multi-step magnetic field forecasting
* CNN-LSTM hybrid architecture
* GRU-based prediction models
* Physics-informed machine learning
* Sensor fusion with IMU data
* Robotics and autonomous system applications

---

## Applications

* Intelligent sensing systems
* Predictive maintenance
* Industrial monitoring
* Human-machine interaction
* Robotics perception
* Autonomous systems
* Magnetic field analysis

---

## License

This project is released for educational and research purposes.
