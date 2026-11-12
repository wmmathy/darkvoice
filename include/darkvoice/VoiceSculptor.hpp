#pragma once
#include <array>
#include <cstddef>
#include <memory>
#include <span>

namespace darkvoice {
struct VoiceSculptorParameters {
    float bodyDb = 0, warmthDb = 0, presenceDb = 0;
};

class VoiceSculptor {
  public:
    VoiceSculptor() noexcept;
    ~VoiceSculptor();
    VoiceSculptor(const VoiceSculptor &) = delete;
    VoiceSculptor &operator=(const VoiceSculptor &) = delete;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    static bool valid(const VoiceSculptorParameters &) noexcept;
    bool setParameters(const VoiceSculptorParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept {
        return 0;
    }

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::array<double, 4> targets_{0, 1, 1, 0};
};
} // namespace darkvoice
