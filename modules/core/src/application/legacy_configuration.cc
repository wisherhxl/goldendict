// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <expat.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace goldendict::core {
namespace {

constexpr std::size_t kMaximumLegacyConfigurationBytes = 1024U * 1024U;
constexpr std::size_t kMaximumLegacyValueBytes = 64U * 1024U;
constexpr std::size_t kMaximumXmlDepth = 64U;
constexpr std::size_t kMaximumImportedPaths = 256U;
constexpr std::size_t kMaximumDictionaryGroups = 256U;
constexpr std::size_t kMaximumDictionariesPerGroup = 256U;
constexpr std::size_t kMaximumGroupValueBytes = 4096U;
constexpr std::size_t kMaximumEncodedGroupIconBytes = 64U * 1024U;
constexpr std::size_t kMaximumPreferenceValueBytes = 4096U;
constexpr std::size_t kMaximumMainWindowGeometryBytes = 64U * 1024U;
constexpr std::size_t kMaximumEncodedMainWindowGeometryBytes =
    ((kMaximumMainWindowGeometryBytes + 2U) / 3U) * 4U;
constexpr std::uint32_t kKnownScanPopupModifierMask = 0x03ffU;

enum class SourceContainer : std::uint8_t {
    kForvo,
    kMediaWikis,
    kWebsites,
    kDictServers,
    kPrograms,
    kCount
};

struct ParserDeleter {
    void operator()(XML_Parser parser) const noexcept {
        XML_ParserFree(parser);
    }
};

struct LegacyParserState {
    XML_Parser parser = nullptr;
    CoreConfiguration configuration;
    std::vector<std::string> elements;
    std::string value;
    std::string sound_name;
    DictionaryGroupConfiguration group;
    std::unordered_set<std::uint32_t> group_ids;
    std::unordered_set<std::string> preference_keys;
    std::string preference_key;
    bool reading_main_window_geometry = false;
    bool has_main_window_geometry = false;
    bool source_containers[static_cast<std::size_t>(SourceContainer::kCount)] =
        {};
    std::unordered_set<std::string> online_ids;
    std::unordered_set<std::string> forvo_fields;
    std::string forvo_field;
    bool failed = false;
    std::string error;
};

template <typename Integer>
Integer ParseInteger(std::string_view value);

bool IsDirectElement(const LegacyParserState& state, std::string_view parent,
                     std::string_view element) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == parent && state.elements[2] == element;
}

bool IsContainer(const LegacyParserState& state, std::string_view name) {
    return state.elements.size() == 2U && state.elements[0] == "config" &&
           state.elements[1] == name;
}

std::unordered_map<std::string, std::string> ReadAttributes(
    const XML_Char** attributes,
    std::initializer_list<std::string_view> allowed) {
    std::unordered_map<std::string, std::string> result;
    for (std::size_t index = 0U; attributes[index] != nullptr; index += 2U) {
        const std::string_view key(attributes[index]);
        const std::string_view value(attributes[index + 1U]);
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end() ||
            value.size() > kMaximumLegacyValueBytes ||
            !result.emplace(key, value).second) {
            throw std::runtime_error("invalid source attribute");
        }
    }
    return result;
}

const std::string& RequiredAttribute(
    const std::unordered_map<std::string, std::string>& attributes,
    std::string_view name) {
    const auto found = attributes.find(std::string(name));
    if (found == attributes.end())
        throw std::runtime_error("missing source attribute");
    return found->second;
}

void AddOnlineId(LegacyParserState& state, const std::string& id) {
    if (!state.online_ids.insert(id).second)
        throw std::runtime_error("duplicate online source ID");
}

std::pair<std::string, std::uint16_t> ParseDictUrl(std::string_view value) {
    constexpr std::string_view kScheme = "dict://";
    if (value.substr(0U, kScheme.size()) == kScheme)
        value.remove_prefix(kScheme.size());
    else if (value.find("://") != std::string_view::npos)
        throw std::runtime_error("unsupported DICT URL scheme");
    if (value.empty() || value.find_first_of("/@?#") != std::string_view::npos)
        throw std::runtime_error("invalid DICT URL");
    std::uint16_t port = 2628U;
    const auto colon = value.rfind(':');
    if (colon != std::string_view::npos) {
        if (value.find(':') != colon)
            throw std::runtime_error("unsupported DICT host syntax");
        port = ParseInteger<std::uint16_t>(value.substr(colon + 1U));
        if (port == 0U)
            throw std::runtime_error("invalid DICT port");
        value = value.substr(0U, colon);
    }
    if (value.empty())
        throw std::runtime_error("empty DICT host");
    return {std::string(value), port};
}

std::string ParseSingleDictAtom(std::string_view value,
                                std::string_view default_value) {
    std::vector<std::string> atoms;
    std::size_t start = 0U;
    for (std::size_t index = 0U; index <= value.size(); ++index) {
        if (index != value.size() && value[index] != ' ' &&
            value[index] != ',' && value[index] != ';') {
            continue;
        }
        if (index > start)
            atoms.emplace_back(value.substr(start, index - start));
        start = index + 1U;
    }
    if (atoms.empty())
        return std::string(default_value);
    if (atoms.size() != 1U)
        throw std::runtime_error("unrepresentable DICT list");
    return std::move(atoms.front());
}

