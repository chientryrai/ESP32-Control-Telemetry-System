

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

    if (_firstRun || dt <= 0.0f) {
        _firstRun = false;
        _lastTime = millis();
        return 0.0f;  // Return 0 on first call to avoid derivative spike
    }

    uint32_t now = millis();
    float deltaTime = (now - _lastTime) / 1000.0f;  // Convert to seconds

    if (deltaTime < 0.001f) {
        deltaTime = 0.001f;
    }

    float error = setpoint - processVariable;

    float pTerm = _kp * error;

    _integral += _ki * error * deltaTime;
    _integral = constrain(_integral, _integralMin, _integralMax);
    float iTerm = _integral;

    float dTerm = -_kd * (processVariable - _lastError) / deltaTime;
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