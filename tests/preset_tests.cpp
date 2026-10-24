#include "PresetBank.hpp"
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <darkvoice/ParameterSchema.hpp>
#include <filesystem>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <thread>

namespace {
int checks = 0;
void require(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
void good(const juce::Result &result, const char *message) {
    if (result.failed())
        std::cerr << result.getErrorMessage() << '\n';
    require(result.wasOk(), message);
}
struct TemporaryDirectory {
    juce::File file = juce::File(std::filesystem::canonical(
                                     juce::File::getSpecialLocation(juce::File::tempDirectory)
                                         .getFullPathName()
                                         .toStdString())
                                     .string())
                          .getChildFile("darkvoice-presets-" + juce::Uuid().toString());
    TemporaryDirectory() {
        good(file.createDirectory(), "Cannot create test root");
    }
    ~TemporaryDirectory() {
        file.deleteRecursively();
    }
};
juce::String jsonFor(const darkvoice::PresetValues &values, int version = 10,
                     const juce::String &name = "Test preset") {
    juce::DynamicObject::Ptr parameters = new juce::DynamicObject();
    for (const auto &[id, value] : values)
        parameters->setProperty(juce::Identifier(juce::String(id)), value);
    juce::DynamicObject::Ptr object = new juce::DynamicObject();
    object->setProperty("schemaVersion", version);
    object->setProperty("name", name);
    object->setProperty("parameters", juce::var(parameters.get()));
    return juce::JSON::toString(juce::var(object.get()), true);
}
void write(const juce::File &file, const juce::String &text) {
    require(file.replaceWithText(text), "Cannot write fixture");
}
// Lock the prior catalogue's identities and every float setting to baseline
// ea3ded9. Fixed little-endian float bytes avoid JSON/locale formatting effects.
std::uint64_t factoryFingerprint(const darkvoice::Preset &preset) {
    std::uint64_t hash = 1469598103934665603ULL;
    const auto byte = [&](unsigned char value) {
        hash ^= value;
        hash *= 1099511628211ULL;
    };
    const auto text = [&](const std::string &value) {
        for (unsigned char c : value)
            byte(c);
        byte(0);
    };
    text(preset.id.toStdString());
    text(preset.name.toStdString());
    for (const auto &[id, value] : preset.values) {
        // Preserve the original 49-field identity/settings lock unchanged.
        if (id == "spaceModel" || id == "spaceDecay" || id == "spaceToneHz" ||
            id == "spacePreDelayMs" || id == "spaceMix")
            continue;
        text(id);
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (int shift = 0; shift < 32; shift += 8)
            byte(static_cast<unsigned char>((bits >> shift) & 255));
    }
    return hash;
}
void basicPersistence() {
    TemporaryDirectory temp;
    darkvoice::PresetBank bank(temp.file.getChildFile("library"));
    good(bank.refresh(), "Empty library refresh");
    require(bank.presets().size() == 15, "Fifteen factory sounds expected");
    std::set<std::string> factorySnapshots;
    for (const auto &preset : bank.presets()) {
        require(preset.factory && !preset.favorite, "Factory flags");
        require(preset.values.at("limiterEnabled") == 0 &&
                    preset.values.at("limiterCeilingDb") == -1 &&
                    preset.values.at("limiterReleaseMs") == 100,
                "Factory limiter remains off with neutral defaults");
        good(darkvoice::PresetBank::validate(preset.values), "Factory values invalid");
        for (std::size_t i = 38; i < 46; ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(preset.values.at(spec.id) == spec.defaultValue,
                    "Existing factories keep new stereo delay neutral");
        }
        require(preset.values.size() == 54 && !preset.values.contains("bypass"),
                "Preset excludes bypass");
        if (preset.id != "factory-double-tracked-rock")
            for (std::size_t i = 46; i < darkvoice::parameterSchema.size(); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                require(preset.values.at(spec.id) == spec.defaultValue,
                        "Existing factories keep simulated doubler neutral");
            }
        for (std::size_t i = 50; i < darkvoice::parameterSchema.size(); ++i) {
            const auto &spec = darkvoice::parameterSchema[i];
            require(preset.values.at(spec.id) == spec.defaultValue,
                    "All factories keep new Space Reverb neutral");
        }
        factorySnapshots.insert(jsonFor(preset.values).toStdString());
    }
    require(factorySnapshots.size() == 15, "Factory settings must actually differ");
    const std::map<std::string, std::uint64_t> priorFactoryFingerprints{
        {"factory-dark-synth", 8960626116177314993ULL},
        {"factory-industrial-bass", 11621749232851812955ULL},
        {"factory-heavy-rock", 7871813731075746782ULL},
        {"factory-clean-studio", 12085619434456688729ULL},
        {"factory-gothic-bass", 11641356722098154743ULL},
        {"factory-cinematic-bass", 5013901383243817466ULL},
        {"factory-dark-wave", 1751362799623169572ULL},
        {"factory-deep-narrator", 10543216408023329900ULL},
        {"factory-industrial-whisper", 4518812483475202072ULL},
        {"factory-grunge-baritone", 15545579731950925572ULL},
        {"factory-acoustic-baritone", 8735270006636597024ULL},
        {"factory-dark-melodic", 16943747349606405484ULL},
        {"factory-industrial-power", 10686810074294856948ULL},
        {"factory-natural-dark-baritone", 8944461555890222485ULL}};
    std::set<std::string> identifiers;
    for (const auto &preset : bank.presets())
        require(identifiers.insert(preset.id.toStdString()).second, "Factory IDs must be unique");
    for (const auto &[id, fingerprint] : priorFactoryFingerprints) {
        const auto *preset = bank.find(id);
        require(preset && factoryFingerprint(*preset) == fingerprint,
                "Existing factory identity/name/all settings changed");
    }
    const auto *doubled = bank.find("factory-double-tracked-rock");
    require(doubled && doubled->name == "Double Tracked Rock" &&
                doubled->values.at("doublerMix") > 0 &&
                doubled->values.at("doublerVariationMs") > 0,
            "Double Tracked Rock uses real simulated ADT");
    const auto factory = bank.presets().front().id;
    require(bank.rename(factory, "Changed").failed(), "Cannot rename factory");
    good(bank.setFavorite(factory, true), "Favorite factory");
    require(bank.search("BARITONE", true).size() == 1, "Case insensitive favorites search");
    auto settings = darkvoice::PresetBank::defaults();
    settings["eqMidGainDb"] = 3.4f;
    settings["delayMix"] = .2f;
    settings["gateRatio"] = 4;
    settings["gateThresholdDb"] = -45;
    settings["gateReleaseMs"] = 200;
    settings["deEssReductionDb"] = 6;
    settings["flangerMix"] = .7f;
    settings["flangerFeedback"] = -.4f;
    settings["limiterEnabled"] = 1;
    settings["limiterCeilingDb"] = -9.2f;
    settings["limiterReleaseMs"] = 210;
    settings["chorusMix"] = .3f;
    settings["stereoWidth"] = 1.5f;
    settings["stereoDelayMode"] = 2;
    settings["stereoDelayMs"] = 420;
    settings["stereoDelayFeedback"] = .6f;
    settings["stereoDelayMix"] = .2f;
    settings["stereoDelaySync"] = 1;
    settings["stereoDelayBpm"] = 80;
    settings["stereoDelayDivision"] = 3;
    settings["stereoDelayHostTempo"] = 1;
    settings["doublerSeparationMs"] = 31.5f;
    settings["doublerVariationMs"] = 4.5f;
    settings["doublerRateHz"] = 1.3f;
    settings["doublerMix"] = .3f;
    settings["spaceModel"] = 2;
    settings["spaceDecay"] = .7f;
    settings["spaceToneHz"] = 5300;
    settings["spacePreDelayMs"] = 17.5f;
    settings["spaceMix"] = .2f;
    juce::String id;
    good(bank.saveAs("  My voice  ", settings, id), "Save user preset");
    require(id.length() == 32 && bank.find(id) != nullptr, "UUID save identifier");
    require(bank.find(id)->name == "My voice", "Trimmed save name");
    const auto originalId = id;
    const auto file = temp.file.getChildFile("library").getChildFile(id + ".json");
    const auto original = file.loadFileAsString();
    good(bank.saveAs("My voice", settings, id), "Same name save-as creates copy");
    require(id != originalId && file.loadFileAsString() == original, "Save-as preserves original");
    good(bank.rename(originalId, "Renamed voice"), "User rename");
    good(bank.setFavorite(originalId, true), "Favorite user");
    darkvoice::PresetBank restored(temp.file.getChildFile("library"));
    good(restored.refresh(), "Persistent refresh");
    require(restored.find(originalId) && restored.find(originalId)->favorite,
            "User favorite persisted");
    require(restored.find(factory) && restored.find(factory)->favorite,
            "Factory favorite persisted");
    require(restored.find(originalId)->name == "Renamed voice", "Rename persisted");
    require(std::abs(restored.find(originalId)->values.at("eqMidGainDb") - 3.4f) < .00001f,
            "Parameter roundtrip");
    require(restored.search("renamed").size() == 1, "Name search");
    require(restored.find(originalId)->values == settings, "Full v10 nondefault preset roundtrip");
    const auto exported = temp.file.getChildFile("export.json");
    good(restored.exportFile(originalId, exported), "Export user preset");
    const auto exportedBytes = exported.loadFileAsString();
    require(restored.exportFile(originalId, exported).failed(), "Export refuses existing file");
    require(exported.loadFileAsString() == exportedBytes, "Failed export preserves existing file");
    juce::String imported;
    good(restored.importFile(exported, imported), "Import export roundtrip");
    require(imported != originalId && imported != id, "Import regenerates identity");
    require(restored.find(imported)->values == restored.find(originalId)->values,
            "Imported values exact");
    // Concurrent exports to the same new destination must not replace the winner.
    const auto raced = temp.file.getChildFile("raced.json");
    std::atomic<bool> go{false};
    std::atomic<int> successes{0};
    auto exportOnce = [&] {
        while (!go.load())
            std::this_thread::yield();
        if (restored.exportFile(originalId, raced).wasOk())
            ++successes;
    };
    std::thread first(exportOnce), second(exportOnce);
    go.store(true);
    first.join();
    second.join();
    require(successes.load() == 1, "Exactly one concurrent export may create a destination");
    require(raced.loadFileAsString() == exportedBytes, "Concurrent export winner content intact");
    const auto factoryExport = temp.file.getChildFile("factory.json");
    good(restored.exportFile(factory, factoryExport), "Factory export");
    good(restored.importFile(factoryExport, imported), "Factory import as user copy");
    require(!restored.find(imported)->factory, "Imported factory is editable user copy");
    good(restored.rename(imported, "My factory copy"), "Rename factory copy");
    require(restored.rename("../../outside", "Bad").failed(), "Reject path identifier");
    require(restored.setFavorite("unknown", true).failed(), "Reject unknown favorite");
}
void malformedAndMigration() {
    TemporaryDirectory temp;
    darkvoice::PresetBank bank(temp.file.getChildFile("bank"));
    auto settings = darkvoice::PresetBank::defaults();
    juce::String id;
    auto invalid = settings;
    invalid["inputDb"] = std::numeric_limits<float>::quiet_NaN();
    require(bank.saveAs("NaN", invalid, id).failed(), "Reject nonfinite in memory");
    invalid = settings;
    invalid["inputDb"] = 25;
    require(bank.saveAs("Range", invalid, id).failed(), "Reject out of range");
    invalid = settings;
    invalid.erase("drive");
    require(bank.saveAs("Missing", invalid, id).failed(), "Reject missing parameter");
    invalid = settings;
    invalid["bypass"] = 0;
    require(bank.saveAs("Bypass", invalid, id).failed(), "Reject bypass in preset");
    for (const auto &name : {juce::String(), juce::String("\t"),
                             juce::String::repeatedString("a", 81), juce::String("bad\nname")})
        require(bank.saveAs(name, settings, id).failed(), "Reject invalid name");
    const auto external = temp.file.getChildFile("external.json");
    {
        darkvoice::PresetBank boundary(temp.file.getChildFile("boundary"));
        auto decimal = juce::JSON::parse(jsonFor(settings));
        auto *numbers = decimal.getDynamicObject()->getProperty("parameters").getDynamicObject();
        numbers->setProperty("chorusRateHz", 0.1);
        numbers->setProperty("eqMidQ", 0.2);
        write(external, juce::JSON::toString(decimal, true));
        good(boundary.importFile(external, id),
             "Decimal minima accepted in float parameter domain");
        require(boundary.find(id)->values.at("chorusRateHz") == .1f &&
                    boundary.find(id)->values.at("eqMidQ") == .2f,
                "Decimal minima preserved");
        for (double bad : {.099, 5.01, std::numeric_limits<double>::max(),
                           -std::numeric_limits<double>::max()}) {
            numbers->setProperty("chorusRateHz", bad);
            write(external, juce::JSON::toString(decimal, true));
            require(boundary.importFile(external, id).failed(),
                    "Reject below/above/overflow float boundaries");
        }
        numbers->setProperty("chorusRateHz", .1);
        numbers->setProperty("eqMidQ", .199);
        write(external, juce::JSON::toString(decimal, true));
        require(boundary.importFile(external, id).failed(),
                "Reject meaningfully below existing Q minimum");
    }
    const auto initial = bank.presets().size();
    for (const auto &data :
         {juce::String("{}"), juce::String("[]"), juce::String("{broken"), jsonFor(settings, 11),
          jsonFor(settings).replace("\"inputDb\": 0.0", "\"inputDb\": true"),
          juce::String("{\"schemaVersion\":2,\"schemaVersion\":1}")}) {
        write(external, data);
        require(bank.importFile(external, id).failed(), "Reject malformed JSON/version/types");
        require(bank.presets().size() == initial && id.isEmpty(),
                "Import failure does not mutate bank");
    }
    const auto cleanJson = jsonFor(settings);
    for (const auto &hostile :
         {cleanJson + " trailing-garbage", cleanJson + "{}",
          cleanJson.replace("\"schemaVersion\": 10", "\"schemaVersion\": 4294967301"),
          juce::String("{\"nested\":") + juce::String::repeatedString("[", 10000) + "0" +
              juce::String::repeatedString("]", 10000) + "}"}) {
        write(external, hostile);
        require(bank.importFile(external, id).failed(), "Reject trailing/deep/overflow JSON");
        require(bank.presets().size() == initial, "Hostile JSON leaves bank unchanged");
    }
    // Replace directly regardless of JUCE whitespace choice.
    auto parsed = juce::JSON::parse(jsonFor(settings));
    parsed.getDynamicObject()
        ->getProperty("parameters")
        .getDynamicObject()
        ->setProperty("inputDb", true);
    write(external, juce::JSON::toString(parsed, true));
    require(bank.importFile(external, id).failed(), "Reject bool numeric parameter");
    parsed = juce::JSON::parse(jsonFor(settings));
    parsed.getDynamicObject()
        ->getProperty("parameters")
        .getDynamicObject()
        ->setProperty("inputDb", 1000.0);
    write(external, juce::JSON::toString(parsed, true));
    require(bank.importFile(external, id).failed(), "Reject JSON range");
    write(external, juce::String::repeatedString(" ", 1024 * 1024 + 1));
    require(bank.importFile(external, id).failed(), "Reject oversized JSON");
    auto v2 = darkvoice::PresetBank::defaults();
    for (auto it = v2.begin(); it != v2.end();)
        if (juce::String(it->first).startsWith("space"))
            it = v2.erase(it);
        else
            ++it;
    for (auto it = v2.begin(); it != v2.end();)
        if (juce::String(it->first).startsWith("doubler"))
            it = v2.erase(it);
        else
            ++it;
    for (auto it = v2.begin(); it != v2.end();)
        if (juce::String(it->first).startsWith("stereoDelay"))
            it = v2.erase(it);
        else
            ++it;
    for (const auto *key : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v2.erase(key);
    for (const auto *key : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v2.erase(key);
    v2.erase("gateThresholdDb");
    v2.erase("gateRatio");
    v2.erase("gateReleaseMs");
    v2.erase("deEssHz");
    v2.erase("deEssThresholdDb");
    v2.erase("deEssReductionDb");
    for (const auto *id : {"chorusRateHz", "chorusDepthMs", "chorusMix", "stereoWidth"})
        v2.erase(id);
    v2["eqMidGainDb"] = 4.0f;
    write(external, jsonFor(v2, 2, "Version Two"));
    good(bank.importFile(external, id), "Migrate v2 preset");
    require(bank.find(id)->values.at("gateRatio") == 1 &&
                bank.find(id)->values.at("gateThresholdDb") == -50 &&
                bank.find(id)->values.at("eqMidGainDb") == 4,
            "V2 preset preserves effects and disables gate");

    auto v3 = darkvoice::PresetBank::defaults();
    for (auto it = v3.begin(); it != v3.end();)
        if (juce::String(it->first).startsWith("space"))
            it = v3.erase(it);
        else
            ++it;
    for (auto it = v3.begin(); it != v3.end();)
        if (juce::String(it->first).startsWith("doubler"))
            it = v3.erase(it);
        else
            ++it;
    for (auto it = v3.begin(); it != v3.end();)
        if (juce::String(it->first).startsWith("stereoDelay"))
            it = v3.erase(it);
        else
            ++it;
    for (const auto *key : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v3.erase(key);
    for (const auto *key : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v3.erase(key);
    v3.erase("deEssHz");
    v3.erase("deEssThresholdDb");
    v3.erase("deEssReductionDb");
    for (const auto *id : {"chorusRateHz", "chorusDepthMs", "chorusMix", "stereoWidth"})
        v3.erase(id);
    v3["gateRatio"] = 4;
    write(external, jsonFor(v3, 3, "Version Three"));
    good(bank.importFile(external, id), "Migrate v3 preset");
    require(bank.find(id)->values.at("deEssReductionDb") == 0 &&
                bank.find(id)->values.at("deEssHz") == 6000 &&
                bank.find(id)->values.at("deEssThresholdDb") == -24 &&
                bank.find(id)->values.at("gateRatio") == 4,
            "V3 preset retains gate and disables de-esser");
    auto v4 = darkvoice::PresetBank::defaults();
    for (auto it = v4.begin(); it != v4.end();)
        if (juce::String(it->first).startsWith("space"))
            it = v4.erase(it);
        else
            ++it;
    for (auto it = v4.begin(); it != v4.end();)
        if (juce::String(it->first).startsWith("doubler"))
            it = v4.erase(it);
        else
            ++it;
    for (auto it = v4.begin(); it != v4.end();)
        if (juce::String(it->first).startsWith("stereoDelay"))
            it = v4.erase(it);
        else
            ++it;
    for (const auto *key : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v4.erase(key);
    for (const auto *key : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v4.erase(key);
    for (const auto *key : {"chorusRateHz", "chorusDepthMs", "chorusMix", "stereoWidth"})
        v4.erase(key);
    v4["deEssReductionDb"] = 9;
    write(external, jsonFor(v4, 4, "Version Four"));
    good(bank.importFile(external, id), "Migrate v4 preset");
    require(bank.find(id)->values.at("chorusMix") == 0 &&
                bank.find(id)->values.at("stereoWidth") == 1 &&
                bank.find(id)->values.at("chorusRateHz") == .8f &&
                bank.find(id)->values.at("chorusDepthMs") == 6 &&
                bank.find(id)->values.at("deEssReductionDb") == 9,
            "V4 preset retains de-esser and neutral modulation");
    auto v5 = darkvoice::PresetBank::defaults();
    for (auto it = v5.begin(); it != v5.end();)
        if (juce::String(it->first).startsWith("space"))
            it = v5.erase(it);
        else
            ++it;
    for (auto it = v5.begin(); it != v5.end();)
        if (juce::String(it->first).startsWith("doubler"))
            it = v5.erase(it);
        else
            ++it;
    for (auto it = v5.begin(); it != v5.end();)
        if (juce::String(it->first).startsWith("stereoDelay"))
            it = v5.erase(it);
        else
            ++it;
    for (const auto *key : {"limiterEnabled", "limiterCeilingDb", "limiterReleaseMs"})
        v5.erase(key);
    for (const auto *key : {"flangerRateHz", "flangerDepthMs", "flangerFeedback", "flangerMix"})
        v5.erase(key);
    v5["chorusMix"] = .25f;
    write(external, jsonFor(v5, 5, "Version Five"));
    good(bank.importFile(external, id), "Migrate v5 preset");
    require(bank.find(id)->values.at("flangerMix") == 0 &&
                bank.find(id)->values.at("flangerFeedback") == 0 &&
                bank.find(id)->values.at("flangerDepthMs") == 2 &&
                bank.find(id)->values.at("flangerRateHz") == .3f &&
                bank.find(id)->values.at("chorusMix") == .25f,
            "V5 preset retains chorus and resets flanger defaults");
    darkvoice::PresetValues legacy;
    for (std::size_t i = 0; i < darkvoice::legacyParameterCount; ++i) {
        const auto &spec = darkvoice::parameterSchema[i];
        if (!spec.boolean)
            legacy[spec.id] = spec.defaultValue;
    }
    legacy["drive"] = .3f;
    write(external, jsonFor(legacy, 1, "Legacy"));
    good(bank.importFile(external, id), "Migrate legacy preset");
    require(bank.find(id)->values.size() == 54 && bank.find(id)->values.at("delayMix") == 0 &&
                bank.find(id)->values.at("drive") == .3f,
            "Migration preserves old controls and new defaults");
    {
        darkvoice::PresetBank migration(temp.file.getChildFile("limiter-migration"));
        for (int version = 1; version <= 6; ++version) {
            darkvoice::PresetValues old;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                if (juce::String(spec.id) != "bypass")
                    old[spec.id] = spec.defaultValue;
            }
            old["inputDb"] = 5.0f;
            if (version == 6)
                old["flangerMix"] = .6f;
            write(external, jsonFor(old, version, "Limiter migration"));
            juce::String migratedId;
            good(migration.importFile(external, migratedId), "Migrate exact legacy preset into v7");
            const auto &migrated = migration.find(migratedId)->values;
            require(migrated.size() == 54 && migrated.at("limiterEnabled") == 0 &&
                        migrated.at("limiterCeilingDb") == -1 &&
                        migrated.at("limiterReleaseMs") == 100 && migrated.at("inputDb") == 5,
                    "All legacy presets preserve old values and disable limiter");
            if (version == 6)
                require(migrated.at("flangerMix") == .6f, "V6 preset retains flanger settings");
        }
        for (float invalid : {.5f, -1.f, 2.f, std::numeric_limits<float>::quiet_NaN()}) {
            auto bad = darkvoice::PresetBank::defaults();
            bad["limiterEnabled"] = invalid;
            juce::String created;
            require(darkvoice::PresetBank::validate(bad).failed() &&
                        migration.saveAs("Invalid limiter", bad, created).failed(),
                    "Invalid preset limiter boolean rejected");
        }
        for (const auto &invalid : {juce::var(.5), juce::var(true), juce::var("1")}) {
            auto parsed = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
            parsed.getDynamicObject()
                ->getProperty("parameters")
                .getDynamicObject()
                ->setProperty("limiterEnabled", invalid);
            write(external, juce::JSON::toString(parsed, true));
            juce::String created;
            const auto count = migration.presets().size();
            require(migration.importFile(external, created).failed() &&
                        migration.presets().size() == count,
                    "Invalid JSON limiter boolean rejected atomically");
        }
    }
    {
        darkvoice::PresetBank migrations(temp.file.getChildFile("stereo-delay-migration"));
        for (int version = 1; version <= 7; ++version) {
            darkvoice::PresetValues old;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                if (juce::String(spec.id) != "bypass")
                    old[spec.id] = spec.defaultValue;
            }
            old["inputDb"] = 6;
            if (version == 7)
                old["limiterEnabled"] = 1;
            write(external, jsonFor(old, version, "Delay migration"));
            juce::String migrated;
            good(migrations.importFile(external, migrated),
                 "Import strict pre-stereo-delay preset");
            const auto &result = migrations.find(migrated)->values;
            require(result.size() == 54 && result.at("inputDb") == 6,
                    "Legacy delay migration preserves prior fields");
            for (std::size_t i = 38; i < darkvoice::parameterSchema.size(); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                require(result.at(spec.id) == spec.defaultValue,
                        "Every old preset initializes eight delay defaults");
            }
            if (version == 7)
                require(result.at("limiterEnabled") == 1, "V7 migration retains limiter");
        }
        for (const auto *field : {"stereoDelaySync", "stereoDelayHostTempo", "stereoDelayMode",
                                  "stereoDelayDivision"}) {
            auto values = darkvoice::PresetBank::defaults();
            values[field] = .5f;
            require(darkvoice::PresetBank::validate(values).failed(),
                    "Fractional delay choice/bool rejected in memory");
            for (const auto *token : {"0.5", "1.00000001", "1e-500", "-1", "6", "true", "\"1\""}) {
                auto parsed = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
                parsed.getDynamicObject()
                    ->getProperty("parameters")
                    .getDynamicObject()
                    ->setProperty(field, "invalid-delay-token");
                write(external,
                      juce::JSON::toString(parsed, true).replace("\"invalid-delay-token\"", token));
                juce::String created;
                const auto count = migrations.presets().size();
                require(migrations.importFile(external, created).failed() &&
                            migrations.presets().size() == count,
                        "Raw delay Boolean/discrete JSON rejected before rounding");
            }
        }
    }
    {
        darkvoice::PresetBank migrations(temp.file.getChildFile("doubler-migration"));
        for (int version = 1; version <= 8; ++version) {
            darkvoice::PresetValues old;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                if (juce::String(spec.id) != "bypass")
                    old[spec.id] = spec.defaultValue;
            }
            old["inputDb"] = 6;
            if (version == 8)
                old["stereoDelayMix"] = .3f;
            write(external, jsonFor(old, version, "Doubler migration"));
            juce::String created;
            good(migrations.importFile(external, created), "Migrate exact pre-doubler preset");
            const auto &values = migrations.find(created)->values;
            require(values.size() == 54 && values.at("inputDb") == 6,
                    "Doubler migration preserves prior fields");
            for (std::size_t i = 46; i < darkvoice::parameterSchema.size(); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                require(values.at(spec.id) == spec.defaultValue,
                        "Every legacy preset initializes four neutral doubler controls");
            }
            if (version == 8)
                require(values.at("stereoDelayMix") == .3f,
                        "V8 preset retains stereo musical delay");
        }
        for (const auto *field :
             {"doublerSeparationMs", "doublerVariationMs", "doublerRateHz", "doublerMix"})
            for (float invalid : {-1.f, 100.f, std::numeric_limits<float>::quiet_NaN()}) {
                auto values = darkvoice::PresetBank::defaults();
                values[field] = invalid;
                require(darkvoice::PresetBank::validate(values).failed(),
                        "Invalid doubler preset values rejected");
            }
    }
    {
        darkvoice::PresetBank migrations(temp.file.getChildFile("space-reverb-migration"));
        for (int version = 1; version <= 9; ++version) {
            darkvoice::PresetValues old;
            for (std::size_t i = 0; i < darkvoice::parameterCountForVersion(version); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                if (juce::String(spec.id) != "bypass")
                    old[spec.id] = spec.defaultValue;
            }
            old["inputDb"] = 6;
            if (version == 9)
                old["doublerMix"] = .3f;
            write(external, jsonFor(old, version, "Space migration"));
            juce::String created;
            good(migrations.importFile(external, created), "Migrate exact pre-Space preset");
            const auto &values = migrations.find(created)->values;
            require(values.size() == 54 && values.at("inputDb") == 6,
                    "Space migration preserves prior fields");
            for (std::size_t i = 50; i < darkvoice::parameterSchema.size(); ++i) {
                const auto &spec = darkvoice::parameterSchema[i];
                require(values.at(spec.id) == spec.defaultValue,
                        "Every legacy preset initializes neutral Space fields");
            }
            if (version == 9)
                require(values.at("doublerMix") == .3f, "V9 preset retains simulated doubler");
        }
        for (const auto *field : {"spaceDecay", "spaceToneHz", "spacePreDelayMs", "spaceMix"})
            for (float invalid : {-1.f, 100000.f, std::numeric_limits<float>::quiet_NaN()}) {
                auto values = darkvoice::PresetBank::defaults();
                values[field] = invalid;
                require(darkvoice::PresetBank::validate(values).failed(),
                        "Invalid Space float preset rejected");
            }
        for (const auto *token : {"0.5", "1.00000001", "1e-500", "3", "-1", "true", "\"1\""}) {
            auto parsed = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
            parsed.getDynamicObject()
                ->getProperty("parameters")
                .getDynamicObject()
                ->setProperty("spaceModel", "invalid-space-token");
            write(external,
                  juce::JSON::toString(parsed, true).replace("\"invalid-space-token\"", token));
            juce::String created;
            const auto count = migrations.presets().size();
            require(migrations.importFile(external, created).failed() &&
                        migrations.presets().size() == count,
                    "Invalid Space model raw JSON rejected before rounding");
        }
        auto invalid = darkvoice::PresetBank::defaults();
        invalid["spaceModel"] = .5f;
        require(darkvoice::PresetBank::validate(invalid).failed(),
                "Fractional Space model map rejected");
    }
    // Emit raw literals: JUCE's JSON writer can round very small doubles to zero.
    for (const auto *invalid :
         {"1e-50", "1.00000001", "0.99999999", "1e-500", "1.00000000000000000001"}) {
        auto parsed = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
        parsed.getDynamicObject()
            ->getProperty("parameters")
            .getDynamicObject()
            ->setProperty("limiterEnabled", "invalid-boolean-token");
        write(external,
              juce::JSON::toString(parsed, true).replace("\"invalid-boolean-token\"", invalid));
        juce::String created;
        const auto count = bank.presets().size();
        require(bank.importFile(external, created).failed() && bank.presets().size() == count,
                "Fractional JSON booleans rejected before float rounding");
    }
    for (const auto *valid : {"0.0", "1.0", "10e-1", "0e-500"}) {
        auto parsed = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
        parsed.getDynamicObject()
            ->getProperty("parameters")
            .getDynamicObject()
            ->setProperty("limiterEnabled", "boolean-token");
        write(external, juce::JSON::toString(parsed, true).replace("\"boolean-token\"", valid));
        darkvoice::PresetBank accepted(
            temp.file.getChildFile("valid-number-" + juce::Uuid().toString()));
        juce::String created;
        require(accepted.importFile(external, created).wasOk() &&
                    accepted.find(created)->values.at("limiterEnabled") ==
                        (juce::String(valid).startsWithChar('0') ? 0.0f : 1.0f),
                "Exact decimal and scientific Boolean values remain accepted");
    }
    const auto broken = temp.file.getChildFile("bank").getChildFile("broken.json");
    write(broken, "broken");
    require(bank.refresh().failed(), "Refresh reports malformed local file");
    require(bank.find(id) != nullptr && bank.presets().size() == 20,
            "Refresh retains valid files and factories");
    require(broken.loadFileAsString() == "broken", "Malformed user file preserved");
    const auto favorites = temp.file.getChildFile("bank").getChildFile(".favorites.json");
    write(favorites, "corrupt metadata");
    require(bank.setFavorite(id, true).failed(), "Corrupt favorites not overwritten");
    require(favorites.loadFileAsString() == "corrupt metadata", "Favorites file preserved");
}
void hostilePathsAndLimits() {
    TemporaryDirectory temp;
    const auto outside = temp.file.getChildFile("outside");
    good(outside.createDirectory(), "Outside fixture");
    const auto link = temp.file.getChildFile("link");
    std::filesystem::create_directory_symlink(outside.getFullPathName().toStdString(),
                                              link.getFullPathName().toStdString());
    darkvoice::PresetBank unsafe(link.getChildFile("nested"));
    juce::String id;
    require(unsafe.saveAs("Unsafe", darkvoice::PresetBank::defaults(), id).failed(),
            "Reject symlink ancestor");
    require(!outside.getChildFile("nested").exists(), "No write outside preset root");
    darkvoice::PresetBank bank(temp.file.getChildFile("bank"));
    const auto external = temp.file.getChildFile("fixture.json");
    write(external, jsonFor(darkvoice::PresetBank::defaults()));
    const auto sourceLink = temp.file.getChildFile("source-link.json");
    std::filesystem::create_symlink(external.getFullPathName().toStdString(),
                                    sourceLink.getFullPathName().toStdString());
    require(bank.importFile(sourceLink, id).failed(), "Reject symlink import");
    const auto factory = bank.presets().front().id;
    require(bank.exportFile(factory, sourceLink).failed(), "Reject symlink export");
    require(external.loadFileAsString() == jsonFor(darkvoice::PresetBank::defaults()),
            "Symlink target untouched");
    auto invalidId = juce::JSON::parse(jsonFor(darkvoice::PresetBank::defaults()));
    invalidId.getDynamicObject()->setProperty("id", "../../outside");
    write(external, juce::JSON::toString(invalidId, true));
    require(bank.importFile(external, id).failed(), "Reject malicious imported id");
    const auto uuid = juce::Uuid().toString();
    const auto userLink = temp.file.getChildFile("bank").getChildFile(uuid + ".json");
    std::filesystem::create_symlink(external.getFullPathName().toStdString(),
                                    userLink.getFullPathName().toStdString());
    require(bank.rename(uuid, "Changed").failed(), "Reject symlink user rename");
    require(bank.refresh().failed(), "Refresh reports symlink user file");
    require(bank.presets().size() == 15, "Symlink preset not loaded");
    const auto capped = temp.file.getChildFile("capped");
    good(capped.createDirectory(), "Cap fixture root");
    for (int i = 0; i < 512; ++i)
        write(capped.getChildFile(juce::String(i) + ".json"), "{}");
    darkvoice::PresetBank full(capped);
    require(full.saveAs("Extra", darkvoice::PresetBank::defaults(), id).failed(),
            "Bound user file count");
    require(capped.findChildFiles(juce::File::findFiles, false, "*.json").size() == 512,
            "Cap does not discard existing files");
}
struct Comma : std::numpunct<char> {
    char do_decimal_point() const override {
        return ',';
    }
};
struct CommaLocale {
    std::locale old = std::locale::global(std::locale(std::locale::classic(), new Comma));
    ~CommaLocale() {
        std::locale::global(old);
    }
};
void localeRoundtrip() {
    TemporaryDirectory temp;
    darkvoice::PresetBank bank(temp.file.getChildFile("bank"));
    auto settings = darkvoice::PresetBank::defaults();
    settings["inputDb"] = -3.5f;
    juce::String id;
    {
        CommaLocale locale;
        good(bank.saveAs("Locale", settings, id), "Save locale-independent JSON");
    }
    darkvoice::PresetBank restored(temp.file.getChildFile("bank"));
    good(restored.refresh(), "Read locale-independent JSON");
    require(restored.find(id) && restored.find(id)->values.at("inputDb") == -3.5f,
            "Locale sound roundtrip");
}
} // namespace
int main() {
    try {
        basicPersistence();
        malformedAndMigration();
        hostilePathsAndLimits();
        localeRoundtrip();
        std::cout << "Preset checks: " << checks << ", failures: 0\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
