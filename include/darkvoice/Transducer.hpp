#pragma once
#include <cstddef>
#include <memory>
#include <span>

namespace darkvoice {
struct TransducerParameters {
    int model = 0; // Telephone, Megaphone, Radio
    float mix = 0;
};
class Transducer {
  public:
    Transducer() noexcept;
    ~Transducer();
    Transducer(const Transducer &) = delete;
    Transducer &operator=(const Transducer &) = delete;
    bool prepare(double rate, std::size_t maxFrames, std::size_t channels) noexcept;
    static bool valid(const TransducerParameters &) noexcept;
    bool setParameters(const TransducerParameters &) noexcept;
    bool process(std::span<float *>, std::size_t frames) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::size_t latencySamples() const noexcept {
        return 0;
    }

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    TransducerParameters target_{};
};
} // namespace darkvoice
