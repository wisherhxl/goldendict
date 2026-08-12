// SPDX-License-Identifier: GPL-3.0-or-later

#include "goldendict/core/favorites_store.h"

#include <expat.h>

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

#include "../foundation/utf8.h"

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-favorites-v1\n";
constexpr std::size_t kMaximumFavoritesBytes = 1024U * 1024U;
constexpr std::size_t kMaximumTextBytes = 4096U;
constexpr std::size_t kMaximumItems = 10000U;
constexpr std::size_t kMaximumDepth = 32U;

struct ParserDeleter {
    void operator()(XML_Parser parser) const noexcept {
        XML_ParserFree(parser);
    }
};

bool Exists(const std::string& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        throw std::runtime_error("Cannot inspect favorites path: " +
                                 error.message());
    }
    return exists;
}

std::string ReadBounded(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open favorites file");
    }
    input.seekg(0, std::ios::end);
    const auto size = input.tellg();
    if (size < 0 ||
        static_cast<std::uintmax_t>(size) > kMaximumFavoritesBytes) {
        throw std::runtime_error("Cannot read bounded favorites file");
    }
    std::string contents(static_cast<std::size_t>(size), '\0');
    input.seekg(0, std::ios::beg);
    input.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!input ||
        input.gcount() != static_cast<std::streamsize>(contents.size())) {
        throw std::runtime_error("Cannot read complete favorites file");
    }
    return contents;
}

void ValidateText(std::string_view text) {
    if (text.empty() || text.size() > kMaximumTextBytes ||
        text.find('\0') != std::string_view::npos ||
        !foundation::IsValidUtf8(text)) {
        throw std::runtime_error("Favorite item text is invalid");
    }
}

void ValidateItems(const Favorites& items, std::size_t depth,
                   std::size_t* total) {
    if (depth > kMaximumDepth) {
        throw std::runtime_error("Favorites tree is too deeply nested");
    }
    for (const auto& item : items) {
        if (++(*total) > kMaximumItems) {
            throw std::runtime_error("Favorites tree has too many items");
        }
        ValidateText(item.text);
        if (item.kind == FavoriteItemKind::kHeadword) {
            if (item.expanded || !item.children.empty()) {
                throw std::runtime_error("Favorite headword has child state");
            }
        } else {
            ValidateItems(item.children, depth + 1U, total);
        }
    }
}

void AppendUint32(std::string* output, std::uint32_t value) {
    for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
        output->push_back(static_cast<char>((value >> shift) & 0xffU));
    }
}

void SerializeItems(const Favorites& items, std::string* output) {
    AppendUint32(output, static_cast<std::uint32_t>(items.size()));
    for (const auto& item : items) {
        output->push_back(item.kind == FavoriteItemKind::kFolder ? '\1' : '\2');
        output->push_back(item.expanded ? '\1' : '\0');
        AppendUint32(output, static_cast<std::uint32_t>(item.text.size()));
        output->append(item.text);
        if (item.kind == FavoriteItemKind::kFolder) {
            SerializeItems(item.children, output);
        }
    }
}

std::string EscapeXml(std::string_view text) {
    std::string escaped;
    for (const char character : text) {
        switch (character) {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            case '\'':
                escaped += "&apos;";
                break;
            default:
                escaped.push_back(character);
                break;
        }
    }
    return escaped;
}

void SerializeXmlItems(const Favorites& items, std::string* output) {
    for (const auto& item : items) {
        if (item.kind == FavoriteItemKind::kHeadword) {
            *output += "<headword>" + EscapeXml(item.text) + "</headword>";
            continue;
        }
        *output += "<folder name=\"" + EscapeXml(item.text) + "\" expanded=\"" +
                   (item.expanded ? "1" : "0") + "\">";
        SerializeXmlItems(item.children, output);
        *output += "</folder>";
    }
}

void WriteAtomically(const std::string& path, std::string_view contents) {
    const std::filesystem::path destination(path);
    if (!destination.parent_path().empty()) {
        std::filesystem::create_directories(destination.parent_path());
    }
    const std::string temporary = destination.string() + ".tmp";
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(contents.data(),
                     static_cast<std::streamsize>(contents.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("Cannot write favorites file");
        }
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        throw std::runtime_error("Cannot replace favorites file: " +
                                 error.message());
    }
}

class BinaryReader {
   public:
    explicit BinaryReader(std::string_view contents) : contents_(contents) {}

    std::uint8_t ReadByte() {
        Require(1U);
        return static_cast<std::uint8_t>(contents_[position_++]);
    }

