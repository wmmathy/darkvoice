// Developer audit: exit0 validates measurements, not an alias-quality threshold.
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <darkvoice/Colour.hpp>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <span>
#include <string>

#ifndef DARKVOICE_AUDIT_REVISION
#define DARKVOICE_AUDIT_REVISION "not-specified"
#endif

namespace {
constexpr std::size_t period = 4096, block = 512;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(std::endian::native == std::endian::little);

bool save(const std::filesystem::path &path, const std::array<float, period> &samples) {
    auto *file = std::fopen(path.string().c_str(), "wb");
    if (!file)
        return false;
    const auto count = std::fwrite(samples.data(), sizeof(float), samples.size(), file);
    const bool closed = std::fclose(file) == 0;
    return count == samples.size() && closed;
}

double rms(const std::array<float, period> &samples) {
    double energy = 0;
    for (float value : samples) {
        if (!std::isfinite(value))
            return -1;
        energy += static_cast<double>(value) * value;
    }
    return std::sqrt(energy / period);
}
} // namespace

int main(int argc, char **argv) {
    if (argc > 2 || (argc == 2 && argv[1][0] == '-')) {
        std::fprintf(stderr, "usage: colour-alias-probe [output-directory]\n");
        return 2;
    }
    try {
        const std::filesystem::path directory = argc == 2 ? argv[1] : "/tmp/darkvoice-colour-alias";
        std::filesystem::create_directories(directory);
        std::ofstream metadata(directory / "metadata.csv");
        if (!metadata)
            return 1;
        metadata << "case,rate,model,amplitude,bin,warm_frames,frames,input_rms,output_rms,source_"
                    "revision\n"
                 << std::setprecision(17);
        int number = 0;
        for (double rate : {44100., 48000., 88200., 96000.})
            for (int model : {0, 1})
                for (double amplitude : {.15, .5})
                    for (int bin : {719, 1597}) {
                        darkvoice::Colour effect;
                        if (!effect.prepare(rate, block, 1) ||
                            !effect.setParameters({model, 1, 16000, 1}))
                            return 1;
                        effect.reset();
                        std::array<float, period> input{}, output{};
                        for (std::size_t i = 0; i < period; ++i)
                            input[i] = static_cast<float>(
                                amplitude * std::sin(2 * std::numbers::pi * bin * i / period));
                        const auto warm =
                            static_cast<std::size_t>(std::ceil(rate / period)) * period;
                        for (std::size_t offset = 0; offset < warm + period; offset += block) {
                            std::array<float, block> audio{};
                            for (std::size_t i = 0; i < block; ++i)
                                audio[i] = input[(offset + i) % period];
                            std::array<float *, 1> channels{audio.data()};
                            if (!effect.process(channels, block))
                                return 1;
                            if (offset >= warm)
                                for (std::size_t i = 0; i < block; ++i)
                                    output[offset - warm + i] = audio[i];
                        }
                        const double inputRms = rms(input), outputRms = rms(output);
                        if (!(inputRms > 0 && outputRms > 0) || !std::isfinite(inputRms) ||
                            !std::isfinite(outputRms) ||
                            !save(directory / ("case-" + std::to_string(number) + "-input.f32"),
                                  input) ||
                            !save(directory / ("case-" + std::to_string(number) + "-output.f32"),
                                  output))
                            return 1;
                        metadata << number << ',' << rate << ',' << model << ',' << amplitude << ','
                                 << bin << ',' << warm << ',' << period << ',' << inputRms << ','
                                 << outputRms << ',' << DARKVOICE_AUDIT_REVISION << '\n';
                        ++number;
                    }
        metadata.close();
        if (!metadata)
            return 1;
        std::printf("Rendered %d fixtures, each4096 finite samples with positive energy. No "
                    "quality pass is encoded.\n",
                    number);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "audit failure: %s\n", error.what());
        return 1;
    }
}
