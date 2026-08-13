// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GOLDENDICT_CORE_APPLICATION_H_
#define GOLDENDICT_CORE_APPLICATION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "goldendict/base/goldendict_def.tp.h"
#include "goldendict/core/desktop_facade.h"
#include "goldendict/core/dictionary_service.h"

namespace goldendict::core {

inline constexpr std::size_t kMaximumDictionaryPaths = 256U;
inline constexpr std::size_t kMaximumSoundDirectories = 256U;
inline constexpr std::size_t kMaximumOnlineSources = 256U;
inline constexpr std::size_t kMaximumExternalProgramArguments = 256U;

struct SoundDirectoryConfiguration {
    std::string path;
    std::string name;

    bool operator==(const SoundDirectoryConfiguration& other) const noexcept {
        return path == other.path && name == other.name;
    }

    bool operator!=(const SoundDirectoryConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct MediaWikiSourceConfiguration {
    std::string id;
    std::string name;
    bool enabled = false;
    std::string base_url;

    bool operator==(const MediaWikiSourceConfiguration& other) const noexcept {
        return std::tie(id, name, enabled, base_url) ==
               std::tie(other.id, other.name, other.enabled, other.base_url);
    }

    bool operator!=(const MediaWikiSourceConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct WebsiteSourceConfiguration {
    std::string id;
    std::string name;
    bool enabled = false;
    std::string url_template;

    bool operator==(const WebsiteSourceConfiguration& other) const noexcept {
        return std::tie(id, name, enabled, url_template) ==
               std::tie(other.id, other.name, other.enabled,
                        other.url_template);
    }

    bool operator!=(const WebsiteSourceConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct ForvoSourceConfiguration {
    std::string id;
    std::string name;
    bool enabled = false;
    std::string api_base_url;
    std::vector<std::string> language_codes;

    bool operator==(const ForvoSourceConfiguration& other) const noexcept {
        return std::tie(id, name, enabled, api_base_url, language_codes) ==
               std::tie(other.id, other.name, other.enabled, other.api_base_url,
                        other.language_codes);
    }

    bool operator!=(const ForvoSourceConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct DictServerSourceConfiguration {
    std::string id;
    std::string name;
    bool enabled = false;
    std::string host;
    std::uint16_t port = 2628U;
    std::string database = "*";
    std::string strategy = "prefix";

    bool operator==(const DictServerSourceConfiguration& other) const noexcept {
        return std::tie(id, name, enabled, host, port, database, strategy) ==
               std::tie(other.id, other.name, other.enabled, other.host,
                        other.port, other.database, other.strategy);
    }

    bool operator!=(const DictServerSourceConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

enum class ExternalProgramOutputKind : std::uint8_t {
    kPlainText,
    kHtml,
    kPrefixMatch
};

struct ExternalProgramSourceConfiguration {
    std::string id;
    std::string name;
    bool enabled = false;
    ExternalProgramOutputKind output_kind =
        ExternalProgramOutputKind::kPlainText;
    std::string executable;
    std::vector<std::string> argument_templates;
    std::string working_directory;

    bool operator==(
        const ExternalProgramSourceConfiguration& other) const noexcept {
        return std::tie(id, name, enabled, output_kind, executable,
                        argument_templates, working_directory) ==
               std::tie(other.id, other.name, other.enabled, other.output_kind,
                        other.executable, other.argument_templates,
                        other.working_directory);
    }

    bool operator!=(
        const ExternalProgramSourceConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

struct DictionaryGroupConfiguration {
    DictionaryGroupConfiguration() = default;

    DictionaryGroupConfiguration(
        std::uint32_t group_id, std::string group_name, std::string group_icon,
        std::vector<std::string> group_dictionary_ids,
        std::vector<std::string> group_muted_dictionary_ids = {},
        std::vector<std::string> group_popup_muted_dictionary_ids = {},
        std::string group_favorites_folder = {},
        std::string group_shortcut = {},
        std::string group_encoded_icon_data = {})
        : id(group_id),
          name(std::move(group_name)),
          icon(std::move(group_icon)),
          dictionary_ids(std::move(group_dictionary_ids)),
          muted_dictionary_ids(std::move(group_muted_dictionary_ids)),
          popup_muted_dictionary_ids(
              std::move(group_popup_muted_dictionary_ids)),
          favorites_folder(std::move(group_favorites_folder)),
          shortcut(std::move(group_shortcut)),
          encoded_icon_data(std::move(group_encoded_icon_data)) {}

    std::uint32_t id = 0U;
    std::string name;
    std::string icon;
    std::vector<std::string> dictionary_ids;
    std::vector<std::string> muted_dictionary_ids;
    std::vector<std::string> popup_muted_dictionary_ids;
    std::string favorites_folder;
    std::string shortcut;
    std::string encoded_icon_data;

    bool operator==(const DictionaryGroupConfiguration& other) const noexcept {
        return id == other.id && name == other.name && icon == other.icon &&
               dictionary_ids == other.dictionary_ids &&
               muted_dictionary_ids == other.muted_dictionary_ids &&
               popup_muted_dictionary_ids == other.popup_muted_dictionary_ids &&
               favorites_folder == other.favorites_folder &&
               shortcut == other.shortcut &&
               encoded_icon_data == other.encoded_icon_data;
    }

    bool operator!=(const DictionaryGroupConfiguration& other) const noexcept {
        return !(*this == other);
    }
};

enum class ProxyMode : std::uint8_t { kDisabled, kSystem, kManual };
enum class ProxyType : std::uint8_t { kSocks5, kHttpConnect, kHttpGet };
enum class AudioBackend : std::uint8_t { kAutomatic, kQtMultimedia, kFfmpeg };
enum class ScanPopupWindowMode : std::uint8_t { kDefault, kPopup, kTool };
enum class FullTextSearchMode : std::uint8_t {
    kWholeWords,
    kWildcard,
    kRegularExpression
};

struct ApplicationPreferences {
    std::string interface_language;
    std::string help_language;
    std::string display_style;
    std::string addon_style;
    bool open_new_tabs_after_current = false;
    bool open_new_tabs_in_background = true;
    bool hide_menubar = false;
    bool enable_tray_icon = true;
    bool start_to_tray = false;
    bool close_to_tray = true;
    bool auto_start = false;
    bool double_click_translates = true;
    bool select_word_by_single_click = false;
    bool escape_hides_main_window = false;
    bool always_on_top = false;
    bool search_in_dock = false;
    bool enable_main_window_hotkey = true;
    std::string main_window_hotkey = "Ctrl+F11,F11";
    bool enable_clipboard_hotkey = true;
    std::string clipboard_hotkey = "Ctrl+C,C";
    bool enable_scan_popup = true;
    bool start_with_scan_popup_on = false;
    bool enable_scan_popup_modifiers = false;
    std::uint32_t scan_popup_modifiers = 0U;
    bool scan_popup_alt_mode = false;
    std::uint32_t scan_popup_alt_mode_seconds = 3U;
    bool ignore_own_clipboard_changes = false;
    bool scan_popup_use_ui_automation = true;
    bool scan_popup_use_accessibility = true;
    bool scan_popup_use_gd_message = true;
    bool scan_to_main_window = false;
    bool ignore_diacritics = false;
    bool show_scan_flag = false;
    bool track_clipboard_changes = false;
    ScanPopupWindowMode scan_popup_window_mode = ScanPopupWindowMode::kDefault;
    bool pronounce_on_load_main = false;
    bool pronounce_on_load_popup = false;
    bool use_internal_player = true;
    AudioBackend audio_backend = AudioBackend::kAutomatic;
    std::string audio_playback_program;
    ProxyMode proxy_mode = ProxyMode::kDisabled;
    ProxyType proxy_type = ProxyType::kSocks5;
    std::string proxy_host;
    std::uint16_t proxy_port = 0U;
    bool check_for_new_releases = true;
    bool disallow_content_from_other_sites = false;
    bool enable_web_plugins = false;
    bool hide_goldendict_header = false;
    std::uint32_t maximum_network_cache_megabytes = 50U;
    bool clear_network_cache_on_exit = true;
    double zoom_factor = 1.0;
    double help_zoom_factor = 1.0;
    std::int32_t words_zoom_level = 0;
    std::uint32_t maximum_history_entries = 500U;
    bool store_history = true;
    std::uint32_t history_store_interval_seconds = 0U;
    std::uint32_t favorites_store_interval_seconds = 0U;
    bool confirm_favorites_deletion = true;
    bool always_expand_optional_parts = false;
    bool collapse_large_articles = false;
    std::uint32_t article_size_limit = 2000U;
    bool limit_input_phrase_length = false;
    std::uint32_t input_phrase_length_limit = 1000U;
    std::uint16_t maximum_dictionary_references = 20U;
    bool synonym_search_enabled = true;
    bool full_text_search_enabled = true;
    FullTextSearchMode full_text_search_mode = FullTextSearchMode::kWholeWords;
    bool full_text_match_case = false;
    std::uint32_t full_text_maximum_articles_per_dictionary = 100U;
    std::uint32_t full_text_maximum_word_distance = 2U;
    bool full_text_use_maximum_word_distance = true;
    bool full_text_use_maximum_articles = false;
    bool full_text_ignore_word_order = false;
    bool full_text_ignore_diacritics = false;
    std::uint32_t full_text_maximum_dictionary_megabytes = 0U;
    std::string full_text_disabled_types;

    bool operator==(const ApplicationPreferences& other) const noexcept {
        return std::tie(
                   interface_language, help_language, display_style,
                   addon_style, open_new_tabs_after_current,
                   open_new_tabs_in_background, hide_menubar, enable_tray_icon,
                   start_to_tray, close_to_tray, auto_start,
                   double_click_translates, select_word_by_single_click,
                   escape_hides_main_window, always_on_top, search_in_dock,
                   enable_main_window_hotkey, main_window_hotkey,
                   enable_clipboard_hotkey, clipboard_hotkey, enable_scan_popup,
                   start_with_scan_popup_on, enable_scan_popup_modifiers,
                   scan_popup_modifiers, scan_popup_alt_mode,
                   scan_popup_alt_mode_seconds, ignore_own_clipboard_changes,
                   scan_popup_use_ui_automation, scan_popup_use_accessibility,
                   scan_popup_use_gd_message, scan_to_main_window,
                   ignore_diacritics, show_scan_flag, track_clipboard_changes,
                   scan_popup_window_mode, pronounce_on_load_main,
                   pronounce_on_load_popup, use_internal_player, audio_backend,
                   audio_playback_program, proxy_mode, proxy_type, proxy_host,
                   proxy_port, check_for_new_releases,
                   disallow_content_from_other_sites, enable_web_plugins,
                   hide_goldendict_header, maximum_network_cache_megabytes,
                   clear_network_cache_on_exit, zoom_factor, help_zoom_factor,
                   words_zoom_level, maximum_history_entries, store_history,
                   history_store_interval_seconds,
                   favorites_store_interval_seconds, confirm_favorites_deletion,
                   always_expand_optional_parts, collapse_large_articles,
                   article_size_limit, limit_input_phrase_length,
                   input_phrase_length_limit, maximum_dictionary_references,
                   synonym_search_enabled, full_text_search_enabled,
                   full_text_search_mode, full_text_match_case,
                   full_text_maximum_articles_per_dictionary,
                   full_text_maximum_word_distance,
                   full_text_use_maximum_word_distance,
                   full_text_use_maximum_articles, full_text_ignore_word_order,
                   full_text_ignore_diacritics,
                   full_text_maximum_dictionary_megabytes,
                   full_text_disabled_types) ==
               std::tie(
                   other.interface_language, other.help_language,
                   other.display_style, other.addon_style,
                   other.open_new_tabs_after_current,
                   other.open_new_tabs_in_background, other.hide_menubar,
                   other.enable_tray_icon, other.start_to_tray,
                   other.close_to_tray, other.auto_start,
                   other.double_click_translates,
                   other.select_word_by_single_click,
                   other.escape_hides_main_window, other.always_on_top,
                   other.search_in_dock, other.enable_main_window_hotkey,
                   other.main_window_hotkey, other.enable_clipboard_hotkey,
                   other.clipboard_hotkey, other.enable_scan_popup,
                   other.start_with_scan_popup_on,
                   other.enable_scan_popup_modifiers,
                   other.scan_popup_modifiers, other.scan_popup_alt_mode,
                   other.scan_popup_alt_mode_seconds,
                   other.ignore_own_clipboard_changes,
                   other.scan_popup_use_ui_automation,
                   other.scan_popup_use_accessibility,
                   other.scan_popup_use_gd_message, other.scan_to_main_window,
                   other.ignore_diacritics, other.show_scan_flag,
                   other.track_clipboard_changes, other.scan_popup_window_mode,
                   other.pronounce_on_load_main, other.pronounce_on_load_popup,
                   other.use_internal_player, other.audio_backend,
                   other.audio_playback_program, other.proxy_mode,
                   other.proxy_type, other.proxy_host, other.proxy_port,
                   other.check_for_new_releases,
                   other.disallow_content_from_other_sites,
                   other.enable_web_plugins, other.hide_goldendict_header,
                   other.maximum_network_cache_megabytes,
                   other.clear_network_cache_on_exit, other.zoom_factor,
                   other.help_zoom_factor, other.words_zoom_level,
                   other.maximum_history_entries, other.store_history,
                   other.history_store_interval_seconds,
                   other.favorites_store_interval_seconds,
                   other.confirm_favorites_deletion,
                   other.always_expand_optional_parts,
                   other.collapse_large_articles, other.article_size_limit,
                   other.limit_input_phrase_length,
                   other.input_phrase_length_limit,
                   other.maximum_dictionary_references,
                   other.synonym_search_enabled, other.full_text_search_enabled,
                   other.full_text_search_mode, other.full_text_match_case,
                   other.full_text_maximum_articles_per_dictionary,
                   other.full_text_maximum_word_distance,
                   other.full_text_use_maximum_word_distance,
                   other.full_text_use_maximum_articles,
                   other.full_text_ignore_word_order,
                   other.full_text_ignore_diacritics,
                   other.full_text_maximum_dictionary_megabytes,
                   other.full_text_disabled_types);
    }

    bool operator!=(const ApplicationPreferences& other) const noexcept {
        return !(*this == other);
    }
};

struct CoreConfiguration {
    std::vector<std::string> dictionary_paths;
    std::string index_directory;
    std::vector<SoundDirectoryConfiguration> sound_directories;
    std::vector<MediaWikiSourceConfiguration> mediawiki_sources;
    std::vector<WebsiteSourceConfiguration> website_sources;
    std::vector<ForvoSourceConfiguration> forvo_sources = {
        {"forvo", "Forvo", false, "https://apifree.forvo.com", {"en", "ru"}}};
    std::vector<DictServerSourceConfiguration> dict_server_sources;
    std::vector<ExternalProgramSourceConfiguration> external_program_sources;
    std::vector<DictionaryGroupConfiguration> dictionary_groups;
    ApplicationPreferences preferences;
    std::optional<ArticleTabSession> article_tab_session;
    std::string main_window_geometry;
};

// Missing files load as an empty clean-profile configuration. Malformed files
// and I/O failures throw std::runtime_error.
GOLDENDICT_EXPORTS CoreConfiguration
LoadConfiguration(const std::string& configuration_path);
// Validates a complete candidate without performing discovery or I/O.
// Invalid candidates throw std::runtime_error with the same policy used by
// LoadConfiguration and SaveConfiguration.
GOLDENDICT_EXPORTS void ValidateConfiguration(
    const CoreConfiguration& configuration);
GOLDENDICT_EXPORTS void SaveConfiguration(
    const std::string& configuration_path,
    const CoreConfiguration& configuration);

// Loads the current configuration when present. Otherwise, imports supported
// local sources, groups, preferences, geometry, online sources, and external
// programs from a legacy GoldenDict XML configuration, saves the complete new
// configuration atomically, and leaves the legacy file untouched.
// Missing current and legacy files return an empty configuration with the
// supplied index directory. Malformed files and I/O failures throw.
GOLDENDICT_EXPORTS CoreConfiguration
LoadOrMigrateConfiguration(const std::string& configuration_path,
                           const std::string& legacy_configuration_path,
                           const std::string& index_directory);

GOLDENDICT_EXPORTS std::unique_ptr<DictionaryService> CreateDictionaryService(
    const CoreConfiguration& configuration);
GOLDENDICT_EXPORTS std::unique_ptr<DesktopFacade> CreateDesktopFacade(
    const CoreConfiguration& configuration);

}  // namespace goldendict::core

#endif  // GOLDENDICT_CORE_APPLICATION_H_
