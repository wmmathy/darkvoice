#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <set>
#include <string>
#include <vector>
namespace darkvoice {
using PresetValues = std::map<std::string, float>;
struct Preset {
    juce::String id, name;
    bool factory = false, favorite = false;
    PresetValues values;
};
class PresetBank {
  public:
    explicit PresetBank(juce::File directory);
    juce::Result refresh();
    const std::vector<Preset> &presets() const noexcept {
        return presets_;
    }
    const Preset *find(const juce::String &id) const noexcept;
    std::vector<Preset> search(const juce::String &query, bool favoritesOnly = false) const;
    static PresetValues defaults();
    static juce::Result validate(const PresetValues &);
    juce::Result saveAs(const juce::String &name, const PresetValues &, juce::String &createdId);
    juce::Result rename(const juce::String &id, const juce::String &name);
    juce::Result setFavorite(const juce::String &id, bool favorite);
    juce::Result importFile(const juce::File &source, juce::String &createdId);
    juce::Result exportFile(const juce::String &id, const juce::File &destination) const;

  private:
    juce::File directory_;
    std::vector<Preset> presets_;
    std::set<std::string> favorites_;
};
} // namespace darkvoice
