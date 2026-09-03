// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include "../foundation/utf8.h"
#include "article_tab_session.h"
#include "input_phrase.h"

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
constexpr std::size_t kMaximumDictionaryGroups = 256U;
constexpr std::size_t kMaximumDictionariesPerGroup = 256U;
constexpr std::size_t kMaximumGroupValueBytes = 4096U;
constexpr std::size_t kMaximumEncodedGroupIconBytes = 64U * 1024U;
constexpr std::size_t kMaximumPreferenceStringBytes = 4096U;
constexpr std::size_t kMaximumFullTextDialogGeometryBytes = 64U * 1024U;
constexpr std::size_t kMaximumMainWindowGeometryBytes = 64U * 1024U;
constexpr std::size_t kMaximumMainWindowStateBytes = 64U * 1024U;
constexpr std::size_t kMaximumOnlineIdBytes = 128U;
constexpr std::size_t kMaximumOnlineNameBytes = 256U;
constexpr std::size_t kMaximumOnlineUrlBytes = 4096U;
constexpr std::size_t kMaximumForvoLanguages = 32U;
constexpr std::size_t kMaximumExternalProgramPathBytes = 4096U;
constexpr std::size_t kMaximumExternalProgramArgumentBytes = 16U * 1024U;
constexpr std::uint32_t kKnownScanPopupModifierMask = 0x03ffU;

std::string Encode(std::string_view value);
std::string Decode(std::string_view value);

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

bool SetPreference(ApplicationPreferences& preferences, std::string_view name,
                   std::string value) {
#define STRING_PREFERENCE(key, member)         \
    if (name == key) {                         \
        preferences.member = std::move(value); \
        return true;                           \
    }
#define BOOL_PREFERENCE(key, member)              \
    if (name == key) {                            \
        preferences.member = ParseBoolean(value); \
        return true;                              \
    }
#define UINT_PREFERENCE(key, member, type)              \
    if (name == key) {                                  \
        preferences.member = ParseInteger<type>(value); \
        return true;                                    \
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
    BOOL_PREFERENCE("open_new_tabs_after_current", open_new_tabs_after_current)
    BOOL_PREFERENCE("open_new_tabs_in_background", open_new_tabs_in_background)
    BOOL_PREFERENCE("hide_single_tab", hide_single_tab)
    BOOL_PREFERENCE("mru_tab_order", mru_tab_order)
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
                    full_text_maximum_dictionary_articles, std::uint32_t)
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
            ParseEnum<FullTextSearchMode>(value, 3U);
    } else if (name == "zoom_factor") {
        preferences.zoom_factor = ParseDouble(value);
    } else if (name == "help_zoom_factor") {
        preferences.help_zoom_factor = ParseDouble(value);
    } else {
        return false;
    }
    return true;
}

bool IsOpaqueField(std::string_view line) {
    const auto separator = line.find('=');
    return separator != std::string_view::npos && separator != 0U &&
           std::all_of(line.begin(), line.begin() + separator,
                       [](unsigned char character) {
                           return (character >= 'a' && character <= 'z') ||
                                  (character >= '0' && character <= '9') ||
                                  character == '_';
                       });
}

bool IsKnownRecordKey(std::string_view key) {
    constexpr std::string_view keys[] = {
        "index_directory",
        "dictionary_path",
        "morphology_dictionary_path",
        "enabled_morphology_dictionary",
        "sound_directory",
        "mediawiki_source",
        "website_source",
        "forvo_sources",
        "forvo_source",
        "dict_server_source",
        "external_programs",
        "external_program",
        "external_program_argument",
        "dictionary_group",
        "dictionary_group_metadata",
        "preference",
        "full_text_dialog_geometry",
        "main_window_geometry",
        "main_window_state",
        "article_tab_session",
        "article_tab",
        "article_tab_navigation",
    };
    return std::find(std::begin(keys), std::end(keys), key) != std::end(keys);
}

bool IsUnknownPreferenceRecord(std::string_view line) {
    constexpr std::string_view prefix = "preference=";
    if (line.substr(0U, prefix.size()) != prefix) {
        return false;
    }
    const auto value = line.substr(prefix.size());
    const auto separator = value.find('|');
    if (separator == std::string_view::npos ||
        value.find('|', separator + 1U) != std::string_view::npos) {
        throw std::runtime_error("Malformed opaque preference field");
    }
    ApplicationPreferences scratch;
    return !SetPreference(scratch, Decode(value.substr(0U, separator)),
                          Decode(value.substr(separator + 1U)));
}

bool HasLegacyWebsiteMarker(std::string_view value) {
    return value.find("%GD1251%") != std::string_view::npos ||
           value.find("%GDISO1%") != std::string_view::npos;
}

