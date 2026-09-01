

#include "PIDController.h"

PIDController::PIDController(float kp, float ki, float kd, 
                             float outputMin, float outputMax)
    : _kp(kp), _ki(ki), _kd(kd),
      _outputMin(outputMin), _outputMax(outputMax),
      _integral(0.0f), _lastError(0.0f), _lastTime(0), _firstRun(true) {

    _integralMin = _outputMin;
    _integralMax = _outputMax;
}

PIDController::~PIDController() {}

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

    // Quạt làm mát: error = nhiệt độ hiện tại - target (c dương nghĩa nhiệt độ cao hơn target)
    float error = processVariable - setpoint;

    float pTerm = _kp * error;

    _integral += _ki * error * deltaTime;
    _integral = constrain(_integral, _integralMin, _integralMax);
    float iTerm = _integral;

    float derivativeError = processVariable - _lastError;
    float dTerm = _kd * (derivativeError / deltaTime);  // bỏ dấu trừ (cùng chiều với error)
    _lastError = processVariable;

    float output = pTerm + iTerm + dTerm;
    output = constrain(output, _outputMin, _outputMax);

    _lastTime = now;

    return output;
}

void PIDController::reset() {
    _integral = 0.0f;
    _lastError = 0.0f;
    _firstRun = true;
}

void PIDController::setGains(float kp, float ki, float kd) {
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void PIDController::setOutputLimits(float min, float max) {
    if (min >= max) {
        return;  // Invalid limits
    }
    
    _outputMin = min;
    _outputMax = max;

    _integral = constrain(_integral, _outputMin, _outputMax);
    _integralMin = _outputMin;
    _integralMax = _outputMax;
}

void PIDController::getTerms(float &pTerm, float &iTerm, float &dTerm) const {

    float error = 0.0f; // Would need setpoint and process variable
    pTerm = _kp * error;
    iTerm = _integral;
    dTerm = 0.0f; // Would need previous error
}