std::vector<std::string> ParseLegacyCommandLine(std::string_view command_line) {
    std::vector<std::string> arguments;
    bool open_quote = false;
    bool possible_double_quote = false;
    bool start_new = true;
    std::size_t index = 0U;
    while (index < command_line.size()) {
        const char character = command_line[index];
        if (character == '"' && !possible_double_quote) {
            ++index;
            if (!open_quote) {
                open_quote = true;
                if (start_new) {
                    arguments.emplace_back();
                    start_new = false;
                }
            } else {
                possible_double_quote = true;
            }
        } else if (possible_double_quote && character != '"') {
            open_quote = false;
            possible_double_quote = false;
        } else if (character == ' ' && !open_quote) {
            ++index;
            start_new = true;
        } else {
            if (start_new) {
                arguments.emplace_back();
                start_new = false;
            }
            arguments.back().push_back(character);
            ++index;
            possible_double_quote = false;
        }
    }
    return arguments;
}

bool IsShellExecutable(const std::filesystem::path& executable) {
    std::string name = executable.filename().string();
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return name == "sh" || name == "bash" || name == "dash" || name == "zsh" ||
           name == "fish" || name == "csh" || name == "tcsh" || name == "cmd" ||
           name == "cmd.exe" || name == "powershell" ||
           name == "powershell.exe" || name == "pwsh" || name == "pwsh.exe";
}

bool IsSourceRecord(const LegacyParserState& state) {
    return IsDirectElement(state, "mediawikis", "mediawiki") ||
           IsDirectElement(state, "websites", "website") ||
           IsDirectElement(state, "dictservers", "server") ||
           IsDirectElement(state, "programs", "program");
}

bool IsSourceContainerName(std::string_view name) {
    return name == "forvo" || name == "mediawikis" || name == "websites" ||
           name == "dictservers" || name == "programs";
}

bool HasUnsupportedWebsiteMarker(std::string_view value) {
    constexpr std::string_view kSupported = "%GDWORD%";
    std::size_t position = 0U;
    while ((position = value.find("%GD", position)) != std::string_view::npos) {
        if (value.substr(position, kSupported.size()) != kSupported)
            return true;
        position += kSupported.size();
    }
    return false;
}

template <typename Integer>
Integer ParseInteger(std::string_view value) {
    Integer parsed{};
    const auto conversion =
        std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (value.empty() || conversion.ec != std::errc() ||
        conversion.ptr != value.data() + value.size()) {
        throw std::runtime_error("invalid integer");
    }
    return parsed;
}

bool ParseBoolean(std::string_view value) {
    if (value == "0")
        return false;
    if (value == "1")
        return true;
    throw std::runtime_error("invalid boolean");
}

double ParseDouble(std::string_view value) {
    double parsed = 0.0;
    const auto conversion =
        std::from_chars(value.data(), value.data() + value.size(), parsed,
                        std::chars_format::general);
    if (value.empty() || conversion.ec != std::errc() ||
        conversion.ptr != value.data() + value.size() ||
        !std::isfinite(parsed)) {
        throw std::runtime_error("invalid floating-point value");
    }
    return parsed;
}

bool IsPreferenceContainer(const LegacyParserState& state,
                           std::string_view name) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "preferences" &&
           (name == "proxyserver" || name == "fullTextSearch");
}

std::string PreferenceKey(const LegacyParserState& state) {
    if (state.elements.size() == 3U && state.elements[0] == "config" &&
        state.elements[1] == "preferences") {
        return state.elements[2];
    }
    if (state.elements.size() == 4U && state.elements[0] == "config" &&
        state.elements[1] == "preferences" &&
        (state.elements[2] == "proxyserver" ||
         state.elements[2] == "fullTextSearch")) {
        return state.elements[2] + "." + state.elements[3];
    }
    return {};
}

bool IsRecognizedPreference(std::string_view key) {
    static const std::unordered_set<std::string> keys = {
        "interfaceLanguage",
        "helpLanguage",
        "displayStyle",
        "addonStyle",
        "newTabsOpenAfterCurrentOne",
        "newTabsOpenInBackground",
        "hideSingleTab",
        "hideMenubar",
        "enableTrayIcon",
        "startToTray",
        "closeToTray",
        "autoStart",
        "doubleClickTranslates",
        "selectWordBySingleClick",
        "escKeyHidesMainWindow",
        "alwaysOnTop",
        "searchInDock",
        "enableMainWindowHotkey",
        "mainWindowHotkey",
        "enableClipboardHotkey",
        "clipboardHotkey",
        "enableScanPopup",
        "startWithScanPopupOn",
        "enableScanPopupModifiers",
        "scanPopupModifiers",
        "scanPopupAltMode",
        "scanPopupAltModeSecs",
        "ignoreOwnClipboardChanges",
        "scanPopupUseUIAutomation",
        "scanPopupUseIAccessibleEx",
        "scanPopupUseGDMessage",
        "scanPopupUnpinnedWindowFlags",
        "scanToMainWindow",
        "ignoreDiacritics",
        "showScanFlag",
        "trackClipboardChanges",
        "pronounceOnLoadMain",
        "pronounceOnLoadPopup",
        "useInternalPlayer",
        "internalPlayerBackend",
        "audioPlaybackProgram",
        "checkForNewReleases",
        "disallowContentFromOtherSites",
        "enableWebPlugins",
        "hideGoldenDictHeader",
        "maxNetworkCacheSize",
        "clearNetworkCacheOnExit",
        "zoomFactor",
        "helpZoomFactor",
        "wordsZoomLevel",
        "maxStringsInHistory",
        "storeHistory",
        "historyStoreInterval",
        "favoritesStoreInterval",
        "confirmFavoritesDeletion",
        "alwaysExpandOptionalParts",
        "collapseBigArticles",
        "articleSizeLimit",
        "limitInputPhraseLength",
        "inputPhraseLengthLimit",
        "maxDictionaryRefsInContextMenu",
        "synonymSearchEnabled",
        "proxyserver.type",
        "proxyserver.host",
        "proxyserver.port",
        "fullTextSearch.searchMode",
        "fullTextSearch.matchCase",
        "fullTextSearch.maxArticlesPerDictionary",
        "fullTextSearch.maxDistanceBetweenWords",
        "fullTextSearch.useMaxArticlesPerDictionary",
        "fullTextSearch.useMaxDistanceBetweenWords",
        "fullTextSearch.disabledTypes",
        "fullTextSearch.enabled",
        "fullTextSearch.ignoreWordsOrder",
        "fullTextSearch.ignoreDiacritics",
        "fullTextSearch.maxDictionarySize"};
    return keys.find(std::string(key)) != keys.end();
}

