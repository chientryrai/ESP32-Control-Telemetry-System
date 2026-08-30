# Fan Controller with PID

PID-based fan speed controller for precise temperature regulation.

## Features
- Classic PID control with anti-windup
- Derivative on measurement (reduces noise sensitivity)
- Configurable output limits
- Integral term clamping
- First-run protection

## Usage
```cpp
#include "PIDController.h"

// Create PID for temperature control (20-40°C range, 0-100% output)
PIDController tempPID(2.0, 0.5, 1.0, 0.0, 100.0);  // Kp, Ki, Kd, minOut, maxOut

// In control loop:
float dutyCycle = tempPID.compute(targetTemp, currentTemp);
ledcWrite(FAN_CHANNEL, map(dutyCycle, 0, 100, 0, PWM_MAX));
```

## Tuning Guidelines
1. Start with Kp only (Ki=Kd=0) until stable
2. Add Ki to eliminate steady-state error
3. Add Kd to reduce overshoot and improve response
4. For fan control: conservative values to avoid oscillation

## Parameters for this project
- Kp: 2.0 (moderate response)
- Ki: 0.5 (slow integral to prevent windup)
- Kd: 1.0 (moderate derivative for damping)
- Output: 0-100% duty cycle