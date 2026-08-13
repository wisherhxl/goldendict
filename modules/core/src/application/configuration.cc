// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include "../foundation/utf8.h"
#include "article_tab_session.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-core-config-v1\n";
constexpr std::size_t kMaximumConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumDictionaryPaths = 256U;
constexpr std::size_t kMaximumSoundDirectories = 256U;
constexpr std::size_t kMaximumDictionaryGroups = 256U;
constexpr std::size_t kMaximumDictionariesPerGroup = 256U;
constexpr std::size_t kMaximumGroupValueBytes = 4096U;
constexpr std::size_t kMaximumEncodedGroupIconBytes = 64U * 1024U;
constexpr std::size_t kMaximumPreferenceStringBytes = 4096U;
constexpr std::uint32_t kKnownScanPopupModifierMask = 0x03ffU;

std::string Encode(std::string_view value);

template <typename Integer>
Integer ParseInteger(std::string_view value) {
    Integer parsed{};
    const auto conversion =
        std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (value.empty() || conversion.ec != std::errc() ||
        conversion.ptr != value.data() + value.size()) {
        throw std::runtime_error("Invalid numeric preference value");
    }
    return parsed;
}

bool ParseBoolean(std::string_view value) {
    if (value == "0")
        return false;
    if (value == "1")
        return true;
    throw std::runtime_error("Invalid boolean preference value");
}

double ParseDouble(std::string_view value) {
    double parsed = 0.0;
    const auto conversion =
        std::from_chars(value.data(), value.data() + value.size(), parsed,
                        std::chars_format::general);
    if (value.empty() || conversion.ec != std::errc() ||
        conversion.ptr != value.data() + value.size() ||
        !std::isfinite(parsed)) {
        throw std::runtime_error("Invalid floating-point preference value");
    }
    return parsed;
}

std::string FormatDouble(double value) {
    char buffer[64];
    const auto conversion = std::to_chars(
        std::begin(buffer), std::end(buffer), value, std::chars_format::general,
        std::numeric_limits<double>::max_digits10);
    if (conversion.ec != std::errc()) {
        throw std::runtime_error("Cannot encode floating-point preference");
    }
    return std::string(buffer, conversion.ptr);
}

template <typename Integer>
std::string FormatInteger(Integer value) {
    char buffer[32];
    const auto conversion =
        std::to_chars(std::begin(buffer), std::end(buffer), value, 10);
    if (conversion.ec != std::errc()) {
        throw std::runtime_error("Cannot encode numeric preference");
    }
    return std::string(buffer, conversion.ptr);
}

void AppendPreference(std::string& contents, std::string_view name,
                      std::string value) {
    contents += "preference=" + Encode(name) + "|" + Encode(value) + "\n";
}

template <typename Enum>
Enum ParseEnum(std::string_view value, std::uint8_t maximum) {
    const auto parsed = ParseInteger<std::uint8_t>(value);
    if (parsed > maximum) {
        throw std::runtime_error("Invalid enumerated preference value");
    }
    return static_cast<Enum>(parsed);
}

