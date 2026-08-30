

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>

class PIDController {
public:
    PIDController(float kp, float ki, float kd, 
                  float outputMin = 0.0f, float outputMax = 100.0f);
    ~PIDController();

    
    float compute(float setpoint, float processVariable, float dt = 0.0f);

    
    void reset();

    
    void setGains(float kp, float ki, float kd);

    
    void setOutputLimits(float min, float max);

    
    void getTerms(float &pTerm, float &iTerm, float &dTerm) const;

private:
    float _kp, _ki, _kd;          // PID gains
    float _outputMin, _outputMax; // Output limits
    float _integral;              // Integral term
    float _lastError;             // Last error for derivative
    uint32_t _lastTime;           // Last computation time
    bool _firstRun;               // Flag for first computation

    float _integralMin, _integralMax;
};

#endif // PID_CONTROLLER_H