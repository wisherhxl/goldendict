// SPDX-License-Identifier: GPL-3.0-or-later

#include "preferences_dialog.h"

#include <utility>

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(
    const goldendict::core::ApplicationPreferences& preferences,
    ApplyCallback apply_callback, const QString& network_cache_directory,
    QWidget* parent)
    : QDialog(parent),
      preferences_(preferences),
      apply_callback_(std::move(apply_callback)) {
    setObjectName(QStringLiteral("preferencesDialog"));
    setWindowTitle(QStringLiteral("Preferences"));
    setModal(true);

    auto* layout = new QVBoxLayout(this);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("preferencesTabs"));
    auto* general_page = new QWidget(tabs);
    general_page->setObjectName(QStringLiteral("preferencesGeneralPage"));
    auto* general_layout = new QVBoxLayout(general_page);
    auto* tab_group = new QGroupBox(QStringLiteral("Tabs"), general_page);
    tab_group->setObjectName(QStringLiteral("preferencesTabGroup"));
    auto* tab_layout = new QVBoxLayout(tab_group);
    open_in_background_ = new QCheckBox(
        QStringLiteral("Open new tabs in the background"), tab_group);
    open_in_background_->setObjectName(
        QStringLiteral("newTabsOpenInBackground"));
    open_in_background_->setChecked(preferences.open_new_tabs_in_background);
    open_after_current_ = new QCheckBox(
        QStringLiteral("Open new tabs after the current one"), tab_group);
    open_after_current_->setObjectName(
        QStringLiteral("newTabsOpenAfterCurrentOne"));
    open_after_current_->setChecked(preferences.open_new_tabs_after_current);
    hide_single_tab_ =
        new QCheckBox(QStringLiteral("Hide single tab"), tab_group);
    hide_single_tab_->setObjectName(QStringLiteral("hideSingleTab"));
    hide_single_tab_->setToolTip(QStringLiteral(
        "Select this option if you don't want to see the main tab bar when "
        "only a single tab is opened."));
    hide_single_tab_->setChecked(preferences.hide_single_tab);
    mru_tab_order_ = new QCheckBox(
        QStringLiteral("Ctrl-Tab navigates tabs in MRU order"), tab_group);
    mru_tab_order_->setObjectName(QStringLiteral("mruTabOrder"));
    mru_tab_order_->setChecked(preferences.mru_tab_order);
    tab_layout->addWidget(open_in_background_);
    tab_layout->addWidget(open_after_current_);
    tab_layout->addWidget(hide_single_tab_);
    tab_layout->addWidget(mru_tab_order_);
    general_layout->addWidget(tab_group);

    escape_hides_main_window_ = new QCheckBox(
        QStringLiteral("ESC key hides main window"), general_page);
    escape_hides_main_window_->setObjectName(
        QStringLiteral("escKeyHidesMainWindow"));
    escape_hides_main_window_->setToolTip(QStringLiteral(
        "Normally, pressing ESC key moves focus to the translation line.\n"
        "With this on however, it will hide the main window."));
    escape_hides_main_window_->setChecked(preferences.escape_hides_main_window);
    general_layout->addWidget(escape_hides_main_window_);

    double_click_translates_ = new QCheckBox(
        QStringLiteral("Double-click translates the word clicked"),
        general_page);
    double_click_translates_->setObjectName(
        QStringLiteral("doubleClickTranslates"));
    double_click_translates_->setChecked(preferences.double_click_translates);
    general_layout->addWidget(double_click_translates_);

    select_word_by_single_click_ = new QCheckBox(
        QStringLiteral("Select word by single click"), general_page);
    select_word_by_single_click_->setObjectName(
        QStringLiteral("selectBySingleClick"));
    select_word_by_single_click_->setToolTip(QStringLiteral(
        "Turn this option on if you want to select words by single mouse "
        "click"));
    select_word_by_single_click_->setChecked(
        preferences.select_word_by_single_click);
    general_layout->addWidget(select_word_by_single_click_);

    auto* history_group =
        new QGroupBox(QStringLiteral("History"), general_page);
    history_group->setObjectName(QStringLiteral("preferencesHistoryGroup"));
    auto* history_layout = new QVBoxLayout(history_group);
    store_history_ =
        new QCheckBox(QStringLiteral("Store &history"), history_group);
    store_history_->setObjectName(QStringLiteral("storeHistory"));
    store_history_->setToolTip(QStringLiteral(
        "Turn this option on to store history of the translated words"));
    store_history_->setChecked(preferences.store_history);
    history_layout->addWidget(store_history_);
    auto* maximum_layout = new QHBoxLayout;
    auto* maximum_label =
        new QLabel(QStringLiteral("Maximum history size:"), history_group);
    maximum_label->setObjectName(QStringLiteral("historySizeLabel"));
    maximum_label->setToolTip(QStringLiteral(
        "Specify the maximum number of entries to keep in history."));
    maximum_history_entries_ = new QSpinBox(history_group);
    maximum_history_entries_->setObjectName(
        QStringLiteral("historyMaxSizeField"));
    maximum_history_entries_->setAccelerated(true);
    maximum_history_entries_->setRange(0, 99999);
    maximum_history_entries_->setValue(
        static_cast<int>(preferences.maximum_history_entries));
    maximum_label->setBuddy(maximum_history_entries_);
    maximum_layout->addWidget(maximum_label);
    maximum_layout->addWidget(maximum_history_entries_);
    maximum_layout->addStretch();
    history_layout->addLayout(maximum_layout);
    general_layout->addWidget(history_group);

    auto* favorites_group =
        new QGroupBox(QStringLiteral("Favorites"), general_page);
    favorites_group->setObjectName(QStringLiteral("favoritesBox"));
    auto* favorites_layout = new QVBoxLayout(favorites_group);
    confirm_favorites_deletion_ = new QCheckBox(
        QStringLiteral("Confirmation for items deletion"), favorites_group);
    confirm_favorites_deletion_->setObjectName(
        QStringLiteral("confirmFavoritesDeletion"));
    confirm_favorites_deletion_->setToolTip(QStringLiteral(
        "Turn this option on to confirm every operation of items deletion"));
    confirm_favorites_deletion_->setChecked(
        preferences.confirm_favorites_deletion);
    favorites_layout->addWidget(confirm_favorites_deletion_);
    general_layout->addWidget(favorites_group);

    auto* articles_group =
        new QGroupBox(QStringLiteral("Articles"), general_page);
    articles_group->setObjectName(QStringLiteral("preferencesArticlesGroup"));
    auto* articles_layout = new QHBoxLayout(articles_group);
    collapse_large_articles_ = new QCheckBox(
        QStringLiteral("Collapse articles more than"), articles_group);
    collapse_large_articles_->setObjectName(
        QStringLiteral("collapseBigArticles"));
    collapse_large_articles_->setToolTip(QStringLiteral(
        "Select this option to automatic collapse big articles"));
    collapse_large_articles_->setChecked(preferences.collapse_large_articles);
    article_size_limit_ = new QSpinBox(articles_group);
    article_size_limit_->setObjectName(QStringLiteral("articleSizeLimit"));
    article_size_limit_->setToolTip(
        QStringLiteral("Articles longer than this size will be collapsed"));
    article_size_limit_->setRange(1, 100000);
    article_size_limit_->setSingleStep(50);
    article_size_limit_->setValue(
        static_cast<int>(preferences.article_size_limit));
    article_size_limit_->setEnabled(preferences.collapse_large_articles);
    connect(collapse_large_articles_, &QCheckBox::toggled, article_size_limit_,
            &QSpinBox::setEnabled);
    auto* symbols = new QLabel(QStringLiteral("symbols"), articles_group);
    symbols->setObjectName(QStringLiteral("articleSizeLimitLabel"));
    articles_layout->addWidget(collapse_large_articles_);
    articles_layout->addWidget(article_size_limit_);
    articles_layout->addWidget(symbols);
    articles_layout->addStretch();
    general_layout->addWidget(articles_group);

    always_expand_optional_parts_ =
        new QCheckBox(QStringLiteral("Expand optional &parts"), general_page);
    always_expand_optional_parts_->setObjectName(
        QStringLiteral("alwaysExpandOptionalParts"));
    always_expand_optional_parts_->setToolTip(QStringLiteral(
        "Turn this option on to always expand optional parts of articles"));
    always_expand_optional_parts_->setChecked(
        preferences.always_expand_optional_parts);
    general_layout->addWidget(always_expand_optional_parts_);

    auto* input_phrase_group =
        new QGroupBox(QStringLiteral("Input phrase length"), general_page);
    input_phrase_group->setObjectName(
        QStringLiteral("preferencesInputPhraseLengthGroup"));
    auto* input_phrase_layout = new QHBoxLayout(input_phrase_group);
    limit_input_phrase_length_ = new QCheckBox(
        QStringLiteral("Ignore input phrases longer than"), input_phrase_group);
    limit_input_phrase_length_->setObjectName(
        QStringLiteral("limitInputPhraseLength"));
    limit_input_phrase_length_->setToolTip(QStringLiteral(
        "Turn this option on to ignore unreasonably long input text"));
    limit_input_phrase_length_->setChecked(
        preferences.limit_input_phrase_length);
    input_phrase_length_limit_ = new QSpinBox(input_phrase_group);
    input_phrase_length_limit_->setObjectName(
        QStringLiteral("inputPhraseLengthLimit"));
    input_phrase_length_limit_->setToolTip(
        QStringLiteral("Input phrases longer than this size will be ignored"));
    input_phrase_length_limit_->setRange(1, 1000000);
    input_phrase_length_limit_->setSingleStep(10);
    input_phrase_length_limit_->setValue(
        static_cast<int>(preferences.input_phrase_length_limit));
    input_phrase_length_limit_->setEnabled(
        preferences.limit_input_phrase_length);
    connect(limit_input_phrase_length_, &QCheckBox::toggled,
            input_phrase_length_limit_, &QSpinBox::setEnabled);
    auto* input_phrase_symbols =
        new QLabel(QStringLiteral("symbols"), input_phrase_group);
    input_phrase_symbols->setObjectName(
        QStringLiteral("inputPhraseLengthLimitLabel"));
    input_phrase_layout->addWidget(limit_input_phrase_length_);
    input_phrase_layout->addWidget(input_phrase_length_limit_);
    input_phrase_layout->addWidget(input_phrase_symbols);
    input_phrase_layout->addStretch();
    general_layout->addWidget(input_phrase_group);
    ignore_diacritics_ =
        new QCheckBox(QStringLiteral("Ignore diacritics"), general_page);
    ignore_diacritics_->setObjectName(QStringLiteral("ignoreDiacritics"));
    ignore_diacritics_->setToolTip(QStringLiteral(
        "Turn this option on to ignore diacritics while searching articles"));
    ignore_diacritics_->setChecked(preferences.ignore_diacritics);
    general_layout->addWidget(ignore_diacritics_);
    synonym_search_enabled_ = new QCheckBox(
        QStringLiteral("Extra search via synonyms"), general_page);
    synonym_search_enabled_->setObjectName(
        QStringLiteral("synonymSearchEnabled"));
    synonym_search_enabled_->setToolTip(QStringLiteral(
        "Turn this option on to enable extra articles search via synonym "
        "lists from Stardict, Babylon and GLS dictionaries"));
    synonym_search_enabled_->setChecked(preferences.synonym_search_enabled);
    general_layout->addWidget(synonym_search_enabled_);

    auto* dictionary_context_layout = new QHBoxLayout;
    auto* dictionary_context_label = new QLabel(
        QStringLiteral("Context menu dictionaries limit:"), general_page);
    dictionary_context_label->setObjectName(
        QStringLiteral("maxDictsInContextMenuLabel"));
    dictionary_context_label->setToolTip(
        QStringLiteral("Adjust this value to avoid huge context menus."));
    maximum_dictionary_references_ = new QSpinBox(general_page);
    maximum_dictionary_references_->setObjectName(
        QStringLiteral("maxDictsInContextMenu"));
    maximum_dictionary_references_->setRange(0, 9999);
    maximum_dictionary_references_->setSingleStep(1);
    maximum_dictionary_references_->setValue(
        static_cast<int>(preferences.maximum_dictionary_references));
    dictionary_context_label->setBuddy(maximum_dictionary_references_);
    dictionary_context_layout->addWidget(dictionary_context_label);
    dictionary_context_layout->addWidget(maximum_dictionary_references_);
    dictionary_context_layout->addStretch();
    general_layout->addLayout(dictionary_context_layout);
    general_layout->addStretch();
    tabs->addTab(general_page, QStringLiteral("General"));

    auto* network_page = new QWidget(tabs);
    network_page->setObjectName(QStringLiteral("preferencesNetworkPage"));
    auto* network_layout = new QVBoxLayout(network_page);
    use_proxy_server_ =
        new QGroupBox(QStringLiteral("Use proxy server"), network_page);
    use_proxy_server_->setObjectName(QStringLiteral("useProxyServer"));
    use_proxy_server_->setCheckable(true);
    use_proxy_server_->setChecked(
        preferences.proxy_mode == goldendict::core::ProxyMode::kManual &&
        preferences.proxy_type == goldendict::core::ProxyType::kHttpConnect);
    use_proxy_server_->setToolTip(
        QStringLiteral("Enable if you wish to use a proxy server\n"
                       "for all program's network requests."));
    auto* proxy_layout = new QVBoxLayout(use_proxy_server_);
    auto* custom_proxy =
        new QCheckBox(QStringLiteral("Custom proxy"), use_proxy_server_);
    custom_proxy->setObjectName(QStringLiteral("customProxy"));
    custom_proxy->setChecked(true);
    custom_proxy->setEnabled(false);
    proxy_layout->addWidget(custom_proxy);
    auto* custom_settings =
        new QGroupBox(QStringLiteral("Custom settings"), use_proxy_server_);
    custom_settings->setObjectName(QStringLiteral("customSettingsGroup"));
    auto* settings_layout = new QHBoxLayout(custom_settings);
    auto* type_label = new QLabel(QStringLiteral("Type:"), custom_settings);
    proxy_type_ = new QComboBox(custom_settings);
    proxy_type_->setObjectName(QStringLiteral("proxyType"));
    proxy_type_->addItem(QStringLiteral("HTTP Transp."));
    proxy_type_->setEnabled(false);
    auto* host_label = new QLabel(QStringLiteral("Host:"), custom_settings);
    proxy_host_ = new QLineEdit(custom_settings);
    proxy_host_->setObjectName(QStringLiteral("proxyHost"));
    proxy_host_->setText(QString::fromStdString(preferences.proxy_host));
    auto* port_label = new QLabel(QStringLiteral("Port:"), custom_settings);
    proxy_port_ = new QSpinBox(custom_settings);
    proxy_port_->setObjectName(QStringLiteral("proxyPort"));
    proxy_port_->setRange(0, 65535);
    proxy_port_->setValue(preferences.proxy_port);
    type_label->setBuddy(proxy_type_);
    host_label->setBuddy(proxy_host_);
    port_label->setBuddy(proxy_port_);
    settings_layout->addWidget(type_label);
    settings_layout->addWidget(proxy_type_);
    settings_layout->addWidget(host_label);
    settings_layout->addWidget(proxy_host_);
    settings_layout->addWidget(port_label);
    settings_layout->addWidget(proxy_port_);
    proxy_layout->addWidget(custom_settings);
    auto* cache_layout = new QHBoxLayout;
    auto* cache_label =
        new QLabel(QStringLiteral("Maximum network cache size:"), network_page);
    cache_label->setObjectName(QStringLiteral("networkCacheSizeLabel"));
    maximum_network_cache_megabytes_ = new QSpinBox(network_page);
    maximum_network_cache_megabytes_->setObjectName(
        QStringLiteral("maxNetworkCacheSize"));
    maximum_network_cache_megabytes_->setRange(0, 2000);