void ApplyPreference(ApplicationPreferences& p, std::string_view key,
                     std::string value) {
#define STRING(xml, member)          \
    if (key == xml) {                \
        p.member = std::move(value); \
        return;                      \
    }
#define BOOL(xml, member)               \
    if (key == xml) {                   \
        p.member = ParseBoolean(value); \
        return;                         \
    }
#define UINT(xml, member, type)               \
    if (key == xml) {                         \
        p.member = ParseInteger<type>(value); \
        return;                               \
    }
    STRING("interfaceLanguage", interface_language)
    STRING("helpLanguage", help_language)
    STRING("displayStyle", display_style)
    STRING("addonStyle", addon_style)
    STRING("mainWindowHotkey", main_window_hotkey)
    STRING("clipboardHotkey", clipboard_hotkey)
    STRING("audioPlaybackProgram", audio_playback_program)
    STRING("proxyserver.host", proxy_host)
    STRING("fullTextSearch.disabledTypes", full_text_disabled_types)
    BOOL("hideMenubar", hide_menubar)
    BOOL("newTabsOpenAfterCurrentOne", open_new_tabs_after_current)
    BOOL("newTabsOpenInBackground", open_new_tabs_in_background)
    BOOL("hideSingleTab", hide_single_tab)
    BOOL("enableTrayIcon", enable_tray_icon)
    BOOL("startToTray", start_to_tray)
    BOOL("closeToTray", close_to_tray)
    BOOL("autoStart", auto_start)
    BOOL("doubleClickTranslates", double_click_translates)
    BOOL("selectWordBySingleClick", select_word_by_single_click)
    BOOL("escKeyHidesMainWindow", escape_hides_main_window)
    BOOL("alwaysOnTop", always_on_top)
    BOOL("searchInDock", search_in_dock)
    BOOL("enableMainWindowHotkey", enable_main_window_hotkey)
    BOOL("enableClipboardHotkey", enable_clipboard_hotkey)
    BOOL("enableScanPopup", enable_scan_popup)
    BOOL("startWithScanPopupOn", start_with_scan_popup_on)
    BOOL("enableScanPopupModifiers", enable_scan_popup_modifiers)
    BOOL("scanPopupAltMode", scan_popup_alt_mode)
    BOOL("ignoreOwnClipboardChanges", ignore_own_clipboard_changes)
    BOOL("scanPopupUseUIAutomation", scan_popup_use_ui_automation)
    BOOL("scanPopupUseIAccessibleEx", scan_popup_use_accessibility)
    BOOL("scanPopupUseGDMessage", scan_popup_use_gd_message)
    BOOL("scanToMainWindow", scan_to_main_window)
    BOOL("ignoreDiacritics", ignore_diacritics)
    BOOL("showScanFlag", show_scan_flag)
    BOOL("trackClipboardChanges", track_clipboard_changes)
    BOOL("pronounceOnLoadMain", pronounce_on_load_main)
    BOOL("pronounceOnLoadPopup", pronounce_on_load_popup)
    BOOL("useInternalPlayer", use_internal_player)
    BOOL("checkForNewReleases", check_for_new_releases)
    BOOL("disallowContentFromOtherSites", disallow_content_from_other_sites)
    BOOL("enableWebPlugins", enable_web_plugins)
    BOOL("hideGoldenDictHeader", hide_goldendict_header)
    BOOL("clearNetworkCacheOnExit", clear_network_cache_on_exit)
    BOOL("storeHistory", store_history)
    BOOL("confirmFavoritesDeletion", confirm_favorites_deletion)
    BOOL("alwaysExpandOptionalParts", always_expand_optional_parts)
    BOOL("collapseBigArticles", collapse_large_articles)
    BOOL("limitInputPhraseLength", limit_input_phrase_length)
    BOOL("synonymSearchEnabled", synonym_search_enabled)
    BOOL("fullTextSearch.matchCase", full_text_match_case)
    BOOL("fullTextSearch.useMaxDistanceBetweenWords",
         full_text_use_maximum_word_distance)
    BOOL("fullTextSearch.useMaxArticlesPerDictionary",
         full_text_use_maximum_articles)
    BOOL("fullTextSearch.enabled", full_text_search_enabled)
    BOOL("fullTextSearch.ignoreWordsOrder", full_text_ignore_word_order)
    BOOL("fullTextSearch.ignoreDiacritics", full_text_ignore_diacritics)
    UINT("scanPopupModifiers", scan_popup_modifiers, std::uint32_t)
    UINT("scanPopupAltModeSecs", scan_popup_alt_mode_seconds, std::uint32_t)
    UINT("proxyserver.port", proxy_port, std::uint16_t)
    UINT("maxNetworkCacheSize", maximum_network_cache_megabytes, std::uint32_t)
    UINT("wordsZoomLevel", words_zoom_level, std::int32_t)
    UINT("maxStringsInHistory", maximum_history_entries, std::uint32_t)
    UINT("historyStoreInterval", history_store_interval_seconds, std::uint32_t)
    UINT("favoritesStoreInterval", favorites_store_interval_seconds,
         std::uint32_t)
    UINT("articleSizeLimit", article_size_limit, std::uint32_t)
    UINT("inputPhraseLengthLimit", input_phrase_length_limit, std::uint32_t)
    UINT("maxDictionaryRefsInContextMenu", maximum_dictionary_references,
         std::uint16_t)
    UINT("fullTextSearch.maxArticlesPerDictionary",
         full_text_maximum_articles_per_dictionary, std::uint32_t)
    UINT("fullTextSearch.maxDistanceBetweenWords",
         full_text_maximum_word_distance, std::uint32_t)
    UINT("fullTextSearch.maxDictionarySize",
         full_text_maximum_dictionary_megabytes, std::uint32_t)
#undef STRING
#undef BOOL
#undef UINT
    if (key == "zoomFactor")
        p.zoom_factor = ParseDouble(value);
    else if (key == "helpZoomFactor")
        p.help_zoom_factor = ParseDouble(value);
    else if (key == "scanPopupUnpinnedWindowFlags") {
        const auto parsed = ParseInteger<std::uint8_t>(value);
        if (parsed > 2U)
            throw std::runtime_error("invalid scan window mode");
        p.scan_popup_window_mode = static_cast<ScanPopupWindowMode>(parsed);
    } else if (key == "internalPlayerBackend") {
        if (value.empty())
            p.audio_backend = AudioBackend::kAutomatic;
        else if (value == "Qt Multimedia")
            p.audio_backend = AudioBackend::kQtMultimedia;
        else if (value == "FFmpeg+libao")
            p.audio_backend = AudioBackend::kFfmpeg;
        else
            throw std::runtime_error("invalid audio backend");
    } else if (key == "proxyserver.type") {
        const auto parsed = ParseInteger<std::uint8_t>(value);
        if (parsed > 2U)
            throw std::runtime_error("invalid proxy type");
        p.proxy_type = static_cast<ProxyType>(parsed);
    } else if (key == "fullTextSearch.searchMode") {
        const auto parsed = ParseInteger<std::uint8_t>(value);
        if (parsed > 2U)
            throw std::runtime_error("invalid search mode");
        p.full_text_search_mode = static_cast<FullTextSearchMode>(parsed);
    }
    if (p.scan_popup_modifiers & ~kKnownScanPopupModifierMask)
        throw std::runtime_error("invalid scan modifier mask");
}