void SetPreference(ApplicationPreferences& preferences, std::string_view name,
                   std::string value) {
#define STRING_PREFERENCE(key, member)         \
    if (name == key) {                         \
        preferences.member = std::move(value); \
        return;                                \
    }
#define BOOL_PREFERENCE(key, member)              \
    if (name == key) {                            \
        preferences.member = ParseBoolean(value); \
        return;                                   \
    }
#define UINT_PREFERENCE(key, member, type)              \
    if (name == key) {                                  \
        preferences.member = ParseInteger<type>(value); \
        return;                                         \
    }
    STRING_PREFERENCE("interface_language", interface_language)
    STRING_PREFERENCE("help_language", help_language)
    STRING_PREFERENCE("display_style", display_style)
    STRING_PREFERENCE("addon_style", addon_style)
    STRING_PREFERENCE("main_window_hotkey", main_window_hotkey)
    STRING_PREFERENCE("clipboard_hotkey", clipboard_hotkey)
    STRING_PREFERENCE("audio_playback_program", audio_playback_program)
    STRING_PREFERENCE("proxy_host", proxy_host)
    STRING_PREFERENCE("full_text_disabled_types", full_text_disabled_types)
    BOOL_PREFERENCE("hide_menubar", hide_menubar)
    BOOL_PREFERENCE("enable_tray_icon", enable_tray_icon)
    BOOL_PREFERENCE("start_to_tray", start_to_tray)
    BOOL_PREFERENCE("close_to_tray", close_to_tray)
    BOOL_PREFERENCE("auto_start", auto_start)
    BOOL_PREFERENCE("double_click_translates", double_click_translates)
    BOOL_PREFERENCE("select_word_by_single_click", select_word_by_single_click)
    BOOL_PREFERENCE("escape_hides_main_window", escape_hides_main_window)
    BOOL_PREFERENCE("always_on_top", always_on_top)
    BOOL_PREFERENCE("search_in_dock", search_in_dock)
    BOOL_PREFERENCE("enable_main_window_hotkey", enable_main_window_hotkey)
    BOOL_PREFERENCE("enable_clipboard_hotkey", enable_clipboard_hotkey)
    BOOL_PREFERENCE("enable_scan_popup", enable_scan_popup)
    BOOL_PREFERENCE("start_with_scan_popup_on", start_with_scan_popup_on)
    BOOL_PREFERENCE("enable_scan_popup_modifiers", enable_scan_popup_modifiers)
    BOOL_PREFERENCE("scan_popup_alt_mode", scan_popup_alt_mode)
    BOOL_PREFERENCE("ignore_own_clipboard_changes",
                    ignore_own_clipboard_changes)
    BOOL_PREFERENCE("scan_popup_use_ui_automation",
                    scan_popup_use_ui_automation)
    BOOL_PREFERENCE("scan_popup_use_accessibility",
                    scan_popup_use_accessibility)
    BOOL_PREFERENCE("scan_popup_use_gd_message", scan_popup_use_gd_message)
    BOOL_PREFERENCE("scan_to_main_window", scan_to_main_window)
    BOOL_PREFERENCE("ignore_diacritics", ignore_diacritics)
    BOOL_PREFERENCE("show_scan_flag", show_scan_flag)
    BOOL_PREFERENCE("track_clipboard_changes", track_clipboard_changes)
    BOOL_PREFERENCE("pronounce_on_load_main", pronounce_on_load_main)
    BOOL_PREFERENCE("pronounce_on_load_popup", pronounce_on_load_popup)
    BOOL_PREFERENCE("use_internal_player", use_internal_player)
    BOOL_PREFERENCE("check_for_new_releases", check_for_new_releases)
    BOOL_PREFERENCE("disallow_content_from_other_sites",
                    disallow_content_from_other_sites)
    BOOL_PREFERENCE("enable_web_plugins", enable_web_plugins)
    BOOL_PREFERENCE("hide_goldendict_header", hide_goldendict_header)
    BOOL_PREFERENCE("clear_network_cache_on_exit", clear_network_cache_on_exit)
    BOOL_PREFERENCE("store_history", store_history)
    BOOL_PREFERENCE("confirm_favorites_deletion", confirm_favorites_deletion)
    BOOL_PREFERENCE("always_expand_optional_parts",
                    always_expand_optional_parts)
    BOOL_PREFERENCE("collapse_large_articles", collapse_large_articles)
    BOOL_PREFERENCE("limit_input_phrase_length", limit_input_phrase_length)
    BOOL_PREFERENCE("synonym_search_enabled", synonym_search_enabled)
    BOOL_PREFERENCE("full_text_search_enabled", full_text_search_enabled)
    BOOL_PREFERENCE("full_text_match_case", full_text_match_case)
    BOOL_PREFERENCE("full_text_use_maximum_word_distance",
                    full_text_use_maximum_word_distance)
    BOOL_PREFERENCE("full_text_use_maximum_articles",
                    full_text_use_maximum_articles)
    BOOL_PREFERENCE("full_text_ignore_word_order", full_text_ignore_word_order)
    BOOL_PREFERENCE("full_text_ignore_diacritics", full_text_ignore_diacritics)
    UINT_PREFERENCE("scan_popup_modifiers", scan_popup_modifiers, std::uint32_t)
    UINT_PREFERENCE("scan_popup_alt_mode_seconds", scan_popup_alt_mode_seconds,
                    std::uint32_t)
    UINT_PREFERENCE("proxy_port", proxy_port, std::uint16_t)
    UINT_PREFERENCE("maximum_network_cache_megabytes",
                    maximum_network_cache_megabytes, std::uint32_t)
    UINT_PREFERENCE("words_zoom_level", words_zoom_level, std::int32_t)
    UINT_PREFERENCE("maximum_history_entries", maximum_history_entries,
                    std::uint32_t)
    UINT_PREFERENCE("history_store_interval_seconds",
                    history_store_interval_seconds, std::uint32_t)
    UINT_PREFERENCE("favorites_store_interval_seconds",
                    favorites_store_interval_seconds, std::uint32_t)
    UINT_PREFERENCE("article_size_limit", article_size_limit, std::uint32_t)
    UINT_PREFERENCE("input_phrase_length_limit", input_phrase_length_limit,
                    std::uint32_t)
    UINT_PREFERENCE("maximum_dictionary_references",
                    maximum_dictionary_references, std::uint16_t)
    UINT_PREFERENCE("full_text_maximum_articles_per_dictionary",
                    full_text_maximum_articles_per_dictionary, std::uint32_t)
    UINT_PREFERENCE("full_text_maximum_word_distance",
                    full_text_maximum_word_distance, std::uint32_t)
    UINT_PREFERENCE("full_text_maximum_dictionary_megabytes",
                    full_text_maximum_dictionary_megabytes, std::uint32_t)
#undef STRING_PREFERENCE
#undef BOOL_PREFERENCE
#undef UINT_PREFERENCE
    if (name == "scan_popup_window_mode") {
        preferences.scan_popup_window_mode =
            ParseEnum<ScanPopupWindowMode>(value, 2U);
    } else if (name == "audio_backend") {
        preferences.audio_backend = ParseEnum<AudioBackend>(value, 2U);
    } else if (name == "proxy_mode") {
        preferences.proxy_mode = ParseEnum<ProxyMode>(value, 2U);
    } else if (name == "proxy_type") {
        preferences.proxy_type = ParseEnum<ProxyType>(value, 2U);
    } else if (name == "full_text_search_mode") {
        preferences.full_text_search_mode =
            ParseEnum<FullTextSearchMode>(value, 2U);
    } else if (name == "zoom_factor") {
        preferences.zoom_factor = ParseDouble(value);
    } else if (name == "help_zoom_factor") {
        preferences.help_zoom_factor = ParseDouble(value);
    } else {
        throw std::runtime_error("Unknown preference field");
    }
}

