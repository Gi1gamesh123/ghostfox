#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ruyijs::domain::browser_environment {

/// Value Object: one installed speech-synthesis voice.
///
/// The name is the platform's own voice name (a SAPI voice name on Windows) and
/// the language is its BCP 47 tag. A voice object's `voiceURI`, `localService`
/// and `default` follow from these two values plus the environment's default
/// voice, so the domain keeps only the data a profile can change.
struct SpeechVoice {
    std::string name;
    std::string lang;

    bool operator==(const SpeechVoice& o) const {
        return name == o.name && lang == o.lang;
    }
};

/// Entity: Speech Synthesis information.
///
/// The baseline voice list of the browsing environment: the voices a fresh
/// profile installs before any profile override. The Speech Synthesis owner
/// reads it through the profile's "speech" group, so the list belongs to this
/// aggregate (the profile data) and not to the JavaScript adapter.
class SpeechInfo {
public:
    const std::vector<SpeechVoice>& voices() const noexcept { return voices_; }
    const std::string& default_voice() const noexcept { return default_name_; }

    /// Firefox 158 on Windows enumerates these five local SAPI voices and marks
    /// none of them as the default (`getVoices()[i].default` is false for all).
    void apply_defaults() {
        voices_ = {
            {"Microsoft Huihui - Chinese (Simplified, PRC)", "zh-CN"},
            {"Microsoft Kangkang - Chinese (Simplified, PRC)", "zh-CN"},
            {"Microsoft Yaoyao - Chinese (Simplified, PRC)", "zh-CN"},
            {"Microsoft Huihui Desktop - Chinese (Simplified)", "zh-CN"},
            {"Microsoft Zira Desktop - English (United States)", "en-US"},
        };
        default_name_.clear();
    }

private:
    std::vector<SpeechVoice> voices_;
    std::string default_name_;
};

} // namespace ruyijs::domain::browser_environment