bool IsGroupElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "groups" && state.elements[2] == "group";
}

enum class GroupValue { kNone, kDictionary, kMuted, kPopupMuted };

GroupValue CurrentGroupValue(const LegacyParserState& state) {
    if (state.elements.size() == 4U && state.elements[0] == "config" &&
        state.elements[1] == "groups" && state.elements[2] == "group" &&
        state.elements[3] == "dictionary") {
        return GroupValue::kDictionary;
    }
    if (state.elements.size() == 5U && state.elements[0] == "config" &&
        state.elements[1] == "groups" && state.elements[2] == "group" &&
        state.elements[3] == "mutedDictionaries") {
        if (state.elements[4] == "mutedDictionary") {
            return GroupValue::kMuted;
        }
        if (state.elements[4] == "popupMutedDictionary") {
            return GroupValue::kPopupMuted;
        }
    }
    return GroupValue::kNone;
}

bool IsCanonicalBase64(std::string_view value) {
    if (value.empty())
        return true;
    if (value.size() % 4U != 0U)
        return false;
    const auto decode = [](char character) {
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
    std::size_t padding = value.back() == '=' ? 1U : 0U;
    if (padding == 1U && value[value.size() - 2U] == '=')
        padding = 2U;
    for (std::size_t index = 0U; index < value.size() - padding; ++index) {
        if (decode(value[index]) < 0)
            return false;
    }
    for (std::size_t index = value.size() - padding; index < value.size();
         ++index) {
        if (value[index] != '=')
            return false;
    }
    if (padding == 1U && (decode(value[value.size() - 2U]) & 3) != 0)
        return false;
    if (padding == 2U && (decode(value[value.size() - 3U]) & 15) != 0)
        return false;
    return true;
}

std::string DecodeMainWindowGeometry(std::string_view value) {
    if (value.empty())
        return {};
    if (value.size() > kMaximumEncodedMainWindowGeometryBytes ||
        value.size() % 4U != 0U || !IsCanonicalBase64(value)) {
        throw std::runtime_error("invalid main-window geometry");
    }
    const auto decode = [](char character) -> unsigned int {
        if (character >= 'A' && character <= 'Z')
            return static_cast<unsigned int>(character - 'A');
        if (character >= 'a' && character <= 'z')
            return static_cast<unsigned int>(character - 'a' + 26);
        if (character >= '0' && character <= '9')
            return static_cast<unsigned int>(character - '0' + 52);
        return character == '+' ? 62U : 63U;
    };
    std::string decoded;
    decoded.reserve((value.size() / 4U) * 3U);
    for (std::size_t index = 0U; index < value.size(); index += 4U) {
        const unsigned int first = decode(value[index]);
        const unsigned int second = decode(value[index + 1U]);
        decoded.push_back(static_cast<char>((first << 2U) | (second >> 4U)));
        if (value[index + 2U] != '=') {
            const unsigned int third = decode(value[index + 2U]);
            decoded.push_back(
                static_cast<char>((second << 4U) | (third >> 2U)));
            if (value[index + 3U] != '=') {
                decoded.push_back(static_cast<char>((third << 6U) |
                                                    decode(value[index + 3U])));
            }
        }
    }
    if (decoded.size() > kMaximumMainWindowGeometryBytes)
        throw std::runtime_error("main-window geometry is too large");
    return decoded;
}

bool IsMainWindowGeometryElement(const LegacyParserState& state) {
    return state.elements.size() == 2U && state.elements[0] == "config" &&
           state.elements[1] == "mainWindowGeometry";
}

bool IsPathElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "paths" && state.elements[2] == "path";
}

