// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/application.h"

#include <expat.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
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
constexpr std::uint32_t kKnownScanPopupModifierMask = 0x03ffU;

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
    bool failed = false;
    std::string error;
};

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
    if (value.size() > kMaximumEncodedGroupIconBytes || value.size() % 4U != 0U)
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
        CurrentGroupValue(*state) != GroupValue::kNone ||
        !state->preference_key.empty()) {
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
                if (!IsCanonicalBase64(attribute)) {
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
    if (!IsPathElement(*state) && !IsSoundDirectoryElement(*state) &&
        CurrentGroupValue(*state) == GroupValue::kNone &&
        state->preference_key.empty()) {
        return;
    }
    const std::size_t maximum =
        !state->preference_key.empty()
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
    if (!state->preference_key.empty() &&
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
