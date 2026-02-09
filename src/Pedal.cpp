#include "Pedal.h"

Pedal::Pedal(int pin, int id) : _pin(pin), _id(id) {}

void Pedal::begin() {
  pinMode(_pin, INPUT);
  // ESP32-S3 ADC is 12-bit by default (0-4095)
  // We can use analogReadResolution if needed, but default is usually 12
  analogReadResolution(12);
}

void Pedal::update() {
  // 1. Oversampling / Reading
  long sum = 0;
  int samples = 16;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(_pin);
  }
  int rawAvg = sum / samples;
  // Revert to inverted reading as requested
  _currentRaw = 4095 - rawAvg;

  // 2. Normalize to Config Min/Max (0.0 - 1.0)
  // Determine actual effective range handling min > max (reverse pot)

  int start, end;
  bool isReversed = _config.min > _config.max;

  int clampedRaw = _currentRaw;

  if (isReversed) {
    start = _config.min - _config.deadzoneStart;
    end = _config.max + _config.deadzoneEnd;
    // Clamp
    if (clampedRaw > start)
      clampedRaw = start;
    if (clampedRaw < end)
      clampedRaw = end;
  } else {
    start = _config.min + _config.deadzoneStart;
    end = _config.max - _config.deadzoneEnd;
    // Clamp
    if (clampedRaw < start)
      clampedRaw = start;
    if (clampedRaw > end)
      clampedRaw = end;
  }

  // Protect against zero range
  if (start == end)
    end = start + 1;

  // 3. Map to 0-1000 (0.0% to 100.0%) for Curve lookup
  // map() handles start > end correctly
  long inputScaled = map(clampedRaw, start, end, 0, 1000);

  // Constrain just in case map extrapolates due to weird bounds (though we
  // clamped raw above)
  inputScaled = constrain(inputScaled, 0, 1000);

  if (_config.inverted) {
    inputScaled = 1000 - inputScaled;
  }

  // 4. Apply Curve
  // Curve points are 0..100 percentages. We interpolate between them.
  // curvePoints has 11 entries: 0%, 10%, ... 100% input

  int index = inputScaled / 100;
  int remainder = inputScaled % 100;

  if (index >= 10) {
    // 100% input case
    index = 9;
    remainder = 100;
  }

  int p1 = _config.curvePoints[index];
  int p2 = _config.curvePoints[index + 1];

  float valP1 = p1;
  float valP2 = p2;

  float interpolated = valP1 + ((valP2 - valP1) * (remainder / 100.0f));

  // interpolated is 0.0 - 100.0 (percentage)

  // 5. Apply Output Ceiling
  // If outputCeiling is 80, we scale 0-100 to 0-80? Or do we clip?
  // "Limiter" usually means clip, but for racing, re-scaling range is often
  // better so you still have full pedal travel. However, brake limit usually
  // means "I press 100% physically but game sees 80%". Wait, usually it's "I
  // want 100% game signal at 80% pressure". That's what calibration Min/Max is
  // for. "Output Limiter" as requested (cap max output) usually means "Don't
  // ever send more than 80% to game". This is useful if a car locks up easily.
  // So we Clip or Rescale. Let's CLIP. If result > ceiling, result = ceiling.
  if (interpolated > _config.outputCeiling)
    interpolated = (float)_config.outputCeiling;

  // 6. Scale to Final Output 0-4095
  float targetOutput = interpolated * 40.95f;

  // 7. Apply Smoothing (Low Pass Filter)
  // _config.smoothing is 0-99.
  // alpha = 1.0 means no filtering (take new value). alpha = 0.01 means mostly
  // old value. Map smoothing 0 (alpha 1.0) -> 90 (alpha 0.1) -> 99 (alpha 0.01)

  float alpha = 1.0f;
  if (_config.smoothing > 0) {
    // Simple mapping: alpha = 1.0 - (smoothing / 100.0)
    // if smoothing 90, alpha = 0.1
    // if smoothing 0, alpha = 1.0
    // Limit max smoothing to prevent freeze
    int s = constrain(_config.smoothing, 0, 98);
    alpha = 1.0f - (s / 100.0f);
  }

  // Initialize if first run or huge jump? No, standard EMA
  _filteredValue = (_filteredValue * (1.0f - alpha)) + (targetOutput * alpha);

  _currentOutput = (int)_filteredValue;
}

int Pedal::getOutput() { return _currentOutput; }

int Pedal::getRaw() { return _currentRaw; }

void Pedal::setConfig(PedalConfig config) {
  bool wasInv = _config.inverted;
  _config = config;
  // reset filter if needed? usually fine not to.
}

PedalConfig Pedal::getConfig() { return _config; }

void Pedal::calibrateMin() { _config.min = _currentRaw; }

void Pedal::calibrateMax() { _config.max = _currentRaw; }