#ifdef Q_OS_WIN
    maximum_network_cache_megabytes_->setSuffix(QStringLiteral(" MB"));
#else
    maximum_network_cache_megabytes_->setSuffix(QStringLiteral(" MiB"));
#endif
    maximum_network_cache_megabytes_->setToolTip(
        QStringLiteral(
            "Maximum disk space occupied by GoldenDict's network cache in\n%1\n"
            "If set to 0 the network disk cache will be disabled.")
            .arg(network_cache_directory));
    maximum_network_cache_megabytes_->setValue(
        static_cast<int>(preferences.maximum_network_cache_megabytes));
    cache_label->setBuddy(maximum_network_cache_megabytes_);
    clear_network_cache_on_exit_ = new QCheckBox(
        QStringLiteral("Clear network cache on exit"), network_page);
    clear_network_cache_on_exit_->setObjectName(
        QStringLiteral("clearNetworkCacheOnExit"));
    clear_network_cache_on_exit_->setToolTip(
        QStringLiteral("When this option is enabled, GoldenDict\n"
                       "clears its network cache from disk during exit."));
    clear_network_cache_on_exit_->setChecked(
        preferences.clear_network_cache_on_exit);
    clear_network_cache_on_exit_->setEnabled(
        preferences.maximum_network_cache_megabytes != 0U);
    connect(maximum_network_cache_megabytes_, &QSpinBox::valueChanged,
            clear_network_cache_on_exit_, [this](int value) {
                clear_network_cache_on_exit_->setEnabled(value != 0);
            });
    cache_layout->addWidget(cache_label);
    cache_layout->addWidget(maximum_network_cache_megabytes_);
    cache_layout->addWidget(clear_network_cache_on_exit_);
    cache_layout->addStretch();
    network_layout->addLayout(cache_layout);
    network_layout->addStretch();
    network_layout->addWidget(use_proxy_server_);
    network_layout->addStretch();
    tabs->addTab(network_page, QStringLiteral("&Network"));
    layout->addWidget(tabs);

    validation_error_ = new QLabel(this);
    validation_error_->setObjectName(
        QStringLiteral("preferencesValidationError"));
    validation_error_->setWordWrap(true);
    validation_error_->hide();
    layout->addWidget(validation_error_);

    auto standard_buttons = QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
