

#include "PIDController.h"

// Constructor: lưu các tham số bộ điều khiển và khởi tạo trạng thái ban đầu.
PIDController::PIDController(float kp, float ki, float kd,
                             float outputMin, float outputMax)
    : _kp(kp), _ki(ki), _kd(kd),
      _outputMin(outputMin), _outputMax(outputMax),
      _integral(0.0f), _lastError(0.0f), _lastTime(0), _firstRun(true) {

    _integralMin = _outputMin;
    _integralMax = _outputMax;
}

PIDController::~PIDController() {}

// Hàm compute: thực hiện một bước tính toán PID dựa trên sai số giữa nhiệt độ thực và nhiệt độ mục tiêu.
// Với hệ thống làm mát, khi nhiệt độ hiện tại > setpoint thì sai số dương và đầu ra tăng để quạt quay nhanh hơn.
float PIDController::compute(float setpoint, float processVariable, float dt) {
    uint32_t now = millis();

    if (_firstRun) {
        _firstRun = false;
        _lastTime = now;
        _lastError = processVariable;
        return 0.0f;
    }

    float deltaTime = dt;
    if (deltaTime <= 0.0f) {
        deltaTime = (now - _lastTime) / 1000.0f;
    }

    if (deltaTime <= 0.001f) {
        deltaTime = 0.001f;
    }

    // Sai số của hệ thống làm mát: nếu nhiệt độ hiện tại cao hơn setpoint thì error > 0.
    // Khi error dương, output của PID tăng, kích hoạt duty quạt lớn hơn để làm mát.
    float error = processVariable - setpoint;

    float pTerm = _kp * error;

    _integral += _ki * error * deltaTime;
    _integral = constrain(_integral, _integralMin, _integralMax);
    float iTerm = _integral;

    float derivativeError = processVariable - _lastError;
    float dTerm = _kd * (derivativeError / deltaTime);
    _lastError = processVariable;

    float output = pTerm + iTerm + dTerm;
    output = constrain(output, _outputMin, _outputMax);

    _lastTime = now;

    return output;
}

// Reset toàn bộ trạng thái tích lũy của PID để bắt đầu lại từ đầu.
void PIDController::reset() {
    _integral = 0.0f;
    _lastError = 0.0f;
    _firstRun = true;
}

// Cập nhật các hệ số Kp, Ki, Kd mà không cần phá huỷ tạo lại object.
void PIDController::setGains(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

// Thiết lập khoảng giá trị đầu ra cho controller, đảm bảo min < max.
void PIDController::setOutputLimits(float min, float max) {
    if (min >= max) {
        return;
    }

    _outputMin = min;
    _outputMax = max;

    _integral = constrain(_integral, _outputMin, _outputMax);
    _integralMin = _outputMin;
    _integralMax = _outputMax;
}

// Hàm debug: trả về các thành phần P/I/D, nhưng cần dữ liệu setpoint và process variable đầy đủ mới có ý nghĩa.
void PIDController::getTerms(float &pTerm, float &iTerm, float &dTerm) const {

    float error = 0.0f;
    pTerm = _kp * error;
    iTerm = _integral;
    dTerm = 0.0f;
}