bool IsSoundDirectoryElement(const LegacyParserState& state) {
    return state.elements.size() == 3U && state.elements[0] == "config" &&
           state.elements[1] == "sounddirs" && state.elements[2] == "sounddir";
}

void Fail(LegacyParserState* state, std::string message) {
    if (state->failed) {
        return;
    }
    state->failed = true;
    state->error = std::move(message);
    XML_StopParser(state->parser, XML_FALSE);
}

void XMLCALL StartElement(void* user_data, const XML_Char* name,
                          const XML_Char** attributes) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (IsPathElement(*state) || IsSoundDirectoryElement(*state) ||
        IsSourceRecord(*state) || !state->forvo_field.empty() ||
        CurrentGroupValue(*state) != GroupValue::kNone ||
        !state->preference_key.empty() || state->reading_main_window_geometry) {
        Fail(state, "Legacy configuration values cannot contain markup");
        return;
    }
    if (state->elements.size() == kMaximumXmlDepth) {
        Fail(state, "Legacy configuration XML is too deeply nested");
        return;
    }
    state->elements.emplace_back(name);
    if (state->elements.size() == 1U && state->elements.front() != "config") {
        Fail(state, "Legacy configuration has an invalid root element");
        return;
    }
    const auto begin_container = [&](SourceContainer container) {
        if (attributes[0] != nullptr)
            throw std::runtime_error("invalid legacy source container");
        const auto index = static_cast<std::size_t>(container);
        if (state->source_containers[index])
            throw std::runtime_error("duplicate legacy source container");
        state->source_containers[index] = true;
    };
    try {
        if (IsContainer(*state, "forvo")) {
            begin_container(SourceContainer::kForvo);
            state->configuration.forvo_sources = {
                {"forvo", "Forvo", false, "https://apifree.forvo.com", {}}};
            return;
        }
        if (IsContainer(*state, "mediawikis")) {
            begin_container(SourceContainer::kMediaWikis);
            state->configuration.mediawiki_sources.clear();
            return;
        }
        if (IsContainer(*state, "websites")) {
            begin_container(SourceContainer::kWebsites);
            state->configuration.website_sources.clear();
            return;
        }
        if (IsContainer(*state, "dictservers")) {
            begin_container(SourceContainer::kDictServers);
            state->configuration.dict_server_sources.clear();
            return;
        }
        if (IsContainer(*state, "programs")) {
            begin_container(SourceContainer::kPrograms);
            state->configuration.external_program_sources.clear();
            return;
        }
        if (state->elements.size() == 3U && state->elements[0] == "config" &&
            state->elements[1] == "forvo") {
            const std::string_view field = state->elements[2];
            if (field != "enable" && field != "apiKey" &&
                field != "languageCodes") {
                throw std::runtime_error("unsupported Forvo field");
            }
            if (attributes[0] != nullptr ||
                !state->forvo_fields.insert(std::string(field)).second) {
                throw std::runtime_error("invalid or duplicate Forvo field");
            }
            state->forvo_field = field;
            state->value.clear();
            return;
        }
        if (IsDirectElement(*state, "mediawikis", "mediawiki")) {
            if (state->configuration.mediawiki_sources.size() ==
                kMaximumOnlineSources)
                throw std::runtime_error("too many MediaWiki sources");
            const auto values = ReadAttributes(
                attributes, {"id", "name", "url", "enabled", "icon"});
            const auto& id = RequiredAttribute(values, "id");
            AddOnlineId(*state, id);
            state->configuration.mediawiki_sources.push_back(
                {id, RequiredAttribute(values, "name"),
                 ParseBoolean(RequiredAttribute(values, "enabled")),
                 RequiredAttribute(values, "url")});
            return;
        }
        if (IsDirectElement(*state, "websites", "website")) {
            if (state->configuration.website_sources.size() ==
                kMaximumOnlineSources)
                throw std::runtime_error("too many website sources");
            const auto values = ReadAttributes(
                attributes,
                {"id", "name", "url", "enabled", "icon", "inside_iframe"});
            if (ParseBoolean(RequiredAttribute(values, "inside_iframe")))
                throw std::runtime_error("iframe website is unrepresentable");
            if (HasUnsupportedWebsiteMarker(RequiredAttribute(values, "url")))
                throw std::runtime_error(
                    "website encoding marker is unrepresentable");
            const auto& id = RequiredAttribute(values, "id");
            AddOnlineId(*state, id);
            state->configuration.website_sources.push_back(
                {id, RequiredAttribute(values, "name"),
                 ParseBoolean(RequiredAttribute(values, "enabled")),
                 RequiredAttribute(values, "url")});
            return;
        }
        if (IsDirectElement(*state, "dictservers", "server")) {
            if (state->configuration.dict_server_sources.size() ==
                kMaximumOnlineSources)
                throw std::runtime_error("too many DICT sources");
            const auto values =
                ReadAttributes(attributes, {"id", "name", "url", "enabled",
                                            "databases", "strategies", "icon"});
            const auto& id = RequiredAttribute(values, "id");
            AddOnlineId(*state, id);
            auto [host, port] = ParseDictUrl(RequiredAttribute(values, "url"));
            state->configuration.dict_server_sources.push_back(
                {id, RequiredAttribute(values, "name"),
                 ParseBoolean(RequiredAttribute(values, "enabled")),
                 std::move(host), port,
                 ParseSingleDictAtom(RequiredAttribute(values, "databases"),
                                     "*"),
                 ParseSingleDictAtom(RequiredAttribute(values, "strategies"),
                                     "prefix")});
            return;
        }
        if (IsDirectElement(*state, "programs", "program")) {
            if (state->configuration.external_program_sources.size() ==
                kMaximumOnlineSources)
                throw std::runtime_error("too many external programs");
            const auto values = ReadAttributes(
                attributes,
                {"id", "name", "commandLine", "enabled", "type", "icon"});
            const auto type =
                ParseInteger<std::uint8_t>(RequiredAttribute(values, "type"));
            if (type == 0U || type > 3U)
                throw std::runtime_error("unsupported external program type");
            auto arguments = ParseLegacyCommandLine(
                RequiredAttribute(values, "commandLine"));
            if (arguments.empty() || arguments.front().empty() ||
                arguments.size() - 1U > kMaximumExternalProgramArguments) {
                throw std::runtime_error("invalid external program command");
            }
            std::string executable = std::move(arguments.front());
            arguments.erase(arguments.begin());
            if (!std::filesystem::path(executable).is_absolute() ||
                executable.find("%GDWORD%") != std::string::npos ||
                IsShellExecutable(executable)) {
                throw std::runtime_error(
                    "unrepresentable external program command");
            }
            const auto& id = RequiredAttribute(values, "id");
            AddOnlineId(*state, id);
            state->configuration.external_program_sources.push_back(
                {id,
                 RequiredAttribute(values, "name"),
                 ParseBoolean(RequiredAttribute(values, "enabled")),
                 static_cast<ExternalProgramOutputKind>(type - 1U),
                 std::move(executable),
                 std::move(arguments),
                 {}});
            return;
        }
        if (state->elements.size() >= 3U && state->elements[0] == "config" &&
            IsSourceContainerName(state->elements[1])) {
            throw std::runtime_error("unsupported legacy source markup");
        }
    } catch (const std::runtime_error&) {
        Fail(state, "Legacy source configuration is invalid or unsupported");
        return;
    }
    if (IsMainWindowGeometryElement(*state)) {
        if (state->has_main_window_geometry) {
            Fail(state, "Legacy main-window geometry is duplicated");
            return;
        }
        state->has_main_window_geometry = true;
        state->reading_main_window_geometry = true;
        state->value.clear();
        return;
    }
    if (IsPreferenceContainer(*state, name) &&
        std::string_view(name) == "proxyserver") {
        if (!state->preference_keys.insert("proxyserver").second) {
            Fail(state, "Legacy proxy preference is duplicated");
            return;
        }
        bool enabled = false;
        bool system = false;
        bool has_enabled = false;
        bool has_system = false;
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            const std::string_view key(attributes[index]);
            if (key != "enabled" && key != "useSystemProxy")
                continue;
            const std::string duplicate_key = "proxyserver@" + std::string(key);
            if (!state->preference_keys.insert(duplicate_key).second) {
                Fail(state, "Legacy preference is duplicated");
                return;
            }
            try {
                const bool parsed = ParseBoolean(attributes[index + 1U]);
                if (key == "enabled") {
                    enabled = parsed;
                    has_enabled = true;
                } else {
                    system = parsed;
                    has_system = true;
                }
            } catch (const std::runtime_error&) {
                Fail(state, "Legacy proxy preference is invalid");
                return;
            }
        }
        if (!has_enabled || !has_system) {
            Fail(state, "Legacy proxy mode attributes are missing");
            return;
        }
        state->configuration.preferences.proxy_mode =
            !enabled ? ProxyMode::kDisabled
                     : (system ? ProxyMode::kSystem : ProxyMode::kManual);
        return;
    }
    if (IsGroupElement(*state)) {
        if (state->configuration.dictionary_groups.size() ==
            kMaximumDictionaryGroups) {
            Fail(state, "Legacy configuration has too many dictionary groups");
            return;
        }
        state->group = {};
        bool has_id = false;
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            const std::string_view key(attributes[index]);
            const std::string_view attribute(attributes[index + 1U]);
            if (attribute.size() > kMaximumGroupValueBytes &&
                key != "iconData") {
                Fail(state, "Legacy dictionary group metadata is too large");
                return;
            }
            if (key == "id") {
                has_id = true;
                const auto conversion = std::from_chars(
                    attribute.data(), attribute.data() + attribute.size(),
                    state->group.id, 10);
                if (conversion.ec != std::errc() ||
                    conversion.ptr != attribute.data() + attribute.size() ||
                    state->group.id == 0U) {
                    Fail(state, "Legacy dictionary group ID is invalid");
                    return;
                }
            } else if (key == "name") {
                state->group.name = attribute;
            } else if (key == "icon") {
                state->group.icon = attribute;
            } else if (key == "favoritesFolder") {
                state->group.favorites_folder = attribute;
            } else if (key == "shortcut") {
                state->group.shortcut = attribute;
            } else if (key == "iconData") {
                if (attribute.size() > kMaximumEncodedGroupIconBytes ||
                    !IsCanonicalBase64(attribute)) {
                    Fail(state, "Legacy dictionary group icon data is invalid");
                    return;
                }
                state->group.encoded_icon_data = attribute;
            }
        }
        if (!has_id || state->group.name.empty() ||
            !state->group_ids.insert(state->group.id).second) {
            Fail(state, "Legacy dictionary group identity is invalid");
            return;
        }
        return;
    }
    if (CurrentGroupValue(*state) != GroupValue::kNone) {
        state->value.clear();
        return;
    }
    const std::string preference_key = PreferenceKey(*state);
    if (IsRecognizedPreference(preference_key)) {
        if (!state->preference_keys.insert(preference_key).second) {
            Fail(state, "Legacy preference is duplicated");
            return;
        }
        state->preference_key = preference_key;
        state->value.clear();
        return;
    }
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state) &&
        CurrentGroupValue(*state) == GroupValue::kNone) {
        return;
    }
    state->value.clear();
    state->sound_name.clear();
    if (IsSoundDirectoryElement(*state)) {
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            if (std::string_view(attributes[index]) == "name") {
                if (std::string_view(attributes[index + 1U]).size() >
                    kMaximumLegacyValueBytes) {
                    Fail(state, "Legacy sound directory name is too large");
                    return;
                }
                state->sound_name = attributes[index + 1U];
            }
        }
    }
}

