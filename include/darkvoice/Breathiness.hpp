#pragma once
#include <cstddef>
#include <memory>
#include <span>
namespace darkvoice {
struct BreathinessParameters {
    float amount = 0;
};
class Breathiness {
  public:
    Breathiness() noexcept;
    ~Breathiness();
    Breathiness(const Breathiness &) = delete;
    Breathiness &operator=(const Breathiness &) = delete;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    static bool valid(const BreathinessParameters &) noexcept;
    bool setParameters(const BreathinessParameters &) noexcept;
    bool process(std::span<float *>, std::size_t frames) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept {
        return 0;
    }

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    double target_ = 0;
};
} // namespace darkvoice