std::string SubstituteWebsiteMarkers(std::string value) {
    constexpr std::string_view markers[] = {"%GDWORD%", "%GD1251%", "%GDISO1%"};
    bool replaced = false;
    for (const auto marker : markers) {
        for (std::size_t position = value.find(marker);
             position != std::string::npos; position = value.find(marker)) {
            value.replace(position, marker.size(), "validation");
            replaced = true;
        }
    }
    if (!replaced) {
        throw std::runtime_error("Website source template is invalid");
    }
    return value;
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

std::vector<std::string_view> SplitFields(std::string_view value) {
    std::vector<std::string_view> fields;
    std::size_t start = 0U;
    while (start <= value.size()) {
        const auto separator = value.find('|', start);
        fields.push_back(value.substr(start, separator == std::string_view::npos
                                                 ? value.size() - start
                                                 : separator - start));
        if (separator == std::string_view::npos)
            break;
        start = separator + 1U;
    }
    return fields;
}

bool IsValidOnlineId(std::string_view value) {
    if (value.empty() || value.size() > kMaximumOnlineIdBytes ||
        !std::isalnum(static_cast<unsigned char>(value.front()))) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isalnum(ch) != 0 || ch == '.' || ch == '_' || ch == '-';
    });
}

bool IsValidText(std::string_view value, std::size_t maximum,
                 bool allow_empty = false) {
    return (allow_empty || !value.empty()) && value.size() <= maximum &&
           foundation::IsValidUtf8(value) &&
           !foundation::ContainsControlCharacter(value);
}

bool IsValidIpv4(std::string_view value) {
    unsigned int parts = 0U;
    std::size_t start = 0U;
    while (start <= value.size()) {
        const auto end = value.find('.', start);
        const auto part = value.substr(start, end == std::string_view::npos
                                                  ? value.size() - start
                                                  : end - start);
        if (part.empty() || part.size() > 3U ||
            (part.size() > 1U && part.front() == '0'))
            return false;
        unsigned int number = 0U;
        const auto parsed =
            std::from_chars(part.data(), part.data() + part.size(), number, 10);
        if (parsed.ec != std::errc() ||
            parsed.ptr != part.data() + part.size() || number > 255U)
            return false;
        ++parts;
        if (end == std::string_view::npos)
            break;
        start = end + 1U;
    }
    return parts == 4U;
}

bool IsValidIpv6(std::string_view value) {
    if (value.empty() || value.find(":::") != std::string_view::npos)
        return false;
    const auto compressed = value.find("::");
    if (compressed != std::string_view::npos &&
        value.find("::", compressed + 2U) != std::string_view::npos)
        return false;
    unsigned int groups = 0U;
    std::size_t start = 0U;
    while (start < value.size()) {
        if (value[start] == ':') {
            ++start;
            continue;
        }
        const auto end = value.find(':', start);
        const auto part = value.substr(start, end == std::string_view::npos
                                                  ? value.size() - start
                                                  : end - start);
        if (part.find('.') != std::string_view::npos) {
            if (end != std::string_view::npos || !IsValidIpv4(part))
                return false;
            groups += 2U;
        } else {
            if (part.empty() || part.size() > 4U ||
                !std::all_of(part.begin(), part.end(), [](unsigned char ch) {
                    return std::isxdigit(ch) != 0;
                }))
                return false;
            ++groups;
        }
        if (end == std::string_view::npos)
            break;
        start = end + 1U;
    }
    return compressed == std::string_view::npos ? groups == 8U : groups < 8U;
}

bool IsValidHost(std::string_view host) {
    if (host.empty() || host.size() > 253U || !foundation::IsValidUtf8(host) ||
        foundation::ContainsControlCharacter(host) ||
        std::any_of(host.begin(), host.end(),
                    [](unsigned char ch) { return std::isspace(ch) != 0; }))
        return false;
    if (host.find(':') != std::string_view::npos) {
        return IsValidIpv6(host);
    }
    if (IsValidIpv4(host))
        return true;
    if (host.front() == '.' || host.back() == '.')
        return false;
    std::size_t start = 0U;
    while (start < host.size()) {
        const auto end = host.find('.', start);
        const auto label = host.substr(start, end == std::string_view::npos
                                                  ? host.size() - start
                                                  : end - start);
        if (label.empty() || label.size() > 63U || label.front() == '-' ||
            label.back() == '-' ||
            !std::all_of(label.begin(), label.end(), [](unsigned char ch) {
                return std::isalnum(ch) != 0 || ch == '-';
            }))
            return false;
        if (end == std::string_view::npos)
            break;
        start = end + 1U;
    }
    return true;
}