bool IsCanonicalBase64(std::string_view value) {
    if (value.empty()) {
        return true;
    }
    if (value.size() > kMaximumEncodedGroupIconBytes ||
        value.size() % 4U != 0U) {
        return false;
    }
    std::size_t padding = 0U;
    if (value.back() == '=') {
        padding = 1U;
        if (value.size() >= 2U && value[value.size() - 2U] == '=') {
            padding = 2U;
        }
    }
    const auto base64_value = [](char character) {
        if (character >= 'A' && character <= 'Z')
            return character - 'A';
        if (character >= 'a' && character <= 'z')
            return character - 'a' + 26;
        if (character >= '0' && character <= '9')
            return character - '0' + 52;
        if (character == '+')
            return 62;
        if (character == '/')
            return 63;
        return -1;
    };
    for (std::size_t index = 0U; index < value.size() - padding; ++index) {
        if (base64_value(value[index]) < 0) {
            return false;
        }
    }
    for (std::size_t index = value.size() - padding; index < value.size();
         ++index) {
        if (value[index] != '=') {
            return false;
        }
    }
    if (padding == 1U && (base64_value(value[value.size() - 2U]) & 0x03) != 0) {
        return false;
    }
    if (padding == 2U && (base64_value(value[value.size() - 3U]) & 0x0f) != 0) {
        return false;
    }
    return true;
}

bool IsUnescaped(unsigned char character) {
    return std::isalnum(character) != 0 || character == '-' ||
           character == '_' || character == '.' || character == '/' ||
           character == ':';
}

std::string Encode(std::string_view value) {
    constexpr char kHex[] = "0123456789ABCDEF";
    std::string encoded;
    for (const unsigned char character : value) {
        if (IsUnescaped(character)) {
            encoded.push_back(static_cast<char>(character));
        } else {
            encoded.push_back('%');
            encoded.push_back(kHex[character >> 4U]);
            encoded.push_back(kHex[character & 0x0fU]);
        }
    }
    return encoded;
}

int HexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

std::string Decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '%') {
            decoded.push_back(value[index]);
            continue;
        }
        if (index + 2U >= value.size()) {
            throw std::runtime_error("Truncated configuration escape");
        }
        const int high = HexValue(value[index + 1U]);
        const int low = HexValue(value[index + 2U]);
        if (high < 0 || low < 0) {
            throw std::runtime_error("Invalid configuration escape");
        }
        decoded.push_back(static_cast<char>((high << 4) | low));
        index += 2U;
    }
    return decoded;
}

