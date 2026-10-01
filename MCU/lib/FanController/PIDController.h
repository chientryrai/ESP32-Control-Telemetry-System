

#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

#include <Arduino.h>

// -----------------------------------------------------------------------------
// Lớp PID điều khiển quạt theo nhiệt độ mục tiêu.
// Cấu trúc này tính toán mức đầu ra dựa trên sai số giữa setpoint và process variable,
// giúp quạt tăng tốc khi nhiệt độ cao hơn mục tiêu và giảm tốc khi nhiệt độ thấp.
// -----------------------------------------------------------------------------
class PIDController {
public:
    // Constructor: nhận các hệ số PID và giới hạn đầu ra.
    PIDController(float kp, float ki, float kd,
                  float outputMin = 0.0f, float outputMax = 100.0f);
    ~PIDController();

    // Tính toán giá trị điều khiển cho một vòng lặp mới.
    // setpoint: nhiệt độ mục tiêu, processVariable: nhiệt độ hiện tại, dt: khoảng thời gian sample.
    float compute(float setpoint, float processVariable, float dt = 0.0f);

    // Reset các trạng thái tích lũy để bắt đầu lại ở trạng thái sạch.
    void reset();

    // Cập nhật lại hệ số Kp, Ki, Kd không cần khởi tạo lại object.
    void setGains(float kp, float ki, float kd);

    // Thiết lập giới hạn đầu ra của bộ điều khiển giá trị 0..100.
    void setOutputLimits(float min, float max);

    // Trả về thành phần P, I, D theo cách không dùng trong hệ thống hiện tại.
    // Hàm này chủ yếu phục vụ debug hoặc theo dõi bộ điều khiển.
    void getTerms(float &pTerm, float &iTerm, float &dTerm) const;

private:
    float _kp, _ki, _kd;          // Hệ số PID: P, I, D.
    float _outputMin, _outputMax; // Giới hạn đầu ra của controller, ví dụ 0..100.
    float _integral;              // Giá trị tích phân của sai số, giúp bù trừ độ lệch lâu dài.
    float _lastError;             // Sai số trước đó, dùng cho thành phần đạo hàm.
    uint32_t _lastTime;           // Thời điểm tính toán lần trước cùng với millis().
    bool _firstRun;               // Cờ báo lần tính toán đầu tiên, cần bỏ qua dữ liệu khởi tạo.

    float _integralMin, _integralMax;  // Giới hạn của tích phân để tránh tích lũy quá mức.
};

#endif // PID_CONTROLLER_H