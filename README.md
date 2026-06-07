# 3-Axis Magnetic Field Dynamics Prediction using LSTM

## Overview

This project presents an end-to-end embedded sensing and deep learning prediction system for real-time 3-axis magnetic field monitoring and forecasting. A custom-built hardware platform, utilizing an orthogonal array of linear Hall-effect sensors, acquires magnetic field vectors along the X, Y, and Z axes ($B_x, B_y, B_z$). Simultaneously, an edge-deployed Long Short-Term Memory (LSTM) recurrent neural network forecasts future magnetic field states based on historical sensor observations.

The system bridges the gap between physical electrical instrumentation and predictive deep learning, demonstrating a high-frequency, low-latency pipeline tailored for the predictive maintenance of electrical machines and industrial automation systems.

---

## Features

* **High-Frequency 3-Axis Sensing:** Continuous monitoring of magnetic vector fields ($B_x, B_y, B_z$) within a linear range of $\pm 1000\text{ Gauss}$.
* **Edge-Side Signal Processing:** Hardware-level offset compensation and real-time noise reduction running on an ESP32 microcontroller.
* **Low-Latency Telemetry Stream:** High-speed, full-duplex wireless data transmission at **50 Hz** (20ms sampling interval) leveraging the **WebSockets** protocol.
* **Deep Learning Time-Series Forecasting:** A robust, two-layer LSTM architecture optimized for short-term spatial-temporal trajectory tracking.
* **Real-Time Prediction Pipeline:** Instantaneous "True vs. Predicted" state generation with an optimized end-to-end system latency of **< 40ms**.
* **Experimental Visualization Dashboard:** Automated data logging and web-based analytical plotting implemented via Python Dash and Plotly.

---

## System Architecture

```text
3x Linear Hall Sensors (SS49E)
            │
            ▼ [Analog Voltages V_x, V_y, V_z]
     ESP32 Controller (12-bit SAR ADC @ 50Hz)
            │
            ▼
 Signal Processing Layer (V_offset Calibration + Digital EMA Filter)
            │
            ▼ [JSON Stream over WebSockets]
     Data Collection Server (Edge Node Gateway)
            │
            ▼
 Time-Series Dataset (Sliding Window: 50 Steps / 1 Second Context)
            │
            ▼ [Feature Vectors: (50, 3)]
       LSTM Network (64 units -> Dropout 0.2 -> 32 units -> Dense)
            │
            ▼
Future Magnetic State Prediction [Output Tensor: (1, 3) for t+1]

```

---

## Hardware Components

### Embedded Platform

* **ESP32 Development Board (SoC ESP32-WROOM-32E):** Handles multi-tasking FreeRTOS routines, high-frequency ADC sampling, digital filtering, and network sockets stack.

### Sensors

* **Three Linear Hall-Effect Sensors (Honeywell SS49E):** Features a ratiometric sensitivity of $1.4\text{ mV/G}$ and a wide response range.
* **Orthogonal Mechanical Frame:** A custom 3D-printed mounting bracket ensuring an absolute $90^\circ$ perpendicular orientation between sensors to eliminate cross-axis geometric sensitivity.

### Power Supply

* **Decoupled 5V DC Supply:** Regulated low-noise power input with dedicated $100\text{ nF}$ ceramic decoupling capacitors to minimize power rail ripple and ADC quantization noise.

---

## Magnetic Field Measurement

The magnetic field components along the orthogonal spatial coordinates are measured independently:

$$\vec{B} = B_x\hat{i} + B_y\hat{j} + B_z\hat{k}$$

The total magnetic field magnitude is calculated dynamically using the Euclidean norm:

$$B = \sqrt{B_x^2 + B_y^2 + B_z^2}$$

The sensing platform delivers a continuous, high-fidelity stream of magnetic field vectors, forming an empirical time-series dataset tailored for regression-based machine learning applications.

---

## Signal Processing

### Offset Calibration

An automatic 5-second startup calibration routine samples the ambient baseline environment to isolate and negate individual sensor offset voltages ($V_{\text{offset}}$) and inherent microcontroller ADC bias errors at 0 Gauss.

### Exponential Moving Average Filter

To suppress high-frequency electromagnetic interference (EMI) and random environmental noise, an Exponential Moving Average (EMA) filter is applied directly at the edge:

$$y_t = \alpha x_t + (1 - \alpha)y_{t-1}$$

where:

* $x_t$ is the raw raw measurement from the 12-bit ADC at time step $t$.
* $y_t$ is the smoothed, filtered output forwarded to the telemetry queue.
* $\alpha$ is the configurable smoothing factor (dynamically tuned to balance noise rejection and phase delay).

---

## Data Acquisition Pipeline

1. **Multi-Channel ADC Sampling:** Sequential reading of analog Hall voltage outputs via calibrated ESP32 ADC channels with an $11\text{ dB}$ attenuation setting ($0.1\text{V} - 3.1\text{V}$ linear window).
2. **Physical Unit Conversion:** Transferring raw voltage values into standardized magnetic induction units (Gauss) based on sensor sensitivity parameters.
3. **Dynamic Offset Compensation:** Real-time subtracting of the baseline calibration matrix.
4. **Edge EMA Filtering:** Smoothing data points per axis to eliminate measurement ripples.
5. **JSON Stream Over WebSockets:** Encapsulating $[B_x, B_y, B_z]$ arrays into JSON text and streaming it over low-overhead WebSockets.
6. **Empirical CSV Data Logging:** Compiling and saving incoming streams on the machine learning host machine under structured directory sets.
7. **Sliding Window Preparation:** Reshaping historical series arrays into standard 3D input blocks for LSTM training.