void Validate(const CoreConfiguration& configuration) {
    if (configuration.dictionary_paths.size() > kMaximumDictionaryPaths) {
        throw std::runtime_error("Configuration has too many dictionary paths");
    }
    if (configuration.sound_directories.size() > kMaximumSoundDirectories) {
        throw std::runtime_error(
            "Configuration has too many sound directories");
    }
    if (configuration.dictionary_groups.size() > kMaximumDictionaryGroups) {
        throw std::runtime_error(
            "Configuration has too many dictionary groups");
    }
    if (configuration.article_tab_session.has_value() &&
        !application::ValidateArticleTabSession(
            *configuration.article_tab_session)) {
        throw std::runtime_error("Article tab session is invalid");
    }
    const auto has_nul = [](const std::string& value) {
        return value.find('\0') != std::string::npos;
    };
    if (std::any_of(
            configuration.sound_directories.begin(),
            configuration.sound_directories.end(),
            [](const auto& directory) { return directory.path.empty(); })) {
        throw std::runtime_error("Sound directory paths cannot be empty");
    }
    if (has_nul(configuration.index_directory) ||
        std::any_of(configuration.dictionary_paths.begin(),
                    configuration.dictionary_paths.end(), has_nul) ||
        std::any_of(configuration.sound_directories.begin(),
                    configuration.sound_directories.end(),
                    [&has_nul](const auto& directory) {
                        return has_nul(directory.path) ||
                               has_nul(directory.name);
                    })) {
        throw std::runtime_error("Configuration values cannot contain NUL");
    }

    const auto& preferences = configuration.preferences;
    const std::string* preference_strings[] = {
        &preferences.interface_language,
        &preferences.help_language,
        &preferences.display_style,
        &preferences.addon_style,
        &preferences.main_window_hotkey,
        &preferences.clipboard_hotkey,
        &preferences.audio_playback_program,
        &preferences.proxy_host,
        &preferences.full_text_disabled_types,
    };
    for (const auto* value : preference_strings) {
        if (value->size() > kMaximumPreferenceStringBytes || has_nul(*value) ||
            !foundation::IsValidUtf8(*value)) {
            throw std::runtime_error("Preference string is invalid");
        }
    }
    if ((preferences.scan_popup_modifiers & ~kKnownScanPopupModifierMask) !=
            0U ||
        preferences.scan_popup_alt_mode_seconds == 0U ||
        preferences.scan_popup_alt_mode_seconds > 60U ||
        static_cast<std::uint8_t>(preferences.scan_popup_window_mode) > 2U ||
        static_cast<std::uint8_t>(preferences.audio_backend) > 2U ||
        static_cast<std::uint8_t>(preferences.proxy_mode) > 2U ||
        static_cast<std::uint8_t>(preferences.proxy_type) > 2U) {
        throw std::runtime_error("Enumerated preference is invalid");
    }
    if (preferences.proxy_mode == ProxyMode::kManual &&
        (preferences.proxy_host.empty() || preferences.proxy_port == 0U)) {
        throw std::runtime_error("Manual proxy requires a host and port");
    }
    if (!std::isfinite(preferences.zoom_factor) ||
        preferences.zoom_factor < 0.25 || preferences.zoom_factor > 5.0 ||
        !std::isfinite(preferences.help_zoom_factor) ||
        preferences.help_zoom_factor < 0.25 ||
        preferences.help_zoom_factor > 5.0 ||
        preferences.words_zoom_level < -10 ||
        preferences.words_zoom_level > 10 ||
        preferences.maximum_network_cache_megabytes > 10240U ||
        preferences.maximum_history_entries > 1000000U ||
        preferences.history_store_interval_seconds > 86400U ||
        preferences.favorites_store_interval_seconds > 86400U ||
        preferences.article_size_limit == 0U ||
        preferences.article_size_limit > 1000000U ||
        preferences.input_phrase_length_limit == 0U ||
        preferences.input_phrase_length_limit > 1000000U ||
        preferences.maximum_dictionary_references == 0U ||
        preferences.maximum_dictionary_references > 1000U ||
        static_cast<std::uint8_t>(preferences.full_text_search_mode) > 2U ||
        preferences.full_text_maximum_articles_per_dictionary == 0U ||
        preferences.full_text_maximum_articles_per_dictionary > 100000U ||
        preferences.full_text_maximum_word_distance > 1000U ||
        preferences.full_text_maximum_dictionary_megabytes > 1048576U) {
        throw std::runtime_error("Numeric preference is outside its bounds");
    }

    std::unordered_set<std::uint32_t> group_ids;
    for (const auto& group : configuration.dictionary_groups) {
        if (group.id == 0U || !group_ids.insert(group.id).second) {
            throw std::runtime_error(
                "Dictionary group IDs must be unique and nonzero");
        }
        if (group.name.empty() || group.name.size() > kMaximumGroupValueBytes ||
            group.icon.size() > kMaximumGroupValueBytes ||
            group.favorites_folder.size() > kMaximumGroupValueBytes ||
            group.shortcut.size() > kMaximumGroupValueBytes ||
            has_nul(group.name) || has_nul(group.icon)) {
            throw std::runtime_error("Dictionary group metadata is invalid");
        }
        if (has_nul(group.favorites_folder) || has_nul(group.shortcut) ||
            !IsCanonicalBase64(group.encoded_icon_data)) {
            throw std::runtime_error("Dictionary group metadata is invalid");
        }
        if (group.dictionary_ids.size() > kMaximumDictionariesPerGroup) {
            throw std::runtime_error(
                "Dictionary group has too many dictionaries");
        }
        std::unordered_set<std::string> dictionary_ids;
        for (const auto& dictionary_id : group.dictionary_ids) {
            if (dictionary_id.empty() ||
                dictionary_id.size() > kMaximumGroupValueBytes ||
                has_nul(dictionary_id) ||
                !dictionary_ids.insert(dictionary_id).second) {
                throw std::runtime_error(
                    "Dictionary group dictionary IDs must be unique and valid");
            }
        }
        const auto validate_muted = [&has_nul](const auto& values) {
            if (values.size() > kMaximumDictionariesPerGroup) {
                throw std::runtime_error(
                    "Dictionary group has too many muted dictionaries");
            }
            std::unordered_set<std::string> unique;
            for (const auto& value : values) {
                if (value.empty() || value.size() > kMaximumGroupValueBytes ||
                    has_nul(value) || !unique.insert(value).second) {
                    throw std::runtime_error(
                        "Dictionary group muted IDs must be unique and valid");
                }
            }
        };
        validate_muted(group.muted_dictionary_ids);
        validate_muted(group.popup_muted_dictionary_ids);
    }
}

}  // namespace

