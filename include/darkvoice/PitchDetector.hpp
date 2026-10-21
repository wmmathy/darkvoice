#pragma once
#include <cstddef>
#include <span>
#include <vector>
namespace darkvoice {
struct PitchEstimate {
    double frequencyHz = 0.0;
    double midiNote = 0.0;
    float confidence = 0.0f;
    bool voiced = false;
};
// Worker/offline analysis only. This bounded O(window * lag) algorithm must NOT
// be called from an audio callback. One owner thread; no locks or shared state.
class PitchDetector {
  public:
    bool prepare(double rate, std::size_t window = 8192, double minimumHz = 40.0,
                 double maximumHz = 1000.0) noexcept;
    PitchEstimate analyze(std::span<const float> samples) noexcept;

  private:
    double rate_ = 0.0, minimum_ = 0.0, maximum_ = 0.0;
    std::size_t window_ = 0, minimumLag_ = 0, maximumLag_ = 0;
    std::vector<double> difference_;
};
} // namespace darkvoice