---

## LSTM-Based Prediction Model

### Objective

Forecast the spatial state of the 3-axis magnetic field vector at the next immediate epoch ($t+1$) given a look-back history window of $n$ time steps.

Input sequence (1-second temporal history):

$$\mathbf{X} = [B_x, B_y, B_z]_{t-49:t} \in \mathbb{R}^{50 \times 3}$$

Output tensor (Next step vector forecast):

$$\mathbf{\hat{Y}} = [B_x, B_y, B_z]_{t+1} \in \mathbb{R}^{3}$$

### Network Architecture

* **Input Layer:** Shape matches the input tensor dimensions $(50, 3)$.
* **LSTM Layer 1:** 64 hidden units configured to return the full sequence output.
* **Dropout Layer:** Structured with a $0.2$ (20%) rate to prevent training over-fitting.
* **LSTM Layer 2:** 32 hidden units processing internal sequence vectors.
* **Dense Output Layer:** 3 linear neurons providing direct, unconstrained regression targets.

### Prediction Targets

* Future $B_x$ value at $t+1$
* Future $B_y$ value at $t+1$
* Future $B_z$ value at $t+1$

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
│   └── processed/             
│
├── model/
│   ├── train_lstm.py          
│   ├── predict.py             
│   └── saved_model/           
│
├── results/
│   ├── loss_chart.png         
│   ├── prediction_results.png 
│   
│
└── docs/
    └── Bao_cao_Hall_Sensor.pdf

```

---

## Software Stack

### Embedded

* **C++ / Arduino Framework:** Core programming language and environment for modular sensor routines.
* **FreeRTOS:** Manages non-blocking concurrent tasks for ADC sampling, filtering, and network streaming.

### Data Processing

* **Python 3.10+**
* **NumPy & Pandas:** For dataset vector operations, sliding-window transformations, and file operations.

### Machine Learning

* **TensorFlow / Keras:** Framework for defining, compiling, and running the deep neural network.
* **LSTM Recurrent Architectures:** Selected for handling sequential, non-linear dynamic physics data.

### Visualization

* **Matplotlib:** Handles static evaluation reporting charts.
* **Dash & Plotly:** Delivers an interactive, dynamic web app UI showing instant true/predicted waveforms.

---

## Experimental Workflow

1. **Physical Sample Collection:** Stream data at 50Hz by manipulating magnetic test rigs near the sensor module.
2. **Dataset Consolidation:** Aggregate records into an uniform historical CSV base dataset.
3. **Min-Max Normalization:** Scale variables linearly between $[0, 1]$ to stabilize backpropagation weights.
4. **Sliding-Window Partitioning:** Segment data streams into shifting time arrays of shape `(batch, 50, 3)`.
5. **LSTM Deep Learning Training:** Train across 100 epochs utilizing EarlyStopping callback protocols.
6. **Validation Testing:** Assess validation sets on metric targets to qualify offline model reliability.
7. **Online Web Server Deployment:** Launch live prediction threads to run simultaneous inference alongside the WebSocket UI stream.

---

## Results

### Evaluation Metrics

The architecture achieved high accuracy tracking complex dynamic waveforms under offline testing validation datasets:

* **Mean Absolute Error (MAE):** $0.0084\text{ Gauss}$
* **Root Mean Square Error (RMSE):** $0.0118\text{ Gauss}$
* **Online Experimental Tracking Error:** Maintained consistently under **3.5%** with zero cumulative drift.

### Visualization Outputs

* **Training Loss Curves:** Shows smooth convergence across train/validation folds indicating robust generalization.
* **True vs. Predicted Slices:** High-fidelity browning-edge bams, effectively predicting phase inflections and extremes without latency delay gaps.
* **Forecasting Error Analysis:** Demonstrates narrow Gaussian error distributions centered at zero.

---

## Future Improvements

* **Multi-Step Vector Forecasting:** Expanding the dense horizon layers to forecast an arbitrary sequence string $(t+m)$ instead of just $(t+1)$.
* **Hybrid CNN-LSTM Topologies:** Deploying convolutional filters ahead of recurrent blocks to capture instantaneous spatial cross-axis features before temporal tracking.
* **Edge TinyML Compilation:** Quantizing and compiling trained models into TensorFlow Lite format to flash directly into the secondary core of the ESP32 chip.
* **Physics-Informed Machine Learning (PINN):** Incorporating Maxwell's electromagnetic boundary condition losses directly into the neural network custom loss function.
* **Sensor Fusion Integrations:** Fusing telemetry streams with multi-axis Inertial Measurement Units (IMUs) to filter ambient vibration noises.

---

## Applications

* **Intelligent Electrical Plant Monitoring:** Non-invasive monitoring of stator/rotor fluxes inside heavy electric machinery.
* **Predictive Maintenance:** Detecting turn-to-turn short circuits, dynamic eccentricities, and bearing wear via signature magnetic distortion forecasting.
* **Robotics Perception and Navigation:** Mapping localized structural stray field gradients for indoor robot positioning systems.
* **Industrial Automated Inspection:** Real-time sorting and magnetic inspection of moving ferromagnetic parts on high-speed assembly lines.
