// SPDX-License-Identifier: GPL-3.0-or-later
#include "image_codec.h"

#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QIODevice>

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace goldendict::core::image_codec {
namespace {

class BoundedOutput final : public QIODevice {
   public:
    BoundedOutput(std::size_t maximum_bytes,
                  const std::function<void()>& checkpoint)
        : maximum_bytes_(maximum_bytes), checkpoint_(checkpoint) {
        open(QIODevice::WriteOnly);
    }

    bool isSequential() const override { return true; }
    std::vector<std::byte> TakeData() { return std::move(data_); }

   private:
    qint64 readData(char*, qint64) override { return -1; }
    qint64 writeData(const char* bytes, qint64 count) override {
        checkpoint_();
        if (count < 0 || static_cast<std::uint64_t>(count) >
                             maximum_bytes_ - data_.size()) {
            throw std::length_error("Encoded image exceeds resource limit");
        }
        const auto* first = reinterpret_cast<const std::byte*>(bytes);
        data_.insert(data_.end(), first, first + count);
        return count;
    }

    std::size_t maximum_bytes_;
    const std::function<void()>& checkpoint_;
    std::vector<std::byte> data_;
};

}  // namespace

std::vector<std::byte> DecodeToBmp(
    const std::vector<std::byte>& input, std::size_t maximum_bytes,
    const std::function<void()>& checkpoint) {
    checkpoint();
    if (input.empty()) return {};
    if (input.size() > maximum_bytes ||
        input.size() > static_cast<std::size_t>(
                           std::numeric_limits<int>::max())) {
        throw std::length_error("Encoded image exceeds resource limit");
    }
    auto bytes = QByteArray::fromRawData(
        reinterpret_cast<const char*>(input.data()),
        static_cast<int>(input.size()));
    QBuffer source(&bytes);
    source.open(QIODevice::ReadOnly);
    QImageReader reader(&source);
    reader.setDecideFormatFromContent(true);
    const auto size = reader.size();
    if (!size.isValid() || size.isEmpty()) return {};
    // QImage can use up to 16 bytes per pixel (RGBA32FPx4). Account for
    // alignment and the BMP header before allowing an untrusted allocation.
    const auto row_bytes = static_cast<std::uint64_t>(size.width()) * 16U;
    if (maximum_bytes <= 1024U || row_bytes > maximum_bytes - 1024U ||
        static_cast<std::uint64_t>(size.height()) >
            (maximum_bytes - 1024U) / row_bytes) {
        throw std::length_error("Decoded image exceeds resource limit");
    }
    checkpoint();
    QImage image = reader.read();
    checkpoint();
    if (image.isNull()) return {};
    if (static_cast<std::uint64_t>(image.sizeInBytes()) > maximum_bytes) {
        throw std::length_error("Decoded image exceeds resource limit");
    }
    BoundedOutput output(maximum_bytes, checkpoint);
    if (!image.save(&output, "BMP")) return {};
    checkpoint();
    return output.TakeData();
}

}  // namespace goldendict::core::image_codec