bool IsValidHttpUrl(std::string_view value, bool allow_query,
                    bool allow_fragment) {
    if (!IsValidText(value, kMaximumOnlineUrlBytes))
        return false;
    std::size_t authority = 0U;
    if (value.substr(0U, 7U) == "http://")
        authority = 7U;
    else if (value.substr(0U, 8U) == "https://")
        authority = 8U;
    else
        return false;
    const auto authority_end = value.find_first_of("/?#", authority);
    const auto authority_value =
        value.substr(authority, authority_end == std::string_view::npos
                                    ? value.size() - authority
                                    : authority_end - authority);
    if (authority_value.empty() ||
        authority_value.find('@') != std::string_view::npos)
        return false;
    std::string_view host = authority_value;
    if (host.front() == '[') {
        const auto close = host.find(']');
        if (close == std::string_view::npos)
            return false;
        const auto suffix = host.substr(close + 1U);
        if (!suffix.empty() && (suffix.front() != ':' || suffix.size() == 1U ||
                                !std::all_of(suffix.begin() + 1, suffix.end(),
                                             [](unsigned char ch) {
                                                 return std::isdigit(ch) != 0;
                                             })))
            return false;
        if (!suffix.empty()) {
            unsigned int port = 0U;
            const auto text = suffix.substr(1U);
            const auto parsed = std::from_chars(
                text.data(), text.data() + text.size(), port, 10);
            if (parsed.ec != std::errc() || port == 0U || port > 65535U)
                return false;
        }
        host = host.substr(1U, close - 1U);
    } else {
        const auto colon = host.rfind(':');
        if (colon != std::string_view::npos) {
            const auto port = host.substr(colon + 1U);
            if (port.empty() ||
                !std::all_of(port.begin(), port.end(), [](unsigned char ch) {
                    return std::isdigit(ch) != 0;
                }))
                return false;
            unsigned int number = 0U;
            const auto parsed = std::from_chars(
                port.data(), port.data() + port.size(), number, 10);
            if (parsed.ec != std::errc() || number == 0U || number > 65535U)
                return false;
            host = host.substr(0U, colon);
        }
    }
    if (!IsValidHost(host))
        return false;
    return (allow_query || value.find('?') == std::string_view::npos) &&
           (allow_fragment || value.find('#') == std::string_view::npos);
}

bool IsDictAtom(std::string_view value) {
    return !value.empty() && value.size() <= 128U &&
           std::all_of(value.begin(), value.end(), [](unsigned char ch) {
               return std::isalnum(ch) != 0 || ch == '*' || ch == '-' ||
                      ch == '_' || ch == '.' || ch == '!';
           });
}

bool IsValidAbsolutePath(std::string_view value, bool allow_empty = false) {
    if (value.empty())
        return allow_empty;
    return IsValidText(value, kMaximumExternalProgramPathBytes) &&
           std::filesystem::path(value).is_absolute();
}