#if defined(Q_OS_LINUX)
    standard_buttons |= QDialogButtonBox::Help;
#endif
    auto* buttons = new QDialogButtonBox(standard_buttons, this);
    buttons->setObjectName(QStringLiteral("preferencesButtonBox"));
    connect(buttons->button(QDialogButtonBox::Ok), &QPushButton::clicked, this,
            &PreferencesDialog::Apply);
    connect(buttons, &QDialogButtonBox::rejected, this,
            &PreferencesDialog::reject);
#if defined(Q_OS_LINUX)
    buttons->button(QDialogButtonBox::Help)
        ->setObjectName(QStringLiteral("preferencesHelpButton"));
    connect(buttons, &QDialogButtonBox::helpRequested, this,
            &PreferencesDialog::HelpRequested);
    auto* help_action = new QAction(this);
    help_action->setObjectName(QStringLiteral("preferencesHelpAction"));
    help_action->setShortcut(QKeySequence(Qt::Key_F1));
    help_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(help_action);
    connect(help_action, &QAction::triggered, this,
            &PreferencesDialog::HelpRequested);
#endif
    layout->addWidget(buttons);
}

void PreferencesDialog::Apply() {
    auto candidate = preferences_;
    candidate.open_new_tabs_in_background = open_in_background_->isChecked();
    candidate.open_new_tabs_after_current = open_after_current_->isChecked();
    candidate.hide_single_tab = hide_single_tab_->isChecked();
    candidate.mru_tab_order = mru_tab_order_->isChecked();
    candidate.escape_hides_main_window = escape_hides_main_window_->isChecked();
    candidate.double_click_translates = double_click_translates_->isChecked();
    candidate.select_word_by_single_click =
        select_word_by_single_click_->isChecked();
    candidate.store_history = store_history_->isChecked();
    candidate.maximum_history_entries =
        static_cast<std::uint32_t>(maximum_history_entries_->value());
    candidate.confirm_favorites_deletion =
        confirm_favorites_deletion_->isChecked();
    candidate.collapse_large_articles = collapse_large_articles_->isChecked();
    candidate.article_size_limit =
        static_cast<std::uint32_t>(article_size_limit_->value());
    candidate.always_expand_optional_parts =
        always_expand_optional_parts_->isChecked();
    candidate.limit_input_phrase_length =
        limit_input_phrase_length_->isChecked();
    candidate.input_phrase_length_limit =
        static_cast<std::uint32_t>(input_phrase_length_limit_->value());
    candidate.ignore_diacritics = ignore_diacritics_->isChecked();
    candidate.synonym_search_enabled = synonym_search_enabled_->isChecked();
    candidate.maximum_dictionary_references =
        static_cast<std::uint16_t>(maximum_dictionary_references_->value());
    candidate.proxy_mode = use_proxy_server_->isChecked()
                               ? goldendict::core::ProxyMode::kManual
                               : goldendict::core::ProxyMode::kDisabled;
    candidate.proxy_type = goldendict::core::ProxyType::kHttpConnect;
    candidate.proxy_host = proxy_host_->text().toStdString();
    candidate.proxy_port = static_cast<std::uint16_t>(proxy_port_->value());
    candidate.maximum_network_cache_megabytes =
        static_cast<std::uint32_t>(maximum_network_cache_megabytes_->value());
    candidate.clear_network_cache_on_exit =
        clear_network_cache_on_exit_->isChecked();
    const QString error = apply_callback_(candidate);
    if (!error.isEmpty()) {
        validation_error_->setText(error);
        validation_error_->show();
        return;
    }
    preferences_ = std::move(candidate);
    accept();
}