void XMLCALL CharacterData(void* user_data, const XML_Char* value, int length) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (IsSourceRecord(*state)) {
        if (length != 0)
            Fail(state, "Legacy source records cannot contain text");
        return;
    }
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state) &&
        CurrentGroupValue(*state) == GroupValue::kNone &&
        state->preference_key.empty() && state->forvo_field.empty() &&
        !state->reading_main_window_geometry) {
        return;
    }
    const std::size_t maximum =
        state->reading_main_window_geometry
            ? kMaximumEncodedMainWindowGeometryBytes
        : !state->preference_key.empty() || !state->forvo_field.empty()
            ? kMaximumPreferenceValueBytes
            : (CurrentGroupValue(*state) == GroupValue::kNone
                   ? kMaximumLegacyValueBytes
                   : kMaximumGroupValueBytes);
    if (length < 0 ||
        state->value.size() + static_cast<std::size_t>(length) > maximum) {
        Fail(state, "Legacy configuration value is too large");
        return;
    }
    state->value.append(value, static_cast<std::size_t>(length));
}

void XMLCALL EndElement(void* user_data, const XML_Char*) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (!state->forvo_field.empty() && state->elements.size() == 3U &&
        state->elements[0] == "config" && state->elements[1] == "forvo" &&
        state->elements[2] == state->forvo_field) {
        try {
            auto& forvo = state->configuration.forvo_sources.front();
            if (state->forvo_field == "enable") {
                forvo.enabled = ParseBoolean(state->value);
            } else if (state->forvo_field == "languageCodes") {
                std::size_t start = 0U;
                while (start <= state->value.size()) {
                    const auto comma = state->value.find(',', start);
                    std::string language = state->value.substr(
                        start, comma == std::string::npos
                                   ? state->value.size() - start
                                   : comma - start);
                    const auto not_space = [](unsigned char character) {
                        return std::isspace(character) == 0;
                    };
                    language.erase(language.begin(),
                                   std::find_if(language.begin(),
                                                language.end(), not_space));
                    language.erase(std::find_if(language.rbegin(),
                                                language.rend(), not_space)
                                       .base(),
                                   language.end());
                    if (language.empty())
                        throw std::runtime_error("empty Forvo language");
                    forvo.language_codes.push_back(std::move(language));
                    if (comma == std::string::npos)
                        break;
                    start = comma + 1U;
                }
            }
        } catch (const std::runtime_error&) {
            Fail(state, "Legacy Forvo configuration is invalid");
            return;
        }
        state->forvo_field.clear();
    } else if (IsContainer(*state, "forvo")) {
        if (state->forvo_fields.count("enable") == 0U ||
            state->forvo_fields.count("languageCodes") == 0U) {
            Fail(state, "Legacy Forvo configuration is incomplete");
            return;
        }
    } else if (state->reading_main_window_geometry &&
               IsMainWindowGeometryElement(*state)) {
        try {
            state->configuration.main_window_geometry =
                DecodeMainWindowGeometry(state->value);
        } catch (const std::runtime_error&) {
            Fail(state, "Legacy main-window geometry is invalid");
            return;
        }
        state->reading_main_window_geometry = false;
    } else if (!state->preference_key.empty() &&
               PreferenceKey(*state) == state->preference_key) {
        try {
            ApplyPreference(state->configuration.preferences,
                            state->preference_key, std::move(state->value));
        } catch (const std::runtime_error&) {
            Fail(state, "Legacy preference value is invalid");
            return;
        }
        state->preference_key.clear();
    } else if (IsPathElement(*state)) {
        if (state->value.empty()) {
            Fail(state, "Legacy dictionary path is empty");
            return;
        }
        if (state->configuration.dictionary_paths.size() ==
            kMaximumImportedPaths) {
            Fail(state, "Legacy configuration has too many dictionary paths");
            return;
        }
        state->configuration.dictionary_paths.push_back(
            std::move(state->value));
    } else if (IsSoundDirectoryElement(*state)) {
        if (state->value.empty()) {
            Fail(state, "Legacy sound directory path is empty");
            return;
        }
        if (state->configuration.sound_directories.size() ==
            kMaximumImportedPaths) {
            Fail(state, "Legacy configuration has too many sound directories");
            return;
        }
        state->configuration.sound_directories.push_back(
            {std::move(state->value), std::move(state->sound_name)});
    } else if (CurrentGroupValue(*state) != GroupValue::kNone) {
        if (state->value.empty()) {
            Fail(state, "Legacy dictionary group reference is empty");
            return;
        }
        auto* destination = &state->group.dictionary_ids;
        if (CurrentGroupValue(*state) == GroupValue::kMuted) {
            destination = &state->group.muted_dictionary_ids;
        } else if (CurrentGroupValue(*state) == GroupValue::kPopupMuted) {
            destination = &state->group.popup_muted_dictionary_ids;
        }
        if (destination->size() == kMaximumDictionariesPerGroup) {
            Fail(state, "Legacy dictionary group has too many references");
            return;
        }
        if (std::find(destination->begin(), destination->end(), state->value) !=
            destination->end()) {
            Fail(state, "Legacy dictionary group has duplicate references");
            return;
        }
        destination->push_back(std::move(state->value));
    } else if (IsGroupElement(*state)) {
        state->configuration.dictionary_groups.push_back(
            std::move(state->group));
    }
    state->elements.pop_back();
}