    std::uint32_t ReadUint32() {
        std::uint32_t value = 0U;
        for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
            value |= static_cast<std::uint32_t>(ReadByte()) << shift;
        }
        return value;
    }

    std::string ReadText() {
        const std::uint32_t size = ReadUint32();
        if (size == 0U || size > kMaximumTextBytes) {
            throw std::runtime_error("Favorite item length is invalid");
        }
        Require(size);
        std::string text(contents_.substr(position_, size));
        position_ += size;
        ValidateText(text);
        return text;
    }

    bool AtEnd() const noexcept { return position_ == contents_.size(); }

   private:
    void Require(std::size_t size) const {
        if (size > contents_.size() - position_) {
            throw std::runtime_error("Favorites data is truncated");
        }
    }

    std::string_view contents_;
    std::size_t position_ = 0U;
};

Favorites ParseItems(BinaryReader* reader, std::size_t depth,
                     std::size_t* total) {
    if (depth > kMaximumDepth) {
        throw std::runtime_error("Favorites tree is too deeply nested");
    }
    const std::uint32_t count = reader->ReadUint32();
    if (count > kMaximumItems - *total) {
        throw std::runtime_error("Favorites tree has too many items");
    }
    Favorites items;
    items.reserve(count);
    for (std::uint32_t index = 0U; index < count; ++index) {
        ++(*total);
        const std::uint8_t kind = reader->ReadByte();
        const std::uint8_t expanded = reader->ReadByte();
        if ((kind != 1U && kind != 2U) || expanded > 1U ||
            (kind == 2U && expanded != 0U)) {
            throw std::runtime_error("Favorite item metadata is invalid");
        }
        FavoriteItem item;
        item.kind = kind == 1U ? FavoriteItemKind::kFolder
                               : FavoriteItemKind::kHeadword;
        item.expanded = expanded != 0U;
        item.text = reader->ReadText();
        if (item.kind == FavoriteItemKind::kFolder) {
            item.children = ParseItems(reader, depth + 1U, total);
        }
        items.push_back(std::move(item));
    }
    return items;
}

Favorites LoadCurrent(const std::string& path) {
    const std::string contents = ReadBounded(path);
    if (contents.size() < kHeader.size() ||
        std::string_view(contents).substr(0U, kHeader.size()) != kHeader) {
        throw std::runtime_error("Unsupported favorites format");
    }
    BinaryReader reader(std::string_view(contents).substr(kHeader.size()));
    std::size_t total = 0U;
    Favorites favorites = ParseItems(&reader, 0U, &total);
    if (!reader.AtEnd()) {
        throw std::runtime_error("Favorites data has trailing bytes");
    }
    return favorites;
}

struct LegacyParserState {
    XML_Parser parser = nullptr;
    Favorites favorites;
    std::vector<std::string> elements;
    std::vector<Favorites*> containers;
    std::string headword;
    std::size_t total = 0U;
    bool failed = false;
    std::string error;
};

void Fail(LegacyParserState* state, std::string message) {
    if (state->failed) {
        return;
    }
    state->failed = true;
    state->error = std::move(message);
    XML_StopParser(state->parser, XML_FALSE);
}

bool HasAttributes(const XML_Char** attributes) {
    return attributes[0] != nullptr;
}

void XMLCALL StartElement(void* user_data, const XML_Char* name,
                          const XML_Char** attributes) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    const std::string_view element(name);
    if (state->elements.empty()) {
        if (element != "root" || HasAttributes(attributes)) {
            Fail(state, "Legacy favorites has an invalid root element");
            return;
        }
        state->elements.emplace_back(name);
        state->containers.push_back(&state->favorites);
        return;
    }
    if (state->elements.back() == "headword") {
        Fail(state, "Legacy favorite headword cannot contain markup");
        return;
    }
    if (state->elements.size() > kMaximumDepth) {
        Fail(state, "Legacy favorites tree is too deeply nested");
        return;
    }
    if (++state->total > kMaximumItems) {
        Fail(state, "Legacy favorites tree has too many items");
        return;
    }
    if (element == "headword") {
        if (HasAttributes(attributes)) {
            Fail(state, "Legacy favorite headword has invalid attributes");
            return;
        }
        state->containers.back()->push_back(
            {FavoriteItemKind::kHeadword, {}, false, {}});
        state->headword.clear();
    } else if (element == "folder") {
        std::string text;
        bool expanded = false;
        bool has_name = false;
        bool has_expanded = false;
        for (std::size_t index = 0U; attributes[index] != nullptr;
             index += 2U) {
            const std::string_view key(attributes[index]);
            const std::string_view value(attributes[index + 1U]);
            if (key == "name" && !has_name) {
                text = std::string(value);
                has_name = true;
            } else if (key == "expanded" && !has_expanded &&
                       (value == "0" || value == "1")) {
                expanded = value == "1";
                has_expanded = true;
            } else {
                Fail(state, "Legacy favorite folder has invalid attributes");
                return;
            }
        }
        try {
            ValidateText(text);
        } catch (const std::exception& error) {
            Fail(state, error.what());
            return;
        }
        state->containers.back()->push_back(
            {FavoriteItemKind::kFolder, std::move(text), expanded, {}});
        state->containers.push_back(&state->containers.back()->back().children);
    } else {
        Fail(state, "Legacy favorites has an unsupported element");
        return;
    }
    state->elements.emplace_back(name);
}