void ValidateConfigurationImpl(const CoreConfiguration& configuration) {
    if (configuration.dictionary_paths.size() > kMaximumDictionaryPaths) {
        throw std::runtime_error("Configuration has too many dictionary paths");
    }
    if (configuration.enabled_morphology_dictionary_ids.size() >
        kMaximumMorphologyDictionaries) {
        throw std::runtime_error(
            "Configuration has too many enabled morphology dictionaries");
    }
    if (configuration.sound_directories.size() > kMaximumSoundDirectories) {
        throw std::runtime_error(
            "Configuration has too many sound directories");
    }
    if (configuration.mediawiki_sources.size() > kMaximumOnlineSources ||
        configuration.website_sources.size() > kMaximumOnlineSources ||
        configuration.forvo_sources.size() > kMaximumOnlineSources ||
        configuration.dict_server_sources.size() > kMaximumOnlineSources ||
        configuration.external_program_sources.size() > kMaximumOnlineSources) {
        throw std::runtime_error("Configuration has too many online sources");
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
    if (configuration.article_tab_session.has_value()) {
        for (const auto& tab : configuration.article_tab_session->tabs) {
            if (std::any_of(
                    tab.history.begin(), tab.history.end(),
                    [&configuration](const TabNavigationState& navigation) {
                        return !application::IsInputPhraseAccepted(
                            navigation.query, configuration.preferences);
                    })) {
                throw std::runtime_error(
                    "Article tab session contains an input phrase that "
                    "exceeds the configured symbol limit");
            }
        }
    }
    if (configuration.full_text_dialog_geometry.size() >
        kMaximumFullTextDialogGeometryBytes) {
        throw std::runtime_error("Full-text dialog geometry is too large");
    }
    if (configuration.main_window_geometry.size() >
        kMaximumMainWindowGeometryBytes) {
        throw std::runtime_error("Main-window geometry is too large");
    }
    if (configuration.main_window_state.size() > kMaximumMainWindowStateBytes) {
        throw std::runtime_error("Main-window state is too large");
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
        has_nul(configuration.morphology_dictionary_path) ||
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
    std::unordered_set<std::string> morphology_ids;
    for (const auto& id : configuration.enabled_morphology_dictionary_ids) {
        if (id.empty() || id.size() > 256U || has_nul(id) ||
            !foundation::IsValidUtf8(id) || !morphology_ids.insert(id).second) {
            throw std::runtime_error(
                "Enabled morphology dictionary identity is invalid");
        }
    }

    std::unordered_set<std::string> online_ids;
    const auto validate_identity = [&online_ids](const auto& source) {
        if (!IsValidOnlineId(source.id) ||
            !IsValidText(source.name, kMaximumOnlineNameBytes) ||
            !online_ids.insert(source.id).second) {
            throw std::runtime_error("Online source identity is invalid");
        }
    };
    for (const auto& source : configuration.mediawiki_sources) {
        validate_identity(source);
        if (!IsValidHttpUrl(source.base_url, false, false)) {
            throw std::runtime_error("MediaWiki source URL is invalid");
        }
    }
    for (const auto& source : configuration.website_sources) {
        validate_identity(source);
        const bool deferred_legacy_behavior =
            source.inside_iframe || HasLegacyWebsiteMarker(source.url_template);
        if (source.enabled && deferred_legacy_behavior) {
            throw std::runtime_error(
                "Enabled website requires unsupported legacy behavior");
        }
        const std::string substituted =
            SubstituteWebsiteMarkers(source.url_template);
        if (!IsValidHttpUrl(substituted, true, true)) {
            throw std::runtime_error("Website source template is invalid");
        }
    }
    for (const auto& source : configuration.forvo_sources) {
        validate_identity(source);
        if (!IsValidHttpUrl(source.api_base_url, true, true) ||
            (source.enabled && source.language_codes.empty()) ||
            source.language_codes.size() > kMaximumForvoLanguages) {
            throw std::runtime_error("Forvo source configuration is invalid");
        }
        std::unordered_set<std::string> languages;
        for (const auto& language : source.language_codes) {
            if (language.size() < 2U || language.size() > 16U ||
                !std::all_of(language.begin(), language.end(),
                             [](char ch) {
                                 return (ch >= 'A' && ch <= 'Z') ||
                                        (ch >= 'a' && ch <= 'z') || ch == '-';
                             }) ||
                !languages.insert(language).second) {
                throw std::runtime_error("Forvo language code is invalid");
            }
        }
    }
    for (const auto& source : configuration.dict_server_sources) {
        validate_identity(source);
        if (!IsValidHost(source.host) || source.port == 0U ||
            !IsDictAtom(source.database) || !IsDictAtom(source.strategy)) {
            throw std::runtime_error("DICT server configuration is invalid");
        }
    }
    for (const auto& source : configuration.external_program_sources) {
        validate_identity(source);
        if (static_cast<std::uint8_t>(source.output_kind) >
                static_cast<std::uint8_t>(
                    ExternalProgramOutputKind::kPrefixMatch) ||
            !IsValidAbsolutePath(source.executable) ||
            !IsValidAbsolutePath(source.working_directory, true) ||
            source.argument_templates.size() >
                kMaximumExternalProgramArguments) {
            throw std::runtime_error(
                "External program configuration is invalid");
        }
        for (const auto& argument : source.argument_templates) {
            if (!IsValidText(argument, kMaximumExternalProgramArgumentBytes,
                             true)) {
                throw std::runtime_error(
                    "External program argument template is invalid");
            }
        }
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
        (preferences.proxy_host.empty() || preferences.proxy_port == 0U ||
         preferences.proxy_host.find_first_of("\r\n") != std::string::npos)) {
        throw std::runtime_error("Manual proxy requires a host and port");
    }
    if (!std::isfinite(preferences.zoom_factor) ||
        preferences.zoom_factor < 0.1 || preferences.zoom_factor > 5.0 ||
        !std::isfinite(preferences.help_zoom_factor) ||
        preferences.help_zoom_factor < 0.25 ||
        preferences.help_zoom_factor > 5.0 ||
        preferences.words_zoom_level < -10 ||
        preferences.words_zoom_level > 10 ||
        preferences.maximum_network_cache_megabytes > 10240U ||
        preferences.maximum_history_entries > 99999U ||
        preferences.history_store_interval_seconds > 86400U ||
        preferences.favorites_store_interval_seconds > 86400U ||
        preferences.article_size_limit == 0U ||
        preferences.article_size_limit > 1000000U ||
        preferences.input_phrase_length_limit == 0U ||
        preferences.input_phrase_length_limit > 1000000U ||
        preferences.maximum_dictionary_references > 9999U ||
        static_cast<std::uint8_t>(preferences.full_text_search_mode) > 3U ||
        preferences.full_text_maximum_articles_per_dictionary == 0U ||
        preferences.full_text_maximum_articles_per_dictionary > 100000U ||
        preferences.full_text_maximum_word_distance > 1000U ||
        preferences.full_text_maximum_dictionary_articles > 10000000U) {
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
    std::size_t opaque_bytes = 0U;
    for (const auto& field : configuration.opaque_fields) {
        opaque_bytes += field.size() + 1U;
        const auto separator = field.find('=');
        const bool unknown_preference = IsUnknownPreferenceRecord(field);
        if (field.empty() ||
            field.find_first_of("\r\n\0", 0U, 3U) != std::string::npos ||
            !foundation::IsValidUtf8(field) || !IsOpaqueField(field) ||
            (!unknown_preference &&
             IsKnownRecordKey(field.substr(0U, separator))) ||
            opaque_bytes > kMaximumConfigurationBytes) {
            throw std::runtime_error("Opaque configuration field is invalid");
        }
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
    bool has_full_text_dialog_geometry = false;
    bool has_main_window_geometry = false;
    bool has_main_window_state = false;
    bool has_index_directory = false;
    bool has_morphology_dictionary_path = false;
    bool has_forvo_records = false;
    std::optional<std::size_t> declared_forvo_source_count;
    std::optional<std::size_t> declared_external_program_count;
    std::optional<std::size_t> current_external_program_index;
    std::size_t expected_external_program_arguments = 0U;
    std::size_t position = kHeader.size();
    while (position < contents.size()) {
        const auto end = contents.find('\n', position);
        if (end == std::string::npos) {
            throw std::runtime_error("Configuration line is not terminated");
        }
        const std::string_view line(contents.data() + position, end - position);
        constexpr std::string_view kIndex = "index_directory=";
        constexpr std::string_view kDictionary = "dictionary_path=";
        constexpr std::string_view kMorphologyPath =
            "morphology_dictionary_path=";
        constexpr std::string_view kEnabledMorphology =
            "enabled_morphology_dictionary=";
        constexpr std::string_view kSoundDirectory = "sound_directory=";
        constexpr std::string_view kMediaWikiSource = "mediawiki_source=";
        constexpr std::string_view kWebsiteSource = "website_source=";
        constexpr std::string_view kForvoSources = "forvo_sources=";
        constexpr std::string_view kForvoSource = "forvo_source=";
        constexpr std::string_view kDictServerSource = "dict_server_source=";
        constexpr std::string_view kExternalPrograms = "external_programs=";
        constexpr std::string_view kExternalProgram = "external_program=";
        constexpr std::string_view kExternalProgramArgument =
            "external_program_argument=";
        constexpr std::string_view kDictionaryGroup = "dictionary_group=";
        constexpr std::string_view kDictionaryGroupMetadata =
            "dictionary_group_metadata=";
        constexpr std::string_view kPreference = "preference=";
        constexpr std::string_view kFullTextDialogGeometry =
            "full_text_dialog_geometry=";
        constexpr std::string_view kMainWindowGeometry =
            "main_window_geometry=";
        constexpr std::string_view kMainWindowState = "main_window_state=";
        constexpr std::string_view kArticleTabSession = "article_tab_session=";
        constexpr std::string_view kArticleTab = "article_tab=";
        constexpr std::string_view kArticleTabNavigation =
            "article_tab_navigation=";
        if (expected_external_program_arguments != 0U &&
            line.substr(0, kExternalProgramArgument.size()) !=
                kExternalProgramArgument) {
            throw std::runtime_error(
                "External program arguments must follow their parent");
        }
        if (line.substr(0, kFullTextDialogGeometry.size()) ==
            kFullTextDialogGeometry) {
            if (has_full_text_dialog_geometry) {
                throw std::runtime_error("Duplicate full-text dialog geometry");
            }
            has_full_text_dialog_geometry = true;
            configuration.full_text_dialog_geometry =
                Decode(line.substr(kFullTextDialogGeometry.size()));
        } else if (line.substr(0, kMainWindowState.size()) ==
                   kMainWindowState) {
            if (has_main_window_state) {
                throw std::runtime_error("Duplicate main-window state");
            }
            has_main_window_state = true;
            configuration.main_window_state =
                Decode(line.substr(kMainWindowState.size()));
        } else if (line.substr(0, kMainWindowGeometry.size()) ==
                   kMainWindowGeometry) {
            if (has_main_window_geometry) {
                throw std::runtime_error("Duplicate main-window geometry");
            }
            has_main_window_geometry = true;
            configuration.main_window_geometry =
                Decode(line.substr(kMainWindowGeometry.size()));
        } else if (line.substr(0, kIndex.size()) == kIndex) {
            if (has_index_directory) {
                throw std::runtime_error("Duplicate index directory");
            }
            has_index_directory = true;
            configuration.index_directory = Decode(line.substr(kIndex.size()));
        } else if (line.substr(0, kMorphologyPath.size()) == kMorphologyPath) {
            if (has_morphology_dictionary_path) {
                throw std::runtime_error(
                    "Duplicate morphology dictionary path");
            }
            has_morphology_dictionary_path = true;
            configuration.morphology_dictionary_path =
                Decode(line.substr(kMorphologyPath.size()));
        } else if (line.substr(0, kEnabledMorphology.size()) ==
                   kEnabledMorphology) {
            configuration.enabled_morphology_dictionary_ids.push_back(
                Decode(line.substr(kEnabledMorphology.size())));
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
        } else if (line.substr(0, kMediaWikiSource.size()) ==
                   kMediaWikiSource) {
            const auto fields =
                SplitFields(line.substr(kMediaWikiSource.size()));
            if (fields.size() != 4U) {
                throw std::runtime_error("Malformed MediaWiki source field");
            }
            configuration.mediawiki_sources.push_back(
                {Decode(fields[0]), Decode(fields[1]), ParseBoolean(fields[2]),
                 Decode(fields[3])});
        } else if (line.substr(0, kWebsiteSource.size()) == kWebsiteSource) {
            const auto fields = SplitFields(line.substr(kWebsiteSource.size()));
            if (fields.size() != 4U && fields.size() != 5U) {
                throw std::runtime_error("Malformed website source field");
            }
            configuration.website_sources.push_back(
                {Decode(fields[0]), Decode(fields[1]), ParseBoolean(fields[2]),
                 Decode(fields[3]),
                 fields.size() == 5U ? ParseBoolean(fields[4]) : false});
        } else if (line.substr(0, kForvoSources.size()) == kForvoSources) {
            if (declared_forvo_source_count.has_value() || has_forvo_records) {
                throw std::runtime_error(
                    "Duplicate or misplaced Forvo source count");
            }
            const auto count =
                ParseInteger<std::size_t>(line.substr(kForvoSources.size()));
            if (count > kMaximumOnlineSources) {
                throw std::runtime_error("Forvo source count is too large");
            }
            declared_forvo_source_count = count;
            configuration.forvo_sources.clear();
        } else if (line.substr(0, kForvoSource.size()) == kForvoSource) {
            const auto fields = SplitFields(line.substr(kForvoSource.size()));
            if (fields.size() < 5U) {
                throw std::runtime_error("Malformed Forvo source field");
            }
            const auto count = ParseInteger<std::size_t>(fields[4]);
            if (count > kMaximumForvoLanguages || fields.size() != 5U + count) {
                throw std::runtime_error("Malformed Forvo source field");
            }
            if (!has_forvo_records) {
                configuration.forvo_sources.clear();
                has_forvo_records = true;
            }
            ForvoSourceConfiguration source{Decode(fields[0]),
                                            Decode(fields[1]),
                                            ParseBoolean(fields[2]),
                                            Decode(fields[3]),
                                            {}};
            for (std::size_t index = 5U; index < fields.size(); ++index) {
                source.language_codes.push_back(Decode(fields[index]));
            }
            configuration.forvo_sources.push_back(std::move(source));
        } else if (line.substr(0, kDictServerSource.size()) ==
                   kDictServerSource) {
            const auto fields =
                SplitFields(line.substr(kDictServerSource.size()));
            if (fields.size() != 7U) {
                throw std::runtime_error("Malformed DICT server source field");
            }
            configuration.dict_server_sources.push_back(
                {Decode(fields[0]), Decode(fields[1]), ParseBoolean(fields[2]),
                 Decode(fields[3]), ParseInteger<std::uint16_t>(fields[4]),
                 Decode(fields[5]), Decode(fields[6])});
        } else if (line.substr(0, kExternalPrograms.size()) ==
                   kExternalPrograms) {
            if (declared_external_program_count.has_value() ||
                !configuration.external_program_sources.empty()) {
                throw std::runtime_error(
                    "Duplicate or misplaced external program count");
            }
            const auto count = ParseInteger<std::size_t>(
                line.substr(kExternalPrograms.size()));
            if (count > kMaximumOnlineSources) {
                throw std::runtime_error("External program count is too large");
            }
            declared_external_program_count = count;
        } else if (line.substr(0, kExternalProgram.size()) ==
                   kExternalProgram) {
            if (!declared_external_program_count.has_value()) {
                throw std::runtime_error(
                    "External program appears without a collection count");
            }
            const auto fields =
                SplitFields(line.substr(kExternalProgram.size()));
            if (fields.size() != 7U) {
                throw std::runtime_error("Malformed external program field");
            }
            const auto argument_count = ParseInteger<std::size_t>(fields[6]);
            if (argument_count > kMaximumExternalProgramArguments) {
                throw std::runtime_error(
                    "External program argument count is too large");
            }
            configuration.external_program_sources.push_back(
                {Decode(fields[0]),
                 Decode(fields[1]),
                 ParseBoolean(fields[2]),
                 static_cast<ExternalProgramOutputKind>(
                     ParseInteger<std::uint8_t>(fields[3])),
                 Decode(fields[4]),
                 {},
                 Decode(fields[5])});
            if (configuration.external_program_sources.size() >
                *declared_external_program_count) {
                throw std::runtime_error(
                    "External program count does not match records");
            }
            current_external_program_index =
                configuration.external_program_sources.size() - 1U;
            expected_external_program_arguments = argument_count;
        } else if (line.substr(0, kExternalProgramArgument.size()) ==
                   kExternalProgramArgument) {
            if (!current_external_program_index.has_value() ||
                expected_external_program_arguments == 0U) {
                throw std::runtime_error("Orphan external program argument");
            }
            const auto fields =
                SplitFields(line.substr(kExternalProgramArgument.size()));
            if (fields.size() != 3U) {
                throw std::runtime_error(
                    "Malformed external program argument field");
            }
            auto& source =
                configuration
                    .external_program_sources[*current_external_program_index];
            const auto index = ParseInteger<std::size_t>(fields[1]);
            if (Decode(fields[0]) != source.id ||
                index != source.argument_templates.size()) {
                throw std::runtime_error(
                    "External program argument order is invalid");
            }
            source.argument_templates.push_back(Decode(fields[2]));
            --expected_external_program_arguments;
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
            if (fields.size() != 10U && fields.size() < 12U) {
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
            if (fields.size() >= 12U) {
                if (fields[10] != "0" && fields[10] != "1") {
                    throw std::runtime_error(
                        "Malformed article tab navigation scope flag");
                }
                navigation.dictionary_filter_active = fields[10] == "1";
                const std::size_t dictionary_count =
                    ParseInteger<std::size_t>(fields[11]);
                if (dictionary_count > kMaximumLookupDictionaryFilters) {
                    throw std::runtime_error(
                        "Malformed article tab navigation scope count");
                }
                const std::size_t scope_end = 12U + dictionary_count;
                if (fields.size() < scope_end ||
                    (!navigation.dictionary_filter_active &&
                     dictionary_count != 0U)) {
                    throw std::runtime_error(
                        "Malformed article tab navigation scope count");
                }
                navigation.dictionary_ids.reserve(dictionary_count);
                for (std::size_t index = 12U; index < fields.size(); ++index) {
                    if (index == scope_end) {
                        break;
                    }
                    navigation.dictionary_ids.push_back(Decode(fields[index]));
                }
                if (fields.size() > scope_end) {
                    const auto exact_flag = fields[scope_end];
                    if (exact_flag == "0") {
                        if (fields.size() != scope_end + 1U) {
                            throw std::runtime_error(
                                "Malformed article exact target");
                        }
                    } else if (exact_flag == "1") {
                        if (fields.size() != scope_end + 3U) {
                            throw std::runtime_error(
                                "Malformed article exact target");
                        }
                        navigation.exact_target =
                            ExactArticleTarget{Decode(fields[scope_end + 1U]),
                                               Decode(fields[scope_end + 2U])};
                    } else {
                        throw std::runtime_error(
                            "Malformed article exact target flag");
                    }
                }
            }
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
            if (!SetPreference(configuration.preferences, name,
                               Decode(value.substr(separator + 1U)))) {
                configuration.opaque_fields.emplace_back(line);
            }
        } else if (!line.empty()) {
            if (!IsOpaqueField(line)) {
                throw std::runtime_error(
                    "Malformed unknown configuration field");
            }
            configuration.opaque_fields.emplace_back(line);
        }
        position = end + 1U;
    }
    if (declared_forvo_source_count.has_value() &&
        configuration.forvo_sources.size() != *declared_forvo_source_count) {
        throw std::runtime_error("Forvo source count does not match records");
    }
    if (expected_external_program_arguments != 0U ||
        (declared_external_program_count.has_value() &&
         configuration.external_program_sources.size() !=
             *declared_external_program_count)) {
        throw std::runtime_error(
            "External program count does not match records");
    }
    ValidateConfigurationImpl(configuration);
    return configuration;
}

void SaveConfiguration(const std::string& configuration_path,
                       const CoreConfiguration& configuration) {
    ValidateConfigurationImpl(configuration);
    std::string contents(kHeader);
    contents +=
        "index_directory=" + Encode(configuration.index_directory) + "\n";
    for (const auto& path : configuration.dictionary_paths) {
        contents += "dictionary_path=" + Encode(path) + "\n";
    }
    if (!configuration.morphology_dictionary_path.empty()) {
        contents += "morphology_dictionary_path=" +
                    Encode(configuration.morphology_dictionary_path) + "\n";
    }
    for (const auto& id : configuration.enabled_morphology_dictionary_ids) {
        contents += "enabled_morphology_dictionary=" + Encode(id) + "\n";
    }
    for (const auto& directory : configuration.sound_directories) {
        contents += "sound_directory=" + Encode(directory.path) + "|" +
                    Encode(directory.name) + "\n";
    }
    for (const auto& source : configuration.mediawiki_sources) {
        contents += "mediawiki_source=" + Encode(source.id) + "|" +
                    Encode(source.name) + "|" + (source.enabled ? "1|" : "0|") +
                    Encode(source.base_url) + "\n";
    }
    for (const auto& source : configuration.website_sources) {
        contents += "website_source=" + Encode(source.id) + "|" +
                    Encode(source.name) + "|" + (source.enabled ? "1|" : "0|") +
                    Encode(source.url_template) + "|" +
                    (source.inside_iframe ? "1\n" : "0\n");
    }
    contents +=
        "forvo_sources=" + std::to_string(configuration.forvo_sources.size()) +
        "\n";
    for (const auto& source : configuration.forvo_sources) {
        contents += "forvo_source=" + Encode(source.id) + "|" +
                    Encode(source.name) + "|" + (source.enabled ? "1|" : "0|") +
                    Encode(source.api_base_url) + "|" +
                    std::to_string(source.language_codes.size());
        for (const auto& language : source.language_codes) {
            contents += "|" + Encode(language);
        }
        contents += "\n";
    }
    for (const auto& source : configuration.dict_server_sources) {
        contents += "dict_server_source=" + Encode(source.id) + "|" +
                    Encode(source.name) + "|" + (source.enabled ? "1|" : "0|") +
                    Encode(source.host) + "|" +
                    std::to_string(static_cast<unsigned int>(source.port)) +
                    "|" + Encode(source.database) + "|" +
                    Encode(source.strategy) + "\n";
    }
    contents += "external_programs=" +
                std::to_string(configuration.external_program_sources.size()) +
                "\n";
    for (const auto& source : configuration.external_program_sources) {
        contents +=
            "external_program=" + Encode(source.id) + "|" +
            Encode(source.name) + "|" + (source.enabled ? "1|" : "0|") +
            std::to_string(static_cast<unsigned int>(source.output_kind)) +
            "|" + Encode(source.executable) + "|" +
            Encode(source.working_directory) + "|" +
            std::to_string(source.argument_templates.size()) + "\n";
        for (std::size_t index = 0U; index < source.argument_templates.size();
             ++index) {
            contents += "external_program_argument=" + Encode(source.id) + "|" +
                        std::to_string(index) + "|" +
                        Encode(source.argument_templates[index]) + "\n";
        }
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
                    Encode(navigation.target_anchor) + "|" +
                    (navigation.dictionary_filter_active ? "1|" : "0|") +
                    std::to_string(navigation.dictionary_ids.size());
                for (const auto& dictionary_id : navigation.dictionary_ids) {
                    contents += "|" + Encode(dictionary_id);
                }
                contents += navigation.exact_target.has_value() ? "|1" : "|0";
                if (navigation.exact_target.has_value()) {
                    contents +=
                        "|" + Encode(navigation.exact_target->dictionary_id) +
                        "|" + Encode(navigation.exact_target->document_id);
                }
                contents += "\n";
            }
        }
    }
    if (!configuration.full_text_dialog_geometry.empty()) {
        contents += "full_text_dialog_geometry=" +
                    Encode(configuration.full_text_dialog_geometry) + "\n";
    }
    if (!configuration.main_window_geometry.empty()) {
        contents += "main_window_geometry=" +
                    Encode(configuration.main_window_geometry) + "\n";
    }
    if (!configuration.main_window_state.empty()) {
        contents +=
            "main_window_state=" + Encode(configuration.main_window_state) +
            "\n";
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
    APPEND_BOOL("open_new_tabs_after_current", open_new_tabs_after_current);
    APPEND_BOOL("open_new_tabs_in_background", open_new_tabs_in_background);
    APPEND_BOOL("hide_single_tab", hide_single_tab);
    APPEND_BOOL("mru_tab_order", mru_tab_order);
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
                  full_text_maximum_dictionary_articles);
    APPEND_STRING("full_text_disabled_types", full_text_disabled_types);
#undef APPEND_STRING
#undef APPEND_BOOL
#undef APPEND_NUMBER
    for (const auto& field : configuration.opaque_fields) {
        contents += field + "\n";
    }
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

void ValidateConfiguration(const CoreConfiguration& configuration) {
    ValidateConfigurationImpl(configuration);
}

}  // namespace goldendict::core
