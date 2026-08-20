// SPDX-License-Identifier: GPL-3.0-or-later

#include "pending_configuration_transaction.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace goldendict::core {
namespace {

constexpr std::string_view kHeader = "goldendict-pending-transaction-v1\n";
constexpr std::size_t kMaximumPayloadBytes = 1024U * 1024U;
constexpr std::size_t kMaximumRecordBytes = 4U * kMaximumPayloadBytes + 4096U;
constexpr std::size_t kMaximumFailureIdentifierBytes = 64U;

enum class Field : std::uint8_t {
    kVersion = 1U,
    kTransactionId = 2U,
    kPhase = 3U,
    kDesiredAttempt = 4U,
    kDesiredConfiguration = 5U,
    kHistoryIntent = 6U,
    kDesiredHistory = 7U,
    kPreviousConfigurationExists = 8U,
    kPreviousConfiguration = 9U,
    kPreviousHistoryExists = 10U,
    kPreviousHistory = 11U,
    kFailurePresent = 12U,
    kFailureOperation = 13U,
    kFailureDestination = 14U,
    kFailureCategory = 15U,
    kFailureIdentifier = 16U,
};

constexpr std::array<std::uint32_t, 64U> kShaConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
    0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
    0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
    0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
    0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
    0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
    0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
    0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
    0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

std::uint32_t RotateRight(std::uint32_t value, unsigned count) {
    return (value >> count) | (value << (32U - count));
}

std::array<std::uint8_t, 32U> Sha256(std::string_view input) {
    std::string padded(input);
    padded.push_back(static_cast<char>(0x80U));
    while (padded.size() % 64U != 56U)
        padded.push_back('\0');
    const auto bit_size = static_cast<std::uint64_t>(input.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8)
        padded.push_back(static_cast<char>((bit_size >> shift) & 0xffU));

    std::array<std::uint32_t, 8U> hash = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U,
                                          0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
                                          0x1f83d9abU, 0x5be0cd19U};
    for (std::size_t block = 0U; block < padded.size(); block += 64U) {
        std::array<std::uint32_t, 64U> words{};
        for (std::size_t index = 0U; index < 16U; ++index) {
            const auto offset = block + index * 4U;
            words[index] = (static_cast<std::uint32_t>(
                                static_cast<unsigned char>(padded[offset]))
                            << 24U) |
                           (static_cast<std::uint32_t>(
                                static_cast<unsigned char>(padded[offset + 1U]))
                            << 16U) |
                           (static_cast<std::uint32_t>(
                                static_cast<unsigned char>(padded[offset + 2U]))
                            << 8U) |
                           static_cast<std::uint32_t>(
                               static_cast<unsigned char>(padded[offset + 3U]));
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto s0 = RotateRight(words[index - 15U], 7U) ^
                            RotateRight(words[index - 15U], 18U) ^
                            (words[index - 15U] >> 3U);
            const auto s1 = RotateRight(words[index - 2U], 17U) ^
                            RotateRight(words[index - 2U], 19U) ^
                            (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
        }
        auto a = hash[0];
        auto b = hash[1];
        auto c = hash[2];
        auto d = hash[3];
        auto e = hash[4];
        auto f = hash[5];
        auto g = hash[6];
        auto h = hash[7];
        for (std::size_t index = 0U; index < words.size(); ++index) {
            const auto sum1 =
                RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
            const auto choice = (e & f) ^ (~e & g);
            const auto temporary1 =
                h + sum1 + choice + kShaConstants[index] + words[index];
            const auto sum0 =
                RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }
    std::array<std::uint8_t, 32U> digest{};
    for (std::size_t index = 0U; index < hash.size(); ++index) {
        for (std::size_t byte = 0U; byte < 4U; ++byte)
            digest[index * 4U + byte] =
                static_cast<std::uint8_t>(hash[index] >> (24U - byte * 8U));
    }
    return digest;
}

void AppendU32(std::string& output, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void AppendU64(std::string& output, std::uint64_t value) {
    for (int shift = 56; shift >= 0; shift -= 8)
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
}

void AppendField(std::string& output, Field field, std::string_view value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Pending transaction field is too large");
    output.push_back(static_cast<char>(field));
    AppendU32(output, static_cast<std::uint32_t>(value.size()));
    output.append(value);
}

std::string Byte(std::uint8_t value) {
    return std::string(1U, static_cast<char>(value));
}

std::string EncodeIdentity(const std::array<std::uint8_t, 16U>& identity) {
    constexpr char kHex[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(32U);
    for (const auto byte : identity) {
        encoded.push_back(kHex[byte >> 4U]);
        encoded.push_back(kHex[byte & 0xfU]);
    }
    return encoded;
}

std::string EncodePayload(const PendingTransactionPayload& payload) {
    if (payload.bytes.size() > kMaximumPayloadBytes ||
        payload.size != payload.bytes.size() ||
        payload.sha256 != Sha256(payload.bytes))
        throw std::runtime_error("Pending transaction payload is invalid");
    std::string encoded;
    encoded.reserve(40U + payload.bytes.size());
    AppendU64(encoded, payload.size);
    encoded.append(reinterpret_cast<const char*>(payload.sha256.data()),
                   payload.sha256.size());
    encoded += payload.bytes;
    return encoded;
}

bool IsKnown(PendingTransactionPhase value) {
    return value >= PendingTransactionPhase::kPrepared &&
           value <= PendingTransactionPhase::kQuarantined;
}

bool RequiresFailureEvidence(PendingTransactionPhase value) {
    return value == PendingTransactionPhase::kDesiredPersistenceFailed ||
           value == PendingTransactionPhase::kDesiredRuntimeFailed ||
           value == PendingTransactionPhase::kPreviousPersistenceBlocked ||
           value == PendingTransactionPhase::kQuarantined;
}

bool IsSafeIdentifier(std::string_view value) {
    if (value.empty() || value.size() > kMaximumFailureIdentifierBytes)
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return (character >= 'a' && character <= 'z') ||
               (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '_' ||
               character == '-';
    });
}

void Validate(const PendingConfigurationTransactionRecord& record) {
    if (record.version != PendingTransactionVersion::kV1 ||
        !IsKnown(record.phase) ||
        (record.desired_recovery_attempt !=
             DesiredRecoveryAttempt::kNotAttempted &&
         record.desired_recovery_attempt != DesiredRecoveryAttempt::kAttempted))
        throw std::runtime_error("Pending transaction enum is invalid");
    if (std::all_of(record.transaction_id.begin(), record.transaction_id.end(),
                    [](std::uint8_t byte) { return byte == 0U; }))
        throw std::runtime_error("Pending transaction identity is invalid");
    EncodePayload(record.desired_configuration);
    if ((record.history_intent == PendingHistoryIntent::kUnchanged &&
         record.desired_history) ||
        (record.history_intent == PendingHistoryIntent::kReplace &&
         !record.desired_history) ||
        (record.history_intent != PendingHistoryIntent::kUnchanged &&
         record.history_intent != PendingHistoryIntent::kReplace))
        throw std::runtime_error(
            "Pending transaction history intent is invalid");
    if (record.desired_history)
        EncodePayload(*record.desired_history);
    const auto validate_previous =
        [](const PendingPreviousDestination& previous) {
            if (previous.existed != previous.payload.has_value())
                throw std::runtime_error(
                    "Pending transaction previous state is invalid");
            if (previous.payload)
                EncodePayload(*previous.payload);
        };
    validate_previous(record.previous_configuration);
    if (record.history_intent == PendingHistoryIntent::kUnchanged) {
        if (record.previous_history.existed || record.previous_history.payload)
            throw std::runtime_error(
                "Unchanged history cannot have previous state");
    } else {
        validate_previous(record.previous_history);
    }
    if (record.failure) {
        if (record.failure->operation < PendingFailureOperation::kReadRecord ||
            record.failure->operation > PendingFailureOperation::kQuarantine ||
            record.failure->destination <
                PendingFailureDestination::kPendingRecord ||
            record.failure->destination >
                PendingFailureDestination::kRuntimePresentation ||
            record.failure->category < PendingFailureCategory::kNotFound ||
            record.failure->category > PendingFailureCategory::kUnknown ||
            !IsSafeIdentifier(record.failure->identifier))
            throw std::runtime_error(
                "Pending transaction failure identity is invalid");
    } else if (RequiresFailureEvidence(record.phase)) {
        throw std::runtime_error(
            "Pending transaction phase requires failure evidence");
    }
}

class Reader {
   public:
    explicit Reader(std::string_view input) : input_(input) {}

    std::string_view FieldValue(Field expected) {
        if (position_ + 5U > input_.size() ||
            static_cast<std::uint8_t>(input_[position_]) !=
                static_cast<std::uint8_t>(expected))
            throw std::runtime_error(
                "Pending transaction field is missing or out of order");
        ++position_;
        const auto length = U32();
        if (length > input_.size() - position_)
            throw std::runtime_error("Pending transaction is truncated");
        const auto value = input_.substr(position_, length);
        position_ += length;
        return value;
    }

    bool AtEnd() const noexcept { return position_ == input_.size(); }

   private:
    std::uint32_t U32() {
        std::uint32_t value = 0U;
        for (std::size_t index = 0U; index < 4U; ++index)
            value =
                (value << 8U) | static_cast<unsigned char>(input_[position_++]);
        return value;
    }

    std::string_view input_;
    std::size_t position_ = 0U;
};

std::uint8_t ParseByte(std::string_view value) {
    if (value.size() != 1U)
        throw std::runtime_error(
            "Pending transaction scalar has invalid width");
    return static_cast<std::uint8_t>(value[0]);
}

bool ParseBoolean(std::string_view value) {
    const auto parsed = ParseByte(value);
    if (parsed > 1U)
        throw std::runtime_error("Pending transaction boolean is invalid");
    return parsed == 1U;
}

std::uint64_t ParseU64(std::string_view value) {
    if (value.size() < 8U)
        throw std::runtime_error("Pending transaction payload is truncated");
    std::uint64_t result = 0U;
    for (std::size_t index = 0U; index < 8U; ++index)
        result = (result << 8U) | static_cast<unsigned char>(value[index]);
    return result;
}

PendingTransactionPayload ParsePayload(std::string_view value) {
    if (value.size() < 40U || value.size() - 40U > kMaximumPayloadBytes)
        throw std::runtime_error(
            "Pending transaction payload has invalid size");
    PendingTransactionPayload payload;
    payload.size = ParseU64(value);
    std::copy_n(reinterpret_cast<const std::uint8_t*>(value.data() + 8U), 32U,
                payload.sha256.begin());
    payload.bytes = std::string(value.substr(40U));
    if (payload.size != payload.bytes.size() ||
        payload.sha256 != Sha256(payload.bytes))
        throw std::runtime_error(
            "Pending transaction payload digest does not match");
    return payload;
}

std::array<std::uint8_t, 16U> ParseIdentity(std::string_view value) {
    if (value.size() != 32U)
        throw std::runtime_error(
            "Pending transaction identity has invalid width");
    std::array<std::uint8_t, 16U> identity{};
    const auto nibble = [](char character) -> std::uint8_t {
        if (character >= '0' && character <= '9')
            return character - '0';
        if (character >= 'a' && character <= 'f')
            return character - 'a' + 10U;
        throw std::runtime_error(
            "Pending transaction identity is not canonical");
    };
    for (std::size_t index = 0U; index < identity.size(); ++index)
        identity[index] = static_cast<std::uint8_t>(
            (nibble(value[index * 2U]) << 4U) | nibble(value[index * 2U + 1U]));
    return identity;
}

}  // namespace

PendingTransactionPayload MakePendingTransactionPayload(std::string bytes) {
    if (bytes.size() > kMaximumPayloadBytes)
        throw std::runtime_error(
            "Pending transaction payload exceeds the size limit");
    PendingTransactionPayload payload;
    payload.size = bytes.size();
    payload.sha256 = Sha256(bytes);
    payload.bytes = std::move(bytes);
    return payload;
}

std::string SerializePendingConfigurationTransaction(
    const PendingConfigurationTransactionRecord& record) {
    Validate(record);
    std::string output(kHeader);
    AppendField(output, Field::kVersion,
                Byte(static_cast<std::uint8_t>(record.version)));
    AppendField(output, Field::kTransactionId,
                EncodeIdentity(record.transaction_id));
    AppendField(output, Field::kPhase,
                Byte(static_cast<std::uint8_t>(record.phase)));
    AppendField(
        output, Field::kDesiredAttempt,
        Byte(static_cast<std::uint8_t>(record.desired_recovery_attempt)));
    AppendField(output, Field::kDesiredConfiguration,
                EncodePayload(record.desired_configuration));
    AppendField(output, Field::kHistoryIntent,
                Byte(static_cast<std::uint8_t>(record.history_intent)));
    AppendField(output, Field::kDesiredHistory,
                record.desired_history ? EncodePayload(*record.desired_history)
                                       : std::string{});
    AppendField(output, Field::kPreviousConfigurationExists,
                Byte(record.previous_configuration.existed ? 1U : 0U));
    AppendField(output, Field::kPreviousConfiguration,
                record.previous_configuration.payload
                    ? EncodePayload(*record.previous_configuration.payload)
                    : std::string{});
    AppendField(output, Field::kPreviousHistoryExists,
                Byte(record.previous_history.existed ? 1U : 0U));
    AppendField(output, Field::kPreviousHistory,
                record.previous_history.payload
                    ? EncodePayload(*record.previous_history.payload)
                    : std::string{});
    AppendField(output, Field::kFailurePresent, Byte(record.failure ? 1U : 0U));
    AppendField(output, Field::kFailureOperation,
                record.failure
                    ? Byte(static_cast<std::uint8_t>(record.failure->operation))
                    : std::string{});
    AppendField(
        output, Field::kFailureDestination,
        record.failure
            ? Byte(static_cast<std::uint8_t>(record.failure->destination))
            : std::string{});
    AppendField(output, Field::kFailureCategory,
                record.failure
                    ? Byte(static_cast<std::uint8_t>(record.failure->category))
                    : std::string{});
    AppendField(output, Field::kFailureIdentifier,
                record.failure ? record.failure->identifier : std::string{});
    if (output.size() > kMaximumRecordBytes)
        throw std::runtime_error(
            "Pending transaction record exceeds the size limit");
    return output;
}

PendingConfigurationTransactionRecord ParsePendingConfigurationTransaction(
    const std::string& bytes) {
    if (bytes.size() > kMaximumRecordBytes ||
        bytes.substr(0U, kHeader.size()) != kHeader)
        throw std::runtime_error("Unsupported pending transaction format");
    Reader reader(std::string_view(bytes).substr(kHeader.size()));
    PendingConfigurationTransactionRecord record;
    record.version = static_cast<PendingTransactionVersion>(
        ParseByte(reader.FieldValue(Field::kVersion)));
    record.transaction_id =
        ParseIdentity(reader.FieldValue(Field::kTransactionId));
    record.phase = static_cast<PendingTransactionPhase>(
        ParseByte(reader.FieldValue(Field::kPhase)));
    record.desired_recovery_attempt = static_cast<DesiredRecoveryAttempt>(
        ParseByte(reader.FieldValue(Field::kDesiredAttempt)));
    record.desired_configuration =
        ParsePayload(reader.FieldValue(Field::kDesiredConfiguration));
    record.history_intent = static_cast<PendingHistoryIntent>(
        ParseByte(reader.FieldValue(Field::kHistoryIntent)));
    const auto desired_history = reader.FieldValue(Field::kDesiredHistory);
    if (!desired_history.empty())
        record.desired_history = ParsePayload(desired_history);
    record.previous_configuration.existed =
        ParseBoolean(reader.FieldValue(Field::kPreviousConfigurationExists));
    const auto previous_configuration =
        reader.FieldValue(Field::kPreviousConfiguration);
    if (!previous_configuration.empty())
        record.previous_configuration.payload =
            ParsePayload(previous_configuration);
    record.previous_history.existed =
        ParseBoolean(reader.FieldValue(Field::kPreviousHistoryExists));
    const auto previous_history = reader.FieldValue(Field::kPreviousHistory);
    if (!previous_history.empty())
        record.previous_history.payload = ParsePayload(previous_history);
    const auto failure_present =
        ParseBoolean(reader.FieldValue(Field::kFailurePresent));
    const auto operation = reader.FieldValue(Field::kFailureOperation);
    const auto destination = reader.FieldValue(Field::kFailureDestination);
    const auto category = reader.FieldValue(Field::kFailureCategory);
    const auto identifier = reader.FieldValue(Field::kFailureIdentifier);
    if (failure_present) {
        record.failure = PendingFailureIdentity{
            static_cast<PendingFailureOperation>(ParseByte(operation)),
            static_cast<PendingFailureDestination>(ParseByte(destination)),
            static_cast<PendingFailureCategory>(ParseByte(category)),
            std::string(identifier)};
    } else if (!operation.empty() || !destination.empty() ||
               !category.empty() || !identifier.empty()) {
        throw std::runtime_error(
            "Pending transaction failure fields are inconsistent");
    }
    if (!reader.AtEnd())
        throw std::runtime_error("Pending transaction has trailing fields");
    Validate(record);
    return record;
}

}  // namespace goldendict::core