void XMLCALL CharacterData(void* user_data, const XML_Char* value, int length) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (state->elements.empty()) {
        return;
    }
    if (state->elements.back() != "headword") {
        for (int index = 0; index < length; ++index) {
            if (!std::isspace(static_cast<unsigned char>(value[index]))) {
                Fail(state, "Legacy favorites has unexpected text");
                return;
            }
        }
        return;
    }
    if (length < 0 ||
        state->headword.size() + static_cast<std::size_t>(length) >
            kMaximumTextBytes) {
        Fail(state, "Legacy favorite headword is too large");
        return;
    }
    state->headword.append(value, static_cast<std::size_t>(length));
}

void XMLCALL EndElement(void* user_data, const XML_Char* name) {
    auto* state = static_cast<LegacyParserState*>(user_data);
    if (state->elements.empty() || state->elements.back() != name) {
        Fail(state, "Legacy favorites XML is inconsistent");
        return;
    }
    if (state->elements.back() == "headword") {
        try {
            ValidateText(state->headword);
        } catch (const std::exception& error) {
            Fail(state, error.what());
            return;
        }
        state->containers.back()->back().text = std::move(state->headword);
    } else if (state->elements.back() == "folder") {
        state->containers.pop_back();
    } else if (state->elements.back() == "root") {
        state->containers.pop_back();
    }
    state->elements.pop_back();
}

void XMLCALL RejectEntity(void* user_data, const XML_Char*, int,
                          const XML_Char*, int, const XML_Char*,
                          const XML_Char*, const XML_Char*, const XML_Char*) {
    Fail(static_cast<LegacyParserState*>(user_data),
         "Legacy favorites entities are not supported");
}

Favorites LoadLegacy(const std::string& path) {
    const std::string contents = ReadBounded(path);
    std::unique_ptr<std::remove_pointer_t<XML_Parser>, ParserDeleter> parser(
        XML_ParserCreate(nullptr));
    if (!parser) {
        throw std::runtime_error("Cannot create legacy favorites parser");
    }
    LegacyParserState state;
    state.parser = parser.get();
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
            std::string("Malformed legacy favorites XML: ") +
            XML_ErrorString(XML_GetErrorCode(parser.get())));
    }
    if (!state.elements.empty() || !state.containers.empty()) {
        throw std::runtime_error("Legacy favorites XML is incomplete");
    }
    return state.favorites;
}

}  // namespace

Favorites LoadFavorites(const std::string& favorites_path) {
    return Exists(favorites_path) ? LoadCurrent(favorites_path) : Favorites{};
}

void SaveFavorites(const std::string& favorites_path,
                   const Favorites& favorites) {
    std::size_t total = 0U;
    ValidateItems(favorites, 0U, &total);
    std::string contents(kHeader);
    SerializeItems(favorites, &contents);
    if (contents.size() > kMaximumFavoritesBytes) {
        throw std::runtime_error("Favorites exceed the size limit");
    }
    WriteAtomically(favorites_path, contents);
}

Favorites ImportFavoritesXml(const std::string& import_path) {
    return LoadLegacy(import_path);
}

void ExportFavoritesXml(const std::string& export_path,
                        const Favorites& favorites) {
    std::size_t total = 0U;
    ValidateItems(favorites, 0U, &total);
    std::string contents = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><root>";
    SerializeXmlItems(favorites, &contents);
    contents += "</root>\n";
    if (contents.size() > kMaximumFavoritesBytes) {
        throw std::runtime_error("Favorites XML exceeds the size limit");
    }
    WriteAtomically(export_path, contents);
}

Favorites LoadOrMigrateFavorites(const std::string& favorites_path,
                                 const std::string& legacy_favorites_path) {
    if (Exists(favorites_path)) {
        return LoadCurrent(favorites_path);
    }
    if (!Exists(legacy_favorites_path)) {
        return {};
    }
    Favorites favorites = LoadLegacy(legacy_favorites_path);
    SaveFavorites(favorites_path, favorites);
    return favorites;
}

}  // namespace goldendict::core