void XMLCALL RejectEntity(void* user_data, const XML_Char*, int,
                          const XML_Char*, int, const XML_Char*,
                          const XML_Char*, const XML_Char*, const XML_Char*) {
    Fail(static_cast<LegacyParserState*>(user_data),
         "Legacy configuration entities are not supported");
}

std::string ReadLegacyConfiguration(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open legacy configuration file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 ||
        static_cast<std::uintmax_t>(size) > kMaximumLegacyConfigurationBytes) {
        throw std::runtime_error(
            "Cannot read bounded legacy configuration file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error(
            "Cannot read complete legacy configuration file");
    }
    return contents;
}

CoreConfiguration ImportLegacyConfiguration(const std::string& path,
                                            std::string index_directory) {
    const std::string contents = ReadLegacyConfiguration(path);
    std::unique_ptr<std::remove_pointer_t<XML_Parser>, ParserDeleter> parser(
        XML_ParserCreate(nullptr));
    if (!parser) {
        throw std::runtime_error("Cannot create legacy configuration parser");
    }
    LegacyParserState state;
    state.parser = parser.get();
    state.configuration.index_directory = std::move(index_directory);
    XML_SetUserData(parser.get(), &state);
    XML_SetElementHandler(parser.get(), StartElement, EndElement);
    XML_SetCharacterDataHandler(parser.get(), CharacterData);
    XML_SetEntityDeclHandler(parser.get(), RejectEntity);
    const XML_Status status = XML_Parse(parser.get(), contents.data(),
                                        static_cast<int>(contents.size()), 1);
    if (state.failed) {
        throw std::runtime_error(state.error);
    }
    if (status != XML_STATUS_OK) {
        throw std::runtime_error(
            std::string("Malformed legacy configuration XML: ") +
            XML_ErrorString(XML_GetErrorCode(parser.get())));
    }
    if (!state.elements.empty()) {
        throw std::runtime_error("Legacy configuration XML is incomplete");
    }
    ValidateConfiguration(state.configuration);
    return state.configuration;
}

bool Exists(const std::string& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect configuration path: " +
                                 error.message());
    }
    return exists;
}

}  // namespace

CoreConfiguration LoadOrMigrateConfiguration(
    const std::string& configuration_path,
    const std::string& legacy_configuration_path,
    const std::string& index_directory) {
    if (Exists(configuration_path)) {
        return LoadConfiguration(configuration_path);
    }
    if (!Exists(legacy_configuration_path)) {
        CoreConfiguration configuration;
        configuration.index_directory = index_directory;
        return configuration;
    }
    CoreConfiguration configuration =
        ImportLegacyConfiguration(legacy_configuration_path, index_directory);
    SaveConfiguration(configuration_path, configuration);
    return configuration;
}

}  // namespace goldendict::core
