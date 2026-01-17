#ifndef PEDAL_H
#define PEDAL_H

#include <Arduino.h>

struct PedalConfig {
    int min = 0;
    int max = 4095;
    int deadzoneStart = 0; // Deadzone at the beginning (raw units from min)
    int deadzoneEnd = 0;   // Deadzone at the end (raw units from max)
    bool inverted = false;
    // S-Curve points (0-100% input -> 0-100% output mapped to 0-1023 internally or similar?)
    // Let's store 11 points (0, 10, 20 ... 100%) mapped to 0-4095 or 0-10000 range for higher internal precision
    int curvePoints[11] = {0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100}; // percentages
    
    int smoothing = 10; // 0 (raw) to 99 (heavy slow), default light smoothing
    int outputCeiling = 100; // 0-100% max output cap
};

class Pedal {
public:
    Pedal(int pin, int id);
    void begin();
    void update(); // Call frequently loop
    int getOutput(); // Returns 0-4095 or USB range (0-1023 or 0-65535 depending on library choice, let's stick to 12bit 0-4095 local, map to USB later)
    int getRaw();
    
    void setConfig(PedalConfig config);
    PedalConfig getConfig();
    
    // Calibration helpers
    void calibrateMin();
    void calibrateMax();

private:
    int _pin;
    int _id;
    PedalConfig _config;
    
    // Filtering
    float _filteredValue = 0;
    
    // Processing
    int applyCurve(int inputPercentage);
    
    int _currentRaw = 0;
    int _currentOutput = 0;
};

#endif
