#pragma once
#include <cstddef>
#include <memory>
#include <span>
namespace darkvoice {
struct ColourParameters {
    int model = 0;
    float drive = .25f, toneHz = 16000, mix = 0;
};
class Colour {
  public:
    Colour() noexcept;
    ~Colour();
    static bool valid(const ColourParameters &) noexcept;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    bool setParameters(const ColourParameters &) noexcept;
    bool process(std::span<float *> channels, std::size_t frames) noexcept;
    void reset() noexcept;
    std::size_t latencySamples() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    ColourParameters target_{};
};
} // namespace darkvoice
