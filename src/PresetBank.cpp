#include "PresetBank.hpp"
#include "BooleanNumber.hpp"
#include <algorithm>
#include <cmath>
#include <darkvoice/ParameterSchema.hpp>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>

namespace darkvoice {
namespace {
constexpr juce::int64 maximumBytes = 1024 * 1024;
constexpr int maximumUsers = 512;
const juce::String favoritesFile = ".favorites.json";
juce::Result fail(const juce::String &message) {
    return juce::Result::fail(message);
}
bool safePath(const juce::File &file) {
    std::filesystem::path current;
    for (const auto &component : std::filesystem::path(file.getFullPathName().toStdString())) {
        current /= component;
        std::error_code ec;
        const auto status = std::filesystem::symlink_status(current, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
            return false;
        if (std::filesystem::is_symlink(status))
            return false;
    }
    return true;
}
juce::Result ready(const juce::File &directory) {
    if (!safePath(directory))
        return fail("Preset directory contains a symbolic link or is inaccessible");
    const auto result = directory.createDirectory();
    if (result.failed())
        return result;
    return directory.isDirectory() ? juce::Result::ok() : fail("Preset root is not a directory");
}
bool validName(const juce::String &name) {
    if (name.isEmpty() || name.length() > 80 || name != name.trim())
        return false;
    for (auto c : name)
        if (c < 32 || c == 127)
            return false;
    return true;
}
bool uuidId(const juce::String &id) {
    return id.length() == 32 && id.containsOnly("0123456789abcdef");
}
bool acceptableId(const juce::String &id) {
    return uuidId(id) || (id.startsWith("factory-") && id.length() < 80 &&
                          id.containsOnly("abcdefghijklmnopqrstuvwxyz0123456789-"));
}
bool numeric(const juce::var &value) {
    return value.isInt() || value.isInt64() || value.isDouble();
}
// Reject duplicate keys and validate Boolean number tokens before JUCE numeric rounding.
bool uniqueKeys(const juce::String &source) {
    const auto text = source.toStdString();
    struct Scope {
        char type;
        std::set<std::string> keys;
    };
    std::vector<Scope> scopes;
    bool started = false, complete = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        const char c = text[i];
        if (std::isspace(byte))
            continue;
        if (complete)
            return false;
        if (!started) {
            if (c != '{')
                return false;
            started = true;
        }
        if (c == '{' || c == '[') {
            if (scopes.size() >= 32)
                return false;
            scopes.push_back({c, {}});
            continue;
        }
        if (c == '}' || c == ']') {
            if (scopes.empty() ||
                (c == '}' ? scopes.back().type != '{' : scopes.back().type != '['))
                return false;
            scopes.pop_back();
            complete = scopes.empty();
            continue;
        }
        if (c != '"')
            continue;
        const auto begin = i++;
        bool escaped = false;
        for (; i < text.size(); ++i) {
            if (static_cast<unsigned char>(text[i]) < 32)
                return false;
            if (!escaped && text[i] == '"')
                break;
            if (!escaped && text[i] == '\\')
                escaped = true;
            else
                escaped = false;
        }
        if (i == text.size())
            return false;
        auto after = i + 1;
        while (after < text.size() && std::isspace(static_cast<unsigned char>(text[after])))
            ++after;
        if (after < text.size() && text[after] == ':' && !scopes.empty() &&
            scopes.back().type == '{') {
            const auto quoted =
                juce::String::fromUTF8(text.data() + begin, static_cast<int>(i - begin + 1));
            const auto decodedKey = juce::JSON::fromString(quoted);
            if (!decodedKey.isString())
                return false;
            const auto key = decodedKey.toString().toStdString();
            if (!scopes.back().keys.insert(key).second)
                return false;
            if (key == "limiterEnabled") {
                auto token = after + 1;
                while (token < text.size() && std::isspace(static_cast<unsigned char>(text[token])))
                    ++token;
                auto end = token;
                while (end < text.size() && text[end] != ',' && text[end] != '}' &&
                       text[end] != ']' && !std::isspace(static_cast<unsigned char>(text[end])))
                    ++end;
                if (!exactBooleanNumber(std::string_view(text).substr(token, end - token)))
                    return false;
            }
        }
    }
    return started && complete && scopes.empty();
}
juce::Result loadJson(const juce::File &source, juce::var &parsed) {
    if (!safePath(source) || !source.existsAsFile())
        return fail("Preset source is missing or contains a symbolic link");
    if (source.getSize() <= 0 || source.getSize() > maximumBytes)
        return fail("Preset file size is invalid");
    auto input = source.createInputStream();
    if (!input)
        return fail("Unable to open preset source");
    juce::MemoryBlock data;
    const auto count = input->readIntoMemoryBlock(data, maximumBytes + 1);
    if (count <= 0 || count > maximumBytes)
        return fail("Preset file changed or exceeds size limit");
    const auto text =
        juce::String::createStringFromData(data.getData(), static_cast<int>(data.getSize()));
    if (!uniqueKeys(text))
        return fail("Duplicate or malformed JSON properties");
    const auto result = juce::JSON::parse(text, parsed);
    if (result.failed() || !parsed.isObject())
        return fail("Invalid preset JSON object");
    return juce::Result::ok();
}
std::string quote(const juce::String &text) {
    return juce::JSON::toString(juce::var(text), true).toStdString();
}
juce::String encode(const Preset &preset) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "{\"schemaVersion\":" << currentSchemaVersion << ",\"id\":" << quote(preset.id)
           << ",\"name\":" << quote(preset.name) << ",\"parameters\":{";
    bool first = true;
    for (const auto &[id, value] : preset.values) {
        if (!first)
            stream << ',';
        first = false;
        stream << quote(juce::String(id)) << ':' << value;
    }
    stream << "}}\n";
    return juce::String::fromUTF8(stream.str().c_str());
}
juce::Result decode(const juce::File &file, Preset &preset, bool local) {
    juce::var parsed;
    if (auto result = loadJson(file, parsed); result.failed())
        return result;
    const auto *object = parsed.getDynamicObject();
    for (const auto &property : object->getProperties())
        if (property.name.toString() != "schemaVersion" && property.name.toString() != "name" &&
            property.name.toString() != "id" && property.name.toString() != "parameters")
            return fail("Unknown preset property");
    const auto version = object->getProperty("schemaVersion");
    if (!(version.isInt() || version.isInt64()) ||
        (static_cast<juce::int64>(version) < 1 ||
         static_cast<juce::int64>(version) > currentSchemaVersion))
        return fail("Unsupported preset schema version");
    const auto name = object->getProperty("name");
    if (!name.isString() || !validName(name.toString().trim()))
        return fail("Invalid preset name");
    const auto id = object->getProperty("id");
    if ((!id.isVoid() && (!id.isString() || !acceptableId(id.toString()))) ||
        (local && !uuidId(id.toString())))
        return fail("Invalid preset identifier");
    if (local && file.getFileNameWithoutExtension() != id.toString())
        return fail("Preset identifier does not match filename");
    const auto parameters = object->getProperty("parameters");
    const auto *values = parameters.getDynamicObject();
    const auto count = parameterCountForVersion(static_cast<int>(version));
    const int expected = static_cast<int>(count - 1);
    if (!values || values->getProperties().size() != expected)
        return fail("Missing or extra preset parameters");
    auto candidate = PresetBank::defaults();
    std::size_t position = 0;
    for (const auto &spec : parameterSchema) {
        const auto index = position++;
        if (juce::String(spec.id) == "bypass" || index >= count)
            continue;
        const auto value = values->getProperty(spec.id);
        if (!numeric(value))
            return fail("Preset parameter is not numeric");
        const auto number = static_cast<double>(value);
        constexpr double floatLimit = std::numeric_limits<float>::max();
        if (!std::isfinite(number) || number < -floatLimit || number > floatLimit ||
            (spec.boolean && number != 0.0 && number != 1.0))
            return fail("Preset parameter outside range");
        // Controls use float values: decimal 0.1 must be accepted at a 0.1f bound.
        const float represented = static_cast<float>(number);
        if (!std::isfinite(represented) || represented < spec.minimum ||
            represented > spec.maximum ||
            (spec.boolean && represented != 0.0f && represented != 1.0f))
            return fail("Preset parameter outside range");
        candidate[spec.id] = represented;
    }
    if (auto result = PresetBank::validate(candidate); result.failed())
        return result;
    preset = {id.toString(), name.toString().trim(), false, false, std::move(candidate)};
    return juce::Result::ok();
}
juce::Result atomicWrite(const juce::File &file, const juce::String &text, bool replace) {
    if (!safePath(file) || !safePath(file.getParentDirectory()))
        return fail("Unsafe preset destination");
    if (file.exists() && (!replace || !file.existsAsFile()))
        return fail("Destination already exists");
    juce::TemporaryFile temporary(file);
    if (!safePath(temporary.getFile()) || !temporary.getFile().replaceWithText(text))
        return fail("Unable to write temporary preset");
    if ((!replace && file.exists()) || !safePath(file))
        return fail("Destination changed during write");
    if (!replace) {
        // Atomic no-replace commit: a newly-created destination is never overwritten.
        // Fail safely on filesystems that do not support same-volume hard links.
        std::error_code ec;
        std::filesystem::create_hard_link(temporary.getFile().getFullPathName().toStdString(),
                                          file.getFullPathName().toStdString(), ec);
        return ec ? fail("Unable to create preset exclusively: " + juce::String(ec.message()))
                  : juce::Result::ok();
    }
    return temporary.overwriteTargetFileWithTemporary()
               ? juce::Result::ok()
               : fail("Unable to replace preset atomically");
}
std::vector<Preset> factories() {
    std::vector<Preset> presets;
    const auto add = [&](const char *id, const char *name,
                         std::initializer_list<std::pair<const char *, float>> changes) {
        Preset preset{juce::String("factory-") + id, name, true, false, PresetBank::defaults()};
        for (const auto &[key, value] : changes)
            preset.values[key] = value;
        presets.push_back(std::move(preset));
    };
    add("dark-synth", "Dark Synth Baritone",
        {{"highPassHz", 65},
         {"drive", .2f},
         {"ratio", 2.5f},
         {"eqLowGainDb", 2},
         {"eqMidGainDb", -2},
         {"eqHighGainDb", -3},
         {"delayMs", 280},
         {"delayMix", .12f},
         {"reverbMix", .1f}});
    add("industrial-bass", "Industrial Bass",
        {{"highPassHz", 55},
         {"drive", .65f},
         {"ratio", 4},
         {"thresholdDb", -22},
         {"eqLowGainDb", 3},
         {"eqMidHz", 1800},
         {"eqMidGainDb", 2},
         {"delayMs", 100},
         {"delayMix", .06f}});
    add("heavy-rock", "Heavy Rock Baritone",
        {{"highPassHz", 80},
         {"drive", .4f},
         {"ratio", 3.5f},
         {"eqLowGainDb", -1},
         {"eqMidHz", 2400},
         {"eqMidGainDb", 3},
         {"eqHighGainDb", 1},
         {"delayMs", 90},
         {"delayMix", .08f},
         {"reverbMix", .06f}});
    add("clean-studio", "Clean Studio Baritone",
        {{"highPassHz", 70},
         {"ratio", 2},
         {"thresholdDb", -16},
         {"eqMidHz", 3000},
         {"eqMidGainDb", 1.5f},
         {"eqHighGainDb", 1},
         {"reverbMix", .03f}});
    add("gothic-bass", "Gothic Bass",
        {{"highPassHz", 50},
         {"drive", .15f},
         {"ratio", 2.2f},
         {"eqLowGainDb", 2.5f},
         {"eqHighGainDb", -4},
         {"delayMs", 400},
         {"delayMix", .1f},
         {"reverbSize", .8f},
         {"reverbMix", .25f}});
    add("cinematic-bass", "Cinematic Bass",
        {{"highPassHz", 45},
         {"drive", .1f},
         {"ratio", 2},
         {"eqLowGainDb", 3},
         {"eqMidGainDb", -1},
         {"eqHighGainDb", -2},
         {"reverbSize", .9f},
         {"reverbMix", .3f},
         {"delayMs", 520},
         {"delayMix", .08f}});
    add("dark-wave", "Dark Wave",
        {{"highPassHz", 60},
         {"drive", 0.12f},
         {"eqLowGainDb", 2},
         {"eqHighGainDb", -1},
         {"chorusMix", 0.16f},
         {"delayMs", 380},
         {"delayMix", 0.12f},
         {"reverbMix", 0.18f},
         {"stereoWidth", 1.2f}});
    add("deep-narrator", "Deep Narrator",
        {{"highPassHz", 45},
         {"eqLowGainDb", 2},
         {"eqMidHz", 600},
         {"eqMidGainDb", -1.5f},
         {"drive", 0.1f},
         {"ratio", 2.5f},
         {"thresholdDb", -20},
         {"reverbMix", 0.015f}});
    add("industrial-whisper", "Industrial Whisper",
        {{"highPassHz", 100},
         {"eqLowGainDb", -2},
         {"eqMidHz", 2000},
         {"eqMidGainDb", 2},
         {"eqHighGainDb", 3},
         {"drive", 0.25f},
         {"thresholdDb", -30},
         {"ratio", 4},
         {"deEssReductionDb", 5},
         {"reverbMix", 0.06f}});
    add("grunge-baritone", "Grunge Baritone",
        {{"highPassHz", 75},
         {"drive", 0.55f},
         {"ratio", 3},
         {"thresholdDb", -24},
         {"eqMidHz", 1100},
         {"eqMidGainDb", 2},
         {"eqHighGainDb", -1},
         {"reverbMix", 0.06f},
         {"delayMs", 90},
         {"delayMix", 0.055f}});
    add("acoustic-baritone", "Acoustic Baritone",
        {{"highPassHz", 70},
         {"ratio", 1.8f},
         {"thresholdDb", -20},
         {"eqLowGainDb", -1},
         {"eqMidHz", 1600},
         {"eqMidGainDb", 1},
         {"eqHighGainDb", 1.5f},
         {"reverbMix", 0.07f}});
    add("dark-melodic", "Dark Melodic",
        {{"eqLowGainDb", 2},
         {"eqMidHz", 1000},
         {"eqMidGainDb", -1},
         {"eqHighGainDb", -1.5f},
         {"chorusMix", 0.1f},
         {"reverbMix", 0.14f},
         {"delayMs", 300},
         {"delayMix", 0.12f}});
    add("industrial-power", "Industrial Power",
        {{"drive", 0.75f},
         {"eqLowGainDb", 1.5f},
         {"eqMidHz", 2600},
         {"eqMidGainDb", 3.5f},
         {"eqHighGainDb", 1},
         {"ratio", 5},
         {"thresholdDb", -24},
         {"deEssReductionDb", 6},
         {"delayMs", 110},
         {"delayMix", 0.08f},
         {"reverbMix", 0.05f},
         {"flangerMix", 0.1f},
         {"flangerFeedback", 0.25f}});
    add("natural-dark-baritone", "Natural Dark Baritone",
        {{"drive", 0.04f},
         {"ratio", 1.6f},
         {"thresholdDb", -20},
         {"eqLowGainDb", 1.5f},
         {"eqMidHz", 900},
         {"eqMidGainDb", -0.5f},
         {"eqHighGainDb", -0.5f},
         {"reverbMix", 0.04f}});
    return presets;
}
juce::Result readFavorites(const juce::File &directory, std::set<std::string> &ids) {
    ids.clear();
    const auto file = directory.getChildFile(favoritesFile);
    if (!file.exists() && !file.isSymbolicLink())
        return juce::Result::ok();
    juce::var parsed;
    if (auto result = loadJson(file, parsed); result.failed())
        return result;
    const auto *object = parsed.getDynamicObject();
    if (object->getProperties().size() != 2 || !object->getProperty("schemaVersion").isInt() ||
        static_cast<int>(object->getProperty("schemaVersion")) != 1)
        return fail("Invalid favorites metadata");
    const auto *entries = object->getProperty("ids").getArray();
    if (!entries || entries->size() > maximumUsers + 6)
        return fail("Invalid favorites list");
    for (const auto &id : *entries)
        if (!id.isString() || !acceptableId(id.toString()) ||
            !ids.insert(id.toString().toStdString()).second)
            return fail("Invalid favorite identifier");
    return juce::Result::ok();
}
juce::String encodeFavorites(const std::set<std::string> &ids) {
    std::string result = "{\"schemaVersion\":1,\"ids\":[";
    bool first = true;
    for (const auto &id : ids) {
        if (!first)
            result += ',';
        first = false;
        result += quote(juce::String(id));
    }
    result += "]}\n";
    return juce::String::fromUTF8(result.c_str());
}
} // namespace
PresetBank::PresetBank(juce::File directory)
    : directory_(std::move(directory)), presets_(factories()) {
    (void)refresh();
}
PresetValues PresetBank::defaults() {
    PresetValues values;
    for (const auto &spec : parameterSchema)
        if (juce::String(spec.id) != "bypass")
            values[spec.id] = spec.defaultValue;
    return values;
}
juce::Result PresetBank::validate(const PresetValues &values) {
    if (values.size() != parameterSchema.size() - 1)
        return fail("Preset must contain exactly the current non-bypass sound parameters");
    for (const auto &spec : parameterSchema) {
        if (juce::String(spec.id) == "bypass")
            continue;
        const auto value = values.find(spec.id);
        if (value == values.end() || !std::isfinite(value->second) ||
            value->second < spec.minimum || value->second > spec.maximum ||
            (spec.boolean && value->second != 0.0f && value->second != 1.0f))
            return fail("Invalid preset sound parameter");
    }
    return juce::Result::ok();
}
juce::Result PresetBank::refresh() {
    auto updated = factories();
    favorites_.clear();
    if (auto result = ready(directory_); result.failed()) {
        presets_ = std::move(updated);
        return result;
    }
    juce::StringArray errors;
    if (auto result = readFavorites(directory_, favorites_); result.failed()) {
        favorites_.clear();
        errors.add(result.getErrorMessage());
    }
    int users = 0;
    for (const auto &entry :
         juce::RangedDirectoryIterator(directory_, false, "*.json", juce::File::findFiles)) {
        const auto file = entry.getFile();
        if (file.getFileName() == favoritesFile)
            continue;
        if (++users > maximumUsers) {
            errors.add("User preset limit exceeded");
            break;
        }
        Preset preset;
        const auto result = decode(file, preset, true);
        if (result.failed()) {
            errors.add("Skipped " + file.getFileName() + ": " + result.getErrorMessage());
            continue;
        }
        updated.push_back(std::move(preset));
    }
    for (auto &preset : updated)
        preset.favorite = favorites_.contains(preset.id.toStdString());
    presets_ = std::move(updated);
    return errors.isEmpty() ? juce::Result::ok() : fail(errors.joinIntoString("; "));
}
const Preset *PresetBank::find(const juce::String &id) const noexcept {
    for (const auto &preset : presets_)
        if (preset.id == id)
            return &preset;
    return nullptr;
}
std::vector<Preset> PresetBank::search(const juce::String &query, bool favoritesOnly) const {
    std::vector<Preset> result;
    for (const auto &preset : presets_)
        if ((!favoritesOnly || preset.favorite) && preset.name.containsIgnoreCase(query.trim()))
            result.push_back(preset);
    return result;
}
juce::Result PresetBank::saveAs(const juce::String &name, const PresetValues &values,
                                juce::String &createdId) {
    createdId.clear();
    const auto trimmed = name.trim();
    if (!validName(trimmed))
        return fail("Preset name must be 1 to 80 characters without controls");
    if (auto result = validate(values); result.failed())
        return result;
    if (auto result = ready(directory_); result.failed())
        return result;
    int users = 0;
    for (const auto &entry :
         juce::RangedDirectoryIterator(directory_, false, "*.json", juce::File::findFiles)) {
        if (entry.getFile().getFileName() != favoritesFile && ++users >= maximumUsers)
            break;
    }
    if (users >= maximumUsers)
        return fail("User preset limit reached");
    const auto id = juce::Uuid().toString();
    Preset preset{id, trimmed, false, false, values};
    if (auto result = atomicWrite(directory_.getChildFile(id + ".json"), encode(preset), false);
        result.failed())
        return result;
    presets_.push_back(preset);
    createdId = id;
    return juce::Result::ok();
}
juce::Result PresetBank::rename(const juce::String &id, const juce::String &name) {
    if (!uuidId(id) || !validName(name.trim()))
        return fail("Factory preset or invalid name/identifier");
    if (auto result = ready(directory_); result.failed())
        return result;
    Preset preset;
    const auto file = directory_.getChildFile(id + ".json");
    if (auto result = decode(file, preset, true); result.failed())
        return result;
    preset.name = name.trim();
    if (auto result = atomicWrite(file, encode(preset), true); result.failed())
        return result;
    (void)refresh();
    return juce::Result::ok();
}
juce::Result PresetBank::setFavorite(const juce::String &id, bool favorite) {
    if (!find(id))
        return fail("Unknown preset");
    if (auto result = ready(directory_); result.failed())
        return result;
    std::set<std::string> updated;
    if (auto result = readFavorites(directory_, updated); result.failed())
        return result;
    if (favorite)
        updated.insert(id.toStdString());
    else
        updated.erase(id.toStdString());
    if (auto result =
            atomicWrite(directory_.getChildFile(favoritesFile), encodeFavorites(updated), true);
        result.failed())
        return result;
    favorites_ = std::move(updated);
    for (auto &preset : presets_)
        preset.favorite = favorites_.contains(preset.id.toStdString());
    return juce::Result::ok();
}
juce::Result PresetBank::importFile(const juce::File &source, juce::String &createdId) {
    Preset preset;
    createdId.clear();
    if (auto result = decode(source, preset, false); result.failed())
        return result;
    return saveAs(preset.name, preset.values, createdId);
}
juce::Result PresetBank::exportFile(const juce::String &id, const juce::File &destination) const {
    const auto *preset = find(id);
    if (!preset)
        return fail("Unknown preset");
    if (!safePath(destination.getParentDirectory()) ||
        !destination.getParentDirectory().isDirectory())
        return fail("Export directory is missing or unsafe");
    return atomicWrite(destination, encode(*preset), false);
}
} // namespace darkvoice