CoreConfiguration LoadConfiguration(const std::string& configuration_path) {
    std::ifstream input(configuration_path, std::ios::binary);
    if (!input) {
        std::error_code error;
        if (!std::filesystem::exists(configuration_path, error) && !error) {
            return {};
        }
        throw std::runtime_error("Cannot open configuration file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 ||
        static_cast<std::uintmax_t>(size) > kMaximumConfigurationBytes) {
        throw std::runtime_error("Cannot read bounded configuration file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error("Cannot read complete configuration file");
    }
    if (contents.substr(0, kHeader.size()) != kHeader) {
        throw std::runtime_error("Unsupported configuration format");
    }

    CoreConfiguration configuration;
    std::unordered_map<std::uint32_t, std::size_t> group_indexes;
    std::unordered_set<std::uint32_t> group_metadata_ids;
    std::unordered_set<std::string> preference_names;
    std::unordered_map<ArticleTabId, std::size_t> tab_indexes;
    bool has_article_tab_session = false;
    bool has_index_directory = false;
    std::size_t position = kHeader.size();
    while (position < contents.size()) {
        const auto end = contents.find('\n', position);
        if (end == std::string::npos) {
            throw std::runtime_error("Configuration line is not terminated");
        }
        const std::string_view line(contents.data() + position, end - position);
        constexpr std::string_view kIndex = "index_directory=";
        constexpr std::string_view kDictionary = "dictionary_path=";
        constexpr std::string_view kSoundDirectory = "sound_directory=";
        constexpr std::string_view kDictionaryGroup = "dictionary_group=";
        constexpr std::string_view kDictionaryGroupMetadata =
            "dictionary_group_metadata=";
        constexpr std::string_view kPreference = "preference=";
        constexpr std::string_view kArticleTabSession = "article_tab_session=";
        constexpr std::string_view kArticleTab = "article_tab=";
        constexpr std::string_view kArticleTabNavigation =
            "article_tab_navigation=";
        if (line.substr(0, kIndex.size()) == kIndex) {
            if (has_index_directory) {
                throw std::runtime_error("Duplicate index directory");
            }
            has_index_directory = true;
            configuration.index_directory = Decode(line.substr(kIndex.size()));
        } else if (line.substr(0, kDictionary.size()) == kDictionary) {
            configuration.dictionary_paths.push_back(
                Decode(line.substr(kDictionary.size())));
        } else if (line.substr(0, kSoundDirectory.size()) == kSoundDirectory) {
            const auto value = line.substr(kSoundDirectory.size());
            const auto separator = value.find('|');
            if (separator == std::string_view::npos) {
                throw std::runtime_error("Malformed sound directory field");
            }
            configuration.sound_directories.push_back(
                {Decode(value.substr(0U, separator)),
                 Decode(value.substr(separator + 1U))});
        } else if (line.substr(0, kDictionaryGroup.size()) ==
                   kDictionaryGroup) {
            const auto value = line.substr(kDictionaryGroup.size());
            std::vector<std::string_view> fields;
            std::size_t field_position = 0U;
            while (field_position <= value.size()) {
                const auto separator = value.find('|', field_position);
                fields.push_back(value.substr(
                    field_position, separator == std::string_view::npos
                                        ? value.size() - field_position
                                        : separator - field_position));
                if (separator == std::string_view::npos) {
                    break;
                }
                field_position = separator + 1U;
            }
            if (fields.size() < 3U) {
                throw std::runtime_error("Malformed dictionary group field");
            }
            DictionaryGroupConfiguration group;
            const auto id_text = fields.front();
            const auto conversion = std::from_chars(
                id_text.data(), id_text.data() + id_text.size(), group.id, 10);
            if (conversion.ec != std::errc() ||
                conversion.ptr != id_text.data() + id_text.size()) {
                throw std::runtime_error("Dictionary group ID is invalid");
            }
            group.name = Decode(fields[1U]);
            group.icon = Decode(fields[2U]);
            for (std::size_t index = 3U; index < fields.size(); ++index) {
                group.dictionary_ids.push_back(Decode(fields[index]));
            }
            configuration.dictionary_groups.push_back(std::move(group));
            const auto& stored = configuration.dictionary_groups.back();
            if (!group_indexes
                     .emplace(stored.id,
                              configuration.dictionary_groups.size() - 1U)
                     .second) {
                throw std::runtime_error("Duplicate dictionary group ID");
            }
        } else if (line.substr(0, kDictionaryGroupMetadata.size()) ==
                   kDictionaryGroupMetadata) {
            const auto value = line.substr(kDictionaryGroupMetadata.size());
            std::vector<std::string_view> fields;
            std::size_t start = 0U;
            while (start <= value.size()) {
                const auto separator = value.find('|', start);
                fields.push_back(
                    value.substr(start, separator == std::string_view::npos
                                            ? value.size() - start
                                            : separator - start));
                if (separator == std::string_view::npos) {
                    break;
                }
                start = separator + 1U;
            }
            if (fields.size() < 6U) {
                throw std::runtime_error(
                    "Malformed dictionary group metadata field");
            }
            std::uint32_t id = 0U;
            const auto id_conversion = std::from_chars(
                fields[0].data(), fields[0].data() + fields[0].size(), id, 10);
            std::size_t muted_count = 0U;
            const auto muted_conversion = std::from_chars(
                fields[4].data(), fields[4].data() + fields[4].size(),
                muted_count, 10);
            if (id_conversion.ec != std::errc() ||
                id_conversion.ptr != fields[0].data() + fields[0].size() ||
                muted_conversion.ec != std::errc() ||
                muted_conversion.ptr != fields[4].data() + fields[4].size() ||
                muted_count > kMaximumDictionariesPerGroup ||
                5U + muted_count >= fields.size()) {
                throw std::runtime_error(
                    "Malformed dictionary group metadata field");
            }
            const std::size_t popup_count_index = 5U + muted_count;
            std::size_t popup_count = 0U;
            const auto popup_conversion =
                std::from_chars(fields[popup_count_index].data(),
                                fields[popup_count_index].data() +
                                    fields[popup_count_index].size(),
                                popup_count, 10);
            if (popup_conversion.ec != std::errc() ||
                popup_conversion.ptr != fields[popup_count_index].data() +
                                            fields[popup_count_index].size() ||
                popup_count > kMaximumDictionariesPerGroup ||
                popup_count_index + 1U + popup_count != fields.size()) {
                throw std::runtime_error(
                    "Malformed dictionary group metadata field");
            }
            const auto found = group_indexes.find(id);
            if (found == group_indexes.end() ||
                !group_metadata_ids.insert(id).second) {
                throw std::runtime_error(
                    "Dictionary group metadata must reference one group");
            }
            auto& group = configuration.dictionary_groups[found->second];
            group.favorites_folder = Decode(fields[1]);
            group.shortcut = Decode(fields[2]);
            group.encoded_icon_data = Decode(fields[3]);
            for (std::size_t index = 5U; index < popup_count_index; ++index) {
                group.muted_dictionary_ids.push_back(Decode(fields[index]));
            }
            for (std::size_t index = popup_count_index + 1U;
                 index < fields.size(); ++index) {
                group.popup_muted_dictionary_ids.push_back(
                    Decode(fields[index]));
            }
        } else if (line.substr(0, kArticleTabSession.size()) ==
                   kArticleTabSession) {
            if (has_article_tab_session) {
                throw std::runtime_error("Duplicate article tab session");
            }
            has_article_tab_session = true;
            configuration.article_tab_session.emplace();
            configuration.article_tab_session->active_tab_id =
                ParseInteger<ArticleTabId>(
                    line.substr(kArticleTabSession.size()));
        } else if (line.substr(0, kArticleTab.size()) == kArticleTab) {
            if (!has_article_tab_session) {
                throw std::runtime_error(
                    "Article tab appears outside a session");
            }
            const auto value = line.substr(kArticleTab.size());
            const auto separator = value.find('|');
            if (separator == std::string_view::npos ||
                value.find('|', separator + 1U) != std::string_view::npos) {
                throw std::runtime_error("Malformed article tab field");
            }
            ArticleTabSessionTab tab;
            tab.id = ParseInteger<ArticleTabId>(value.substr(0U, separator));
            tab.history_cursor =
                ParseInteger<std::size_t>(value.substr(separator + 1U));
            auto& tabs = configuration.article_tab_session->tabs;
            tabs.push_back(std::move(tab));
            if (!tab_indexes.emplace(tabs.back().id, tabs.size() - 1U).second) {
                throw std::runtime_error("Duplicate article tab ID");
            }
        } else if (line.substr(0, kArticleTabNavigation.size()) ==
                   kArticleTabNavigation) {
            if (!has_article_tab_session) {
                throw std::runtime_error(
                    "Article navigation appears outside a session");
            }
            const auto value = line.substr(kArticleTabNavigation.size());
            std::vector<std::string_view> fields;
            std::size_t start = 0U;
            while (start <= value.size()) {
                const auto separator = value.find('|', start);
                fields.push_back(
                    value.substr(start, separator == std::string_view::npos
                                            ? value.size() - start
                                            : separator - start));
                if (separator == std::string_view::npos) {
                    break;
                }
                start = separator + 1U;
            }
            if (fields.size() != 10U) {
                throw std::runtime_error(
                    "Malformed article tab navigation field");
            }
            const ArticleTabId tab_id = ParseInteger<ArticleTabId>(fields[0]);
            const auto found = tab_indexes.find(tab_id);
            if (found == tab_indexes.end()) {
                throw std::runtime_error(
                    "Article navigation references an unknown tab");
            }
            TabNavigationState navigation;
            navigation.kind = static_cast<TabNavigationKind>(
                ParseInteger<std::uint8_t>(fields[1]));
            navigation.query = Decode(fields[2]);
            navigation.group_id = ParseInteger<std::uint32_t>(fields[3]);
            navigation.title = Decode(fields[4]);
            navigation.internal_url = Decode(fields[5]);
            navigation.source_dictionary_id = Decode(fields[6]);
            navigation.source_article_id = Decode(fields[7]);
            navigation.target_article_id = Decode(fields[8]);
            navigation.target_anchor = Decode(fields[9]);
            configuration.article_tab_session->tabs[found->second]
                .history.push_back(std::move(navigation));
        } else if (line.substr(0, kPreference.size()) == kPreference) {
            const auto value = line.substr(kPreference.size());
            const auto separator = value.find('|');
            if (separator == std::string_view::npos ||
                value.find('|', separator + 1U) != std::string_view::npos) {
                throw std::runtime_error("Malformed preference field");
            }
            std::string name = Decode(value.substr(0U, separator));
            if (name.empty() || !preference_names.insert(name).second) {
                throw std::runtime_error("Duplicate or empty preference field");
            }
            SetPreference(configuration.preferences, name,
                          Decode(value.substr(separator + 1U)));
        } else if (!line.empty()) {
            throw std::runtime_error("Unknown configuration field");
        }
        position = end + 1U;
    }
    Validate(configuration);
    return configuration;
}

void SaveConfiguration(const std::string& configuration_path,
                       const CoreConfiguration& configuration) {
    Validate(configuration);
    std::string contents(kHeader);
    contents +=
        "index_directory=" + Encode(configuration.index_directory) + "\n";
    for (const auto& path : configuration.dictionary_paths) {
        contents += "dictionary_path=" + Encode(path) + "\n";
    }
    for (const auto& directory : configuration.sound_directories) {
        contents += "sound_directory=" + Encode(directory.path) + "|" +
                    Encode(directory.name) + "\n";
    }
    for (const auto& group : configuration.dictionary_groups) {
        contents += "dictionary_group=" + std::to_string(group.id) + "|" +
                    Encode(group.name) + "|" + Encode(group.icon);
        for (const auto& dictionary_id : group.dictionary_ids) {
            contents += "|" + Encode(dictionary_id);
        }
        contents += "\n";
        if (!group.favorites_folder.empty() || !group.shortcut.empty() ||
            !group.encoded_icon_data.empty() ||
            !group.muted_dictionary_ids.empty() ||
            !group.popup_muted_dictionary_ids.empty()) {
            contents +=
                "dictionary_group_metadata=" + std::to_string(group.id) + "|" +
                Encode(group.favorites_folder) + "|" + Encode(group.shortcut) +
                "|" + Encode(group.encoded_icon_data) + "|" +
                std::to_string(group.muted_dictionary_ids.size());
            for (const auto& id : group.muted_dictionary_ids) {
                contents += "|" + Encode(id);
            }
            contents +=
                "|" + std::to_string(group.popup_muted_dictionary_ids.size());
            for (const auto& id : group.popup_muted_dictionary_ids) {
                contents += "|" + Encode(id);
            }
            contents += "\n";
        }
    }
    if (configuration.article_tab_session.has_value()) {
        const auto& session = *configuration.article_tab_session;
        contents +=
            "article_tab_session=" + std::to_string(session.active_tab_id) +
            "\n";
        for (const auto& tab : session.tabs) {
            contents += "article_tab=" + std::to_string(tab.id) + "|" +
                        std::to_string(tab.history_cursor) + "\n";
            for (const auto& navigation : tab.history) {
                contents +=
                    "article_tab_navigation=" + std::to_string(tab.id) + "|" +
                    std::to_string(static_cast<unsigned int>(navigation.kind)) +
                    "|" + Encode(navigation.query) + "|" +
                    std::to_string(navigation.group_id) + "|" +
                    Encode(navigation.title) + "|" +
                    Encode(navigation.internal_url) + "|" +
                    Encode(navigation.source_dictionary_id) + "|" +
                    Encode(navigation.source_article_id) + "|" +
                    Encode(navigation.target_article_id) + "|" +
                    Encode(navigation.target_anchor) + "\n";
            }
        }
    }
    const auto& p = configuration.preferences;
#define APPEND_STRING(key, member) AppendPreference(contents, key, p.member)
#define APPEND_BOOL(key, member) \
    AppendPreference(contents, key, p.member ? "1" : "0")
#define APPEND_NUMBER(key, member) \
    AppendPreference(contents, key, FormatInteger(p.member))
    APPEND_STRING("interface_language", interface_language);
    APPEND_STRING("help_language", help_language);
    APPEND_STRING("display_style", display_style);
    APPEND_STRING("addon_style", addon_style);
    APPEND_BOOL("hide_menubar", hide_menubar);
    APPEND_BOOL("enable_tray_icon", enable_tray_icon);
    APPEND_BOOL("start_to_tray", start_to_tray);
    APPEND_BOOL("close_to_tray", close_to_tray);
    APPEND_BOOL("auto_start", auto_start);
    APPEND_BOOL("double_click_translates", double_click_translates);
    APPEND_BOOL("select_word_by_single_click", select_word_by_single_click);
    APPEND_BOOL("escape_hides_main_window", escape_hides_main_window);
    APPEND_BOOL("always_on_top", always_on_top);
    APPEND_BOOL("search_in_dock", search_in_dock);
    APPEND_BOOL("enable_main_window_hotkey", enable_main_window_hotkey);
    APPEND_STRING("main_window_hotkey", main_window_hotkey);
    APPEND_BOOL("enable_clipboard_hotkey", enable_clipboard_hotkey);
    APPEND_STRING("clipboard_hotkey", clipboard_hotkey);
    APPEND_BOOL("enable_scan_popup", enable_scan_popup);
    APPEND_BOOL("start_with_scan_popup_on", start_with_scan_popup_on);
    APPEND_BOOL("enable_scan_popup_modifiers", enable_scan_popup_modifiers);
    APPEND_NUMBER("scan_popup_modifiers", scan_popup_modifiers);
    APPEND_BOOL("scan_popup_alt_mode", scan_popup_alt_mode);
    APPEND_NUMBER("scan_popup_alt_mode_seconds", scan_popup_alt_mode_seconds);
    APPEND_BOOL("ignore_own_clipboard_changes", ignore_own_clipboard_changes);
    APPEND_BOOL("scan_popup_use_ui_automation", scan_popup_use_ui_automation);
    APPEND_BOOL("scan_popup_use_accessibility", scan_popup_use_accessibility);
    APPEND_BOOL("scan_popup_use_gd_message", scan_popup_use_gd_message);
    APPEND_BOOL("scan_to_main_window", scan_to_main_window);
    APPEND_BOOL("ignore_diacritics", ignore_diacritics);
    APPEND_BOOL("show_scan_flag", show_scan_flag);
    APPEND_BOOL("track_clipboard_changes", track_clipboard_changes);
    AppendPreference(
        contents, "scan_popup_window_mode",
        FormatInteger(static_cast<std::uint8_t>(p.scan_popup_window_mode)));
    APPEND_BOOL("pronounce_on_load_main", pronounce_on_load_main);
    APPEND_BOOL("pronounce_on_load_popup", pronounce_on_load_popup);
    APPEND_BOOL("use_internal_player", use_internal_player);
    AppendPreference(contents, "audio_backend",
                     FormatInteger(static_cast<std::uint8_t>(p.audio_backend)));
    APPEND_STRING("audio_playback_program", audio_playback_program);
    AppendPreference(contents, "proxy_mode",
                     FormatInteger(static_cast<std::uint8_t>(p.proxy_mode)));
    AppendPreference(contents, "proxy_type",
                     FormatInteger(static_cast<std::uint8_t>(p.proxy_type)));
    APPEND_STRING("proxy_host", proxy_host);
    APPEND_NUMBER("proxy_port", proxy_port);
    APPEND_BOOL("check_for_new_releases", check_for_new_releases);
    APPEND_BOOL("disallow_content_from_other_sites",
                disallow_content_from_other_sites);
    APPEND_BOOL("enable_web_plugins", enable_web_plugins);
    APPEND_BOOL("hide_goldendict_header", hide_goldendict_header);
    APPEND_NUMBER("maximum_network_cache_megabytes",
                  maximum_network_cache_megabytes);
    APPEND_BOOL("clear_network_cache_on_exit", clear_network_cache_on_exit);
    AppendPreference(contents, "zoom_factor", FormatDouble(p.zoom_factor));
    AppendPreference(contents, "help_zoom_factor",
                     FormatDouble(p.help_zoom_factor));
    APPEND_NUMBER("words_zoom_level", words_zoom_level);
    APPEND_NUMBER("maximum_history_entries", maximum_history_entries);
    APPEND_BOOL("store_history", store_history);
    APPEND_NUMBER("history_store_interval_seconds",
                  history_store_interval_seconds);
    APPEND_NUMBER("favorites_store_interval_seconds",
                  favorites_store_interval_seconds);
    APPEND_BOOL("confirm_favorites_deletion", confirm_favorites_deletion);
    APPEND_BOOL("always_expand_optional_parts", always_expand_optional_parts);
    APPEND_BOOL("collapse_large_articles", collapse_large_articles);
    APPEND_NUMBER("article_size_limit", article_size_limit);
    APPEND_BOOL("limit_input_phrase_length", limit_input_phrase_length);
    APPEND_NUMBER("input_phrase_length_limit", input_phrase_length_limit);
    APPEND_NUMBER("maximum_dictionary_references",
                  maximum_dictionary_references);
    APPEND_BOOL("synonym_search_enabled", synonym_search_enabled);
    APPEND_BOOL("full_text_search_enabled", full_text_search_enabled);
    AppendPreference(
        contents, "full_text_search_mode",
        FormatInteger(static_cast<std::uint8_t>(p.full_text_search_mode)));
    APPEND_BOOL("full_text_match_case", full_text_match_case);
    APPEND_NUMBER("full_text_maximum_articles_per_dictionary",
                  full_text_maximum_articles_per_dictionary);
    APPEND_NUMBER("full_text_maximum_word_distance",
                  full_text_maximum_word_distance);
    APPEND_BOOL("full_text_use_maximum_word_distance",
                full_text_use_maximum_word_distance);
    APPEND_BOOL("full_text_use_maximum_articles",
                full_text_use_maximum_articles);
    APPEND_BOOL("full_text_ignore_word_order", full_text_ignore_word_order);
    APPEND_BOOL("full_text_ignore_diacritics", full_text_ignore_diacritics);
    APPEND_NUMBER("full_text_maximum_dictionary_megabytes",
                  full_text_maximum_dictionary_megabytes);
    APPEND_STRING("full_text_disabled_types", full_text_disabled_types);
#undef APPEND_STRING
#undef APPEND_BOOL
#undef APPEND_NUMBER
    if (contents.size() > kMaximumConfigurationBytes) {
        throw std::runtime_error("Configuration exceeds the size limit");
    }

    const std::filesystem::path destination(configuration_path);
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    const auto temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write configuration file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot replace configuration file: " +
                                 error.message());
    }
}

}  // namespace goldendict::core
