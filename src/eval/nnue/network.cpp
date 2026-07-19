#include "eval/nnue/network.h"

#include "core/bitboard.h"
#include "core/nnue_state.h"
#include "position/position.h"
#include "position/state.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <new>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace Eunshin::NNUE {

struct Network::Parameters final {
    std::vector<std::int16_t> ftWeights;
    std::array<std::int32_t, HIDDEN> ftBias{};
    std::array<std::int16_t, OUTPUT_INPUTS> outputWeights{};
    std::int32_t outputBias = 0;
    std::string sourcePath;
    std::string payloadDigest;
    std::uint64_t generation = 0;
};

namespace {

constexpr unsigned char MAGIC[8] = {'S', 'B', 'N', 'N', 'U', 'E', '2', '\0'};
constexpr std::uint32_t FORMAT_VERSION = 2;
constexpr std::uint32_t HEADER_BYTES = 192;
constexpr std::uint32_t ENDIAN_MARKER = 0x01020304U;
constexpr std::uint32_t ARCHITECTURE_A_256 = 1;
constexpr std::uint32_t FEATURE_PERSPECTIVE_32K_PCSQ_V2 = 1;
constexpr std::uint32_t KING_BUCKETS = 32;
constexpr std::uint32_t HIDDEN_COUNT = 1;
constexpr std::uint32_t ACTIVATION_CLIPPED_RELU = 1;
constexpr std::uint32_t QUANTIZATION_SYMMETRIC_INTEGER = 1;
constexpr std::uint32_t TENSOR_COUNT = 4;
constexpr std::uint32_t TENSOR_ENTRY_BYTES = 64;
constexpr std::uint64_t DIRECTORY_BYTES =
    static_cast<std::uint64_t>(TENSOR_COUNT) * TENSOR_ENTRY_BYTES;
constexpr std::uint64_t FT_WEIGHT_COUNT =
    static_cast<std::uint64_t>(INPUT_FEATURES) * HIDDEN;
constexpr std::uint64_t FT_WEIGHT_BYTES = FT_WEIGHT_COUNT * 2ULL;
constexpr std::uint64_t FT_BIAS_BYTES = static_cast<std::uint64_t>(HIDDEN) * 4ULL;
constexpr std::uint64_t OUTPUT_WEIGHT_BYTES =
    static_cast<std::uint64_t>(OUTPUT_INPUTS) * 2ULL;
constexpr std::uint64_t OUTPUT_BIAS_BYTES = 4;
constexpr std::uint64_t PAYLOAD_BYTES =
    FT_WEIGHT_BYTES + FT_BIAS_BYTES + OUTPUT_WEIGHT_BYTES + OUTPUT_BIAS_BYTES;
constexpr std::size_t MAX_PATH_BYTES = 32768;

enum ElementType : std::uint16_t {
    ElementInt8 = 1,
    ElementInt16 = 2,
    ElementInt32 = 3
};

struct Header final {
    std::uint32_t version = 0;
    std::uint32_t headerBytes = 0;
    std::uint32_t endianMarker = 0;
    std::uint32_t architecture = 0;
    std::uint32_t featureSet = 0;
    std::uint32_t kingBuckets = 0;
    std::uint32_t hiddenCount = 0;
    std::array<std::uint32_t, 4> hiddenDimensions{};
    std::uint32_t activation = 0;
    std::uint32_t quantization = 0;
    std::uint32_t scale = 0;
    std::uint32_t tensorCount = 0;
    std::uint32_t tensorEntryBytes = 0;
    std::uint64_t directoryBytes = 0;
    std::uint64_t payloadBytes = 0;
    std::array<unsigned char, 32> payloadSha256{};
    std::array<unsigned char, 32> trainingRunId{};
    std::array<unsigned char, 32> datasetManifestSha256{};
    std::uint64_t reserved = 0;
};

struct Tensor final {
    std::uint32_t id = 0;
    std::uint16_t elementType = 0;
    std::uint16_t rank = 0;
    std::array<std::uint32_t, 4> dimensions{};
    std::uint64_t elementCount = 0;
    std::uint64_t offset = 0;
    std::uint64_t bytes = 0;
    std::uint32_t flags = 0;
    std::uint32_t reserved32 = 0;
    std::uint64_t reserved64 = 0;
};

struct TensorSpec final {
    std::uint32_t id;
    std::uint16_t elementType;
    std::uint16_t rank;
    std::array<std::uint32_t, 4> dimensions;
    std::uint64_t offset;
    std::uint64_t bytes;
};

constexpr TensorSpec REQUIRED_TENSORS[TENSOR_COUNT] = {
    {1, ElementInt16, 2, {24576, 256, 0, 0}, 0, FT_WEIGHT_BYTES},
    {2, ElementInt32, 1, {256, 0, 0, 0}, FT_WEIGHT_BYTES, FT_BIAS_BYTES},
    {3, ElementInt16, 1, {518, 0, 0, 0},
        FT_WEIGHT_BYTES + FT_BIAS_BYTES, OUTPUT_WEIGHT_BYTES},
    {4, ElementInt32, 1, {1, 0, 0, 0},
        FT_WEIGHT_BYTES + FT_BIAS_BYTES + OUTPUT_WEIGHT_BYTES,
        OUTPUT_BIAS_BYTES}
};

struct Candidate final {
    std::vector<std::int16_t> ftWeights;
    std::array<std::int32_t, HIDDEN> ftBias{};
    std::array<std::int16_t, OUTPUT_INPUTS> outputWeights{};
    std::int32_t outputBias = 0;
    std::string sourcePath;
    std::string payloadDigest;
};

[[nodiscard]] std::uint16_t readU16LE(const unsigned char* data) noexcept {
    return static_cast<std::uint16_t>(data[0])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(data[1]) << 8);
}

[[nodiscard]] std::uint32_t readU32LE(const unsigned char* data) noexcept {
    return static_cast<std::uint32_t>(data[0])
         | (static_cast<std::uint32_t>(data[1]) << 8)
         | (static_cast<std::uint32_t>(data[2]) << 16)
         | (static_cast<std::uint32_t>(data[3]) << 24);
}

[[nodiscard]] std::uint64_t readU64LE(const unsigned char* data) noexcept {
    std::uint64_t value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
        value |= static_cast<std::uint64_t>(data[shift / 8]) << shift;
    return value;
}

[[nodiscard]] std::int16_t readI16LE(const unsigned char* data) noexcept {
    const std::uint16_t value = readU16LE(data);
    const std::int32_t signedValue = value <= 0x7FFFU
        ? static_cast<std::int32_t>(value)
        : static_cast<std::int32_t>(value) - 0x10000;
    return static_cast<std::int16_t>(signedValue);
}

[[nodiscard]] std::int32_t readI32LE(const unsigned char* data) noexcept {
    const std::uint32_t value = readU32LE(data);
    const std::int64_t signedValue = value <= 0x7FFFFFFFU
        ? static_cast<std::int64_t>(value)
        : static_cast<std::int64_t>(value) - 0x100000000LL;
    return static_cast<std::int32_t>(signedValue);
}

[[nodiscard]] bool checkedMultiply(
    std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& output) noexcept {
    if (lhs != 0 && rhs > std::numeric_limits<std::uint64_t>::max() / lhs)
        return false;
    output = lhs * rhs;
    return true;
}

[[nodiscard]] bool checkedAdd(
    std::uint64_t lhs, std::uint64_t rhs, std::uint64_t& output) noexcept {
    if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs)
        return false;
    output = lhs + rhs;
    return true;
}

[[nodiscard]] bool fail(
    std::string& error, const char* code, std::string detail = {}) {
    error = code;
    if (!detail.empty()) {
        error += ": ";
        error += std::move(detail);
    }
    return false;
}

template <std::size_t Size>
[[nodiscard]] bool anyNonZero(
    const std::array<unsigned char, Size>& bytes) noexcept {
    for (const unsigned char value : bytes) {
        if (value != 0)
            return true;
    }
    return false;
}

[[nodiscard]] std::uint32_t rotateRight(
    std::uint32_t value, unsigned count) noexcept {
    return (value >> count) | (value << (32U - count));
}

class Sha256 final {
public:
    Sha256() noexcept { reset(); }

    void update(const unsigned char* input, std::size_t length) noexcept {
        for (std::size_t i = 0; i < length; ++i) {
            data_[dataLength_++] = input[i];
            if (dataLength_ == data_.size()) {
                transform();
                bitLength_ += 512;
                dataLength_ = 0;
            }
        }
    }

    [[nodiscard]] std::array<unsigned char, 32> finish() noexcept {
        std::array<unsigned char, 32> digest{};
        const std::uint64_t totalBits =
            bitLength_ + static_cast<std::uint64_t>(dataLength_) * 8ULL;
        std::size_t indexValue = dataLength_;
        data_[indexValue++] = 0x80U;
        if (indexValue > 56) {
            while (indexValue < data_.size())
                data_[indexValue++] = 0;
            transform();
            indexValue = 0;
        }
        while (indexValue < 56)
            data_[indexValue++] = 0;
        for (int shift = 56; shift >= 0; shift -= 8) {
            data_[indexValue++] = static_cast<unsigned char>(
                (totalBits >> static_cast<unsigned>(shift)) & 0xFFU);
        }
        transform();

        for (std::size_t word = 0; word < state_.size(); ++word) {
            digest[word * 4] = static_cast<unsigned char>((state_[word] >> 24) & 0xFFU);
            digest[word * 4 + 1] = static_cast<unsigned char>((state_[word] >> 16) & 0xFFU);
            digest[word * 4 + 2] = static_cast<unsigned char>((state_[word] >> 8) & 0xFFU);
            digest[word * 4 + 3] = static_cast<unsigned char>(state_[word] & 0xFFU);
        }
        return digest;
    }

private:
    void reset() noexcept {
        data_.fill(0);
        dataLength_ = 0;
        bitLength_ = 0;
        state_ = {0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                  0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    }

    void transform() noexcept {
        static constexpr std::uint32_t CONSTANTS[64] = {
            0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
            0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
            0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
            0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
            0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
            0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
            0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
            0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
            0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
            0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
            0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
            0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
            0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
            0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
            0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
            0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
        };

        std::uint32_t words[64]{};
        for (int i = 0; i < 16; ++i) {
            const std::size_t offset = static_cast<std::size_t>(i) * 4;
            words[i] = (static_cast<std::uint32_t>(data_[offset]) << 24)
                     | (static_cast<std::uint32_t>(data_[offset + 1]) << 16)
                     | (static_cast<std::uint32_t>(data_[offset + 2]) << 8)
                     | static_cast<std::uint32_t>(data_[offset + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotateRight(words[i - 15], 7)
                                   ^ rotateRight(words[i - 15], 18)
                                   ^ (words[i - 15] >> 3);
            const std::uint32_t s1 = rotateRight(words[i - 2], 17)
                                   ^ rotateRight(words[i - 2], 19)
                                   ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t sum1 = rotateRight(e, 6)
                                     ^ rotateRight(e, 11)
                                     ^ rotateRight(e, 25);
            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + sum1 + choose
                                      + CONSTANTS[i] + words[i];
            const std::uint32_t sum0 = rotateRight(a, 2)
                                     ^ rotateRight(a, 13)
                                     ^ rotateRight(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<unsigned char, 64> data_{};
    std::size_t dataLength_ = 0;
    std::uint64_t bitLength_ = 0;
    std::array<std::uint32_t, 8> state_{};
};

[[nodiscard]] std::array<unsigned char, 32> sha256(
    const std::vector<unsigned char>& bytes) noexcept {
    Sha256 hash;
    if (!bytes.empty())
        hash.update(bytes.data(), bytes.size());
    return hash.finish();
}

[[nodiscard]] std::string hexDigest(
    const std::array<unsigned char, 32>& digest) {
    static constexpr char HEX[] = "0123456789abcdef";
    std::string result;
    result.reserve(64);
    for (const unsigned char value : digest) {
        result.push_back(HEX[value >> 4]);
        result.push_back(HEX[value & 0x0FU]);
    }
    return result;
}

[[nodiscard]] std::filesystem::path executableDirectory() noexcept {
#if defined(_WIN32)
    try {
        std::array<wchar_t, 32768> buffer{};
        const DWORD length = GetModuleFileNameW(
            nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return {};
        return std::filesystem::path(
            std::wstring(buffer.data(), static_cast<std::size_t>(length))).parent_path();
    } catch (...) {
        return {};
    }
#else
    return {};
#endif
}

[[nodiscard]] std::filesystem::path resolvePath(
    std::string_view requested, std::string& error) {
    if (requested.empty()) {
        (void)fail(error, "PATH_EMPTY");
        return {};
    }
    if (requested.size() > MAX_PATH_BYTES) {
        (void)fail(error, "PATH_TOO_LONG");
        return {};
    }
    if (requested.find('\0') != std::string_view::npos) {
        (void)fail(error, "PATH_INVALID", "embedded NUL");
        return {};
    }

    try {
        const std::filesystem::path direct =
            std::filesystem::u8path(requested.begin(), requested.end());
        std::error_code existsError;
        if (direct.is_absolute() || std::filesystem::exists(direct, existsError))
            return direct;

        const std::filesystem::path directory = executableDirectory();
        if (!directory.empty()) {
            const std::filesystem::path besideExecutable = directory / direct;
            existsError.clear();
            if (std::filesystem::exists(besideExecutable, existsError))
                return besideExecutable;
        }
        return direct;
    } catch (const std::exception& exception) {
        (void)fail(error, "PATH_INVALID", exception.what());
        return {};
    }
}

[[nodiscard]] bool readExact(
    std::ifstream& stream, unsigned char* destination, std::size_t bytes) {
    if (bytes == 0)
        return true;
    stream.read(reinterpret_cast<char*>(destination),
                static_cast<std::streamsize>(bytes));
    return static_cast<std::size_t>(stream.gcount()) == bytes;
}

[[nodiscard]] bool parseHeader(
    const std::array<unsigned char, HEADER_BYTES>& raw,
    Header& header,
    std::string& error) {
    if (!std::equal(std::begin(MAGIC), std::end(MAGIC), raw.begin()))
        return fail(error, "BAD_MAGIC");

    header.version = readU32LE(raw.data() + 8);
    header.headerBytes = readU32LE(raw.data() + 12);
    header.endianMarker = readU32LE(raw.data() + 16);
    header.architecture = readU32LE(raw.data() + 20);
    header.featureSet = readU32LE(raw.data() + 24);
    header.kingBuckets = readU32LE(raw.data() + 28);
    header.hiddenCount = readU32LE(raw.data() + 32);
    for (std::size_t i = 0; i < header.hiddenDimensions.size(); ++i)
        header.hiddenDimensions[i] = readU32LE(raw.data() + 36 + i * 4);
    header.activation = readU32LE(raw.data() + 52);
    header.quantization = readU32LE(raw.data() + 56);
    header.scale = readU32LE(raw.data() + 60);
    header.tensorCount = readU32LE(raw.data() + 64);
    header.tensorEntryBytes = readU32LE(raw.data() + 68);
    header.directoryBytes = readU64LE(raw.data() + 72);
    header.payloadBytes = readU64LE(raw.data() + 80);
    std::copy(raw.begin() + 88, raw.begin() + 120, header.payloadSha256.begin());
    std::copy(raw.begin() + 120, raw.begin() + 152, header.trainingRunId.begin());
    std::copy(raw.begin() + 152, raw.begin() + 184,
              header.datasetManifestSha256.begin());
    header.reserved = readU64LE(raw.data() + 184);

    if (header.version != FORMAT_VERSION)
        return fail(error, "UNSUPPORTED_VERSION");
    if (header.headerBytes != HEADER_BYTES)
        return fail(error, "BAD_HEADER_SIZE");
    if (header.endianMarker != ENDIAN_MARKER)
        return fail(error, "BAD_ENDIAN_MARKER");
    if (header.architecture != ARCHITECTURE_A_256)
        return fail(error, "UNSUPPORTED_ARCHITECTURE");
    if (header.featureSet != FEATURE_PERSPECTIVE_32K_PCSQ_V2)
        return fail(error, "BAD_FEATURE_SET");
    if (header.kingBuckets != KING_BUCKETS
        || header.hiddenCount != HIDDEN_COUNT
        || header.hiddenDimensions != std::array<std::uint32_t, 4>{256, 0, 0, 0}) {
        return fail(error, "BAD_DIMENSIONS");
    }
    if (header.activation != ACTIVATION_CLIPPED_RELU)
        return fail(error, "BAD_ACTIVATION");
    if (header.quantization != QUANTIZATION_SYMMETRIC_INTEGER)
        return fail(error, "BAD_QUANTIZATION");
    if (header.scale != QUANTIZATION_SCALE)
        return fail(error, "BAD_QUANTIZATION_SCALE");
    if (header.tensorCount != TENSOR_COUNT)
        return fail(error, "TENSOR_COUNT_MISMATCH");
    if (header.tensorEntryBytes != TENSOR_ENTRY_BYTES)
        return fail(error, "BAD_TENSOR_ENTRY_SIZE");

    std::uint64_t computedDirectoryBytes = 0;
    if (!checkedMultiply(header.tensorCount, header.tensorEntryBytes,
                         computedDirectoryBytes)) {
        return fail(error, "DIRECTORY_SIZE_OVERFLOW");
    }
    if (computedDirectoryBytes != header.directoryBytes
        || header.directoryBytes != DIRECTORY_BYTES) {
        return fail(error, "DIRECTORY_SIZE_MISMATCH");
    }
    if (header.payloadBytes != PAYLOAD_BYTES)
        return fail(error, "PAYLOAD_SIZE_MISMATCH");
    if (!anyNonZero(header.trainingRunId)
        || !anyNonZero(header.datasetManifestSha256)) {
        return fail(error, "BAD_METADATA_ID");
    }
    if (header.reserved != 0)
        return fail(error, "NONZERO_RESERVED");
    return true;
}

[[nodiscard]] std::uint64_t elementSize(std::uint16_t type) noexcept {
    if (type == ElementInt8)
        return 1;
    if (type == ElementInt16)
        return 2;
    if (type == ElementInt32)
        return 4;
    return 0;
}

[[nodiscard]] bool parseDirectory(
    const std::array<unsigned char, DIRECTORY_BYTES>& raw,
    const Header& header,
    std::string& error) {
    std::uint64_t nextOffset = 0;
    for (std::uint32_t entryIndex = 0;
         entryIndex < TENSOR_COUNT;
         ++entryIndex) {
        const unsigned char* data = raw.data()
            + static_cast<std::size_t>(entryIndex) * TENSOR_ENTRY_BYTES;
        Tensor tensor;
        tensor.id = readU32LE(data);
        tensor.elementType = readU16LE(data + 4);
        tensor.rank = readU16LE(data + 6);
        for (std::size_t dimension = 0; dimension < tensor.dimensions.size(); ++dimension)
            tensor.dimensions[dimension] = readU32LE(data + 8 + dimension * 4);
        tensor.elementCount = readU64LE(data + 24);
        tensor.offset = readU64LE(data + 32);
        tensor.bytes = readU64LE(data + 40);
        tensor.flags = readU32LE(data + 48);
        tensor.reserved32 = readU32LE(data + 52);
        tensor.reserved64 = readU64LE(data + 56);

        if (tensor.rank == 0 || tensor.rank > 4)
            return fail(error, "BAD_TENSOR_RANK");
        std::uint64_t elementCount = 1;
        for (std::uint16_t dimension = 0; dimension < tensor.rank; ++dimension) {
            if (tensor.dimensions[dimension] == 0)
                return fail(error, "BAD_TENSOR_DIMENSION");
            std::uint64_t nextCount = 0;
            if (!checkedMultiply(elementCount, tensor.dimensions[dimension], nextCount))
                return fail(error, "DIMENSION_PRODUCT_OVERFLOW");
            elementCount = nextCount;
        }
        for (std::uint16_t dimension = tensor.rank; dimension < 4; ++dimension) {
            if (tensor.dimensions[dimension] != 0)
                return fail(error, "BAD_TENSOR_DIMENSION");
        }

        const TensorSpec& required = REQUIRED_TENSORS[entryIndex];
        if (tensor.id != required.id)
            return fail(error, "TENSOR_ID_MISMATCH");
        if (tensor.elementType != required.elementType)
            return fail(error, "TENSOR_TYPE_MISMATCH");
        if (tensor.rank != required.rank || tensor.dimensions != required.dimensions)
            return fail(error, "TENSOR_SHAPE_MISMATCH");
        if (tensor.elementCount != elementCount)
            return fail(error, "ELEMENT_COUNT_MISMATCH");

        std::uint64_t computedBytes = 0;
        if (!checkedMultiply(elementCount, elementSize(tensor.elementType), computedBytes))
            return fail(error, "TENSOR_BYTE_OVERFLOW");
        if (tensor.bytes != computedBytes || tensor.bytes != required.bytes)
            return fail(error, "TENSOR_BYTE_LENGTH_MISMATCH");
        if (tensor.offset != nextOffset || tensor.offset != required.offset)
            return fail(error, "TENSOR_OFFSET_MISMATCH");
        std::uint64_t tensorEnd = 0;
        if (!checkedAdd(tensor.offset, tensor.bytes, tensorEnd))
            return fail(error, "TENSOR_OFFSET_OVERFLOW");
        if (tensorEnd > header.payloadBytes)
            return fail(error, "TENSOR_OUT_OF_RANGE");
        nextOffset = tensorEnd;
        if (tensor.flags != 0 || tensor.reserved32 != 0 || tensor.reserved64 != 0)
            return fail(error, "NONZERO_TENSOR_RESERVED");
    }

    if (nextOffset != header.payloadBytes)
        return fail(error, "PAYLOAD_SIZE_MISMATCH");
    return true;
}

[[nodiscard]] bool validateAndDecode(
    std::string_view requestedPath,
    Candidate& candidate,
    std::string& error) {
    const std::filesystem::path path = resolvePath(requestedPath, error);
    if (!error.empty())
        return false;

    std::error_code sizeError;
    const std::uintmax_t fileSize = std::filesystem::file_size(path, sizeError);
    if (sizeError)
        return fail(error, "OPEN_FAILED", path.u8string());
    if (fileSize < HEADER_BYTES)
        return fail(error, "TRUNCATED_HEADER");

    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        return fail(error, "OPEN_FAILED", path.u8string());

    std::array<unsigned char, HEADER_BYTES> rawHeader{};
    if (!readExact(stream, rawHeader.data(), rawHeader.size()))
        return fail(error, "TRUNCATED_HEADER");
    Header header;
    if (!parseHeader(rawHeader, header, error))
        return false;

    std::uint64_t headerAndDirectory = 0;
    std::uint64_t expectedFileBytes = 0;
    if (!checkedAdd(header.headerBytes, header.directoryBytes, headerAndDirectory)
        || !checkedAdd(headerAndDirectory, header.payloadBytes, expectedFileBytes)) {
        return fail(error, "FILE_SIZE_OVERFLOW");
    }
    if (fileSize < headerAndDirectory)
        return fail(error, "TRUNCATED_DIRECTORY");
    if (fileSize < expectedFileBytes)
        return fail(error, "TRUNCATED_PAYLOAD");
    if (fileSize > expectedFileBytes)
        return fail(error, "TRAILING_BYTES");

    std::array<unsigned char, DIRECTORY_BYTES> rawDirectory{};
    if (!readExact(stream, rawDirectory.data(), rawDirectory.size()))
        return fail(error, "TRUNCATED_DIRECTORY");
    if (!parseDirectory(rawDirectory, header, error))
        return false;

    std::vector<unsigned char> payload(static_cast<std::size_t>(header.payloadBytes));
    if (!readExact(stream, payload.data(), payload.size()))
        return fail(error, "TRUNCATED_PAYLOAD");
    if (stream.peek() != std::ifstream::traits_type::eof())
        return fail(error, "TRAILING_BYTES");

    const std::array<unsigned char, 32> digest = sha256(payload);
    if (digest != header.payloadSha256)
        return fail(error, "CHECKSUM_MISMATCH");

    candidate.ftWeights.resize(static_cast<std::size_t>(FT_WEIGHT_COUNT));
    std::size_t cursor = 0;
    for (std::size_t i = 0; i < candidate.ftWeights.size(); ++i, cursor += 2)
        candidate.ftWeights[i] = readI16LE(payload.data() + cursor);
    for (std::size_t i = 0; i < candidate.ftBias.size(); ++i, cursor += 4)
        candidate.ftBias[i] = readI32LE(payload.data() + cursor);
    for (std::size_t i = 0; i < candidate.outputWeights.size(); ++i, cursor += 2)
        candidate.outputWeights[i] = readI16LE(payload.data() + cursor);
    candidate.outputBias = readI32LE(payload.data() + cursor);
    cursor += 4;
    if (cursor != payload.size())
        return fail(error, "PAYLOAD_DECODE_MISMATCH");

    std::error_code absoluteError;
    const std::filesystem::path absolutePath = std::filesystem::absolute(path, absoluteError);
    candidate.sourcePath = absoluteError ? path.u8string() : absolutePath.u8string();
    candidate.payloadDigest = hexDigest(digest);
    return true;
}

[[nodiscard]] int normalizedSquare(
    Square square, Color perspective, Square perspectiveKingSquare) noexcept {
    const int verticalKing = perspective == Color::White
        ? index(perspectiveKingSquare)
        : index(perspectiveKingSquare) ^ 56;
    const int mirror = (verticalKing & 7) >= 4 ? 7 : 0;
    const int verticalSquare = perspective == Color::White
        ? index(square) : index(square) ^ 56;
    return verticalSquare ^ mirror;
}

using WideAccumulator = std::array<std::int64_t, HIDDEN>;

[[nodiscard]] bool scratchPerspective(
    const Position& position,
    const Network::Parameters& parameters,
    Color perspective,
    WideAccumulator& output) noexcept {
    const Square kingSquare = position.kingSquare(perspective);
    if (!isValid(kingSquare)
        || parameters.ftWeights.size()
            != static_cast<std::size_t>(INPUT_FEATURES) * HIDDEN) {
        return false;
    }

    for (std::size_t hidden = 0; hidden < output.size(); ++hidden)
        output[hidden] = parameters.ftBias[hidden];

    for (int colorIndex = 0; colorIndex < COLOR_NB; ++colorIndex) {
        const Color color = static_cast<Color>(colorIndex);
        for (PieceType type = PieceType::Pawn;
             type <= PieceType::King;
             type = static_cast<PieceType>(index(type) + 1)) {
            Bitboard pieces = position.pieces(color, type);
            while (pieces != EMPTY_BB) {
                const Square square = popLsb(pieces);
                const int feature = canonicalFeatureIndex(
                    square, color, type, perspective, kingSquare);
                if (feature < 0 || feature >= INPUT_FEATURES)
                    return false;
                const std::size_t offset =
                    static_cast<std::size_t>(feature) * HIDDEN;
                for (std::size_t hidden = 0; hidden < output.size(); ++hidden)
                    output[hidden] += parameters.ftWeights[offset + hidden];
            }
        }
    }
    return true;
}

[[nodiscard]] int phaseContext(const Position& position) noexcept {
    int phase = 0;
    phase += popcount(position.pieces(PieceType::Knight));
    phase += popcount(position.pieces(PieceType::Bishop));
    phase += 2 * popcount(position.pieces(PieceType::Rook));
    phase += 4 * popcount(position.pieces(PieceType::Queen));
    phase = std::clamp(phase, 0, 24);
    return roundAwayFromZero(
        static_cast<std::int64_t>(phase) * QUANTIZATION_SCALE, 24);
}

[[nodiscard]] Value scoreAccumulators(
    const Position& position,
    const Network::Parameters& parameters,
    const WideAccumulator& white,
    const WideAccumulator& black) noexcept {
    std::int64_t raw = parameters.outputBias;
    for (std::size_t hidden = 0; hidden < static_cast<std::size_t>(HIDDEN); ++hidden) {
        const std::int64_t whiteActivation = std::clamp<std::int64_t>(
            white[hidden], 0, QUANTIZATION_SCALE);
        const std::int64_t blackActivation = std::clamp<std::int64_t>(
            black[hidden], 0, QUANTIZATION_SCALE);
        raw += whiteActivation * parameters.outputWeights[hidden];
        raw += blackActivation * parameters.outputWeights[HIDDEN + hidden];
    }

    const std::uint8_t rights = position.castlingRights();
    const int context[CONTEXTS] = {
        position.sideToMove() == Color::White
            ? QUANTIZATION_SCALE : -QUANTIZATION_SCALE,
        phaseContext(position),
        (rights & WhiteKingSide) != 0 ? QUANTIZATION_SCALE : 0,
        (rights & WhiteQueenSide) != 0 ? QUANTIZATION_SCALE : 0,
        (rights & BlackKingSide) != 0 ? QUANTIZATION_SCALE : 0,
        (rights & BlackQueenSide) != 0 ? QUANTIZATION_SCALE : 0
    };
    for (std::size_t lane = 0; lane < static_cast<std::size_t>(CONTEXTS); ++lane) {
        raw += static_cast<std::int64_t>(context[lane])
             * parameters.outputWeights[2 * HIDDEN + lane];
    }

    return std::clamp(
        roundAwayFromZero(raw, QUANTIZATION_SCALE),
        -NETWORK_CP_LIMIT, NETWORK_CP_LIMIT);
}

[[nodiscard]] bool refreshPerspective(
    Position& position,
    const Network::Parameters& parameters,
    Color perspective) noexcept {
    StateInfo* state = position.state();
    if (!state)
        return false;
    WideAccumulator scratch{};
    if (!scratchPerspective(position, parameters, perspective, scratch)) {
        state->accumulator.validMask &= static_cast<std::uint8_t>(
            ~(1U << index(perspective)));
        return false;
    }

    NNUE::Accumulator& stored =
        state->accumulator.values[static_cast<std::size_t>(index(perspective))];
    for (std::size_t hidden = 0; hidden < stored.size(); ++hidden) {
        if (scratch[hidden] < std::numeric_limits<std::int32_t>::min()
            || scratch[hidden] > std::numeric_limits<std::int32_t>::max()) {
            state->accumulator.validMask &= static_cast<std::uint8_t>(
                ~(1U << index(perspective)));
            return false;
        }
        stored[hidden] = static_cast<std::int32_t>(scratch[hidden]);
    }
    state->accumulator.validMask |= static_cast<std::uint8_t>(
        1U << index(perspective));
    state->accumulator.generation = parameters.generation;
    return true;
}

[[nodiscard]] bool ensureAccumulators(
    Position& position, const Network::Parameters& parameters) noexcept {
    StateInfo* state = position.state();
    if (!state)
        return false;
    if (state->accumulator.generation != parameters.generation) {
        state->accumulator.validMask = 0;
        state->accumulator.generation = parameters.generation;
    }
    for (int perspectiveIndex = 0; perspectiveIndex < COLOR_NB; ++perspectiveIndex) {
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << perspectiveIndex);
        if ((state->accumulator.validMask & mask) == 0
            && !refreshPerspective(position, parameters,
                                   static_cast<Color>(perspectiveIndex))) {
            return false;
        }
    }
    return (state->accumulator.validMask & 3U) == 3U;
}

void widenStored(
    const NNUE::Accumulator& stored, WideAccumulator& wide) noexcept {
    for (std::size_t hidden = 0; hidden < stored.size(); ++hidden)
        wide[hidden] = stored[hidden];
}

[[nodiscard]] bool applyFeatureDelta(
    NNUE::Accumulator& accumulator,
    const Network::Parameters& parameters,
    Color perspective,
    Square perspectiveKing,
    Piece piece,
    Square square,
    int sign) noexcept {
    if (!isValid(piece) || !isValid(square) || !isValid(perspectiveKing))
        return false;
    const int feature = canonicalFeatureIndex(
        square, colorOf(piece), typeOf(piece), perspective, perspectiveKing);
    if (feature < 0 || feature >= INPUT_FEATURES)
        return false;
    const std::size_t offset = static_cast<std::size_t>(feature) * HIDDEN;
    for (std::size_t hidden = 0; hidden < accumulator.size(); ++hidden) {
        const std::int64_t value = static_cast<std::int64_t>(accumulator[hidden])
            + static_cast<std::int64_t>(sign)
            * parameters.ftWeights[offset + hidden];
        if (value < std::numeric_limits<std::int32_t>::min()
            || value > std::numeric_limits<std::int32_t>::max()) {
            return false;
        }
        accumulator[hidden] = static_cast<std::int32_t>(value);
    }
    return true;
}

[[nodiscard]] Square castleRookFrom(Move move) noexcept {
    return static_cast<Square>(index(move.to()) > index(move.from())
        ? index(move.to()) + 1 : index(move.to()) - 2);
}

[[nodiscard]] Square castleRookTo(Move move) noexcept {
    return static_cast<Square>(index(move.to()) > index(move.from())
        ? index(move.to()) - 1 : index(move.to()) + 1);
}

} // namespace

int roundAwayFromZero(
    std::int64_t numerator, std::int64_t positiveDenominator) noexcept {
    if (positiveDenominator <= 0)
        return 0;
    std::int64_t quotient = numerator / positiveDenominator;
    const std::int64_t remainder = numerator % positiveDenominator;
    const std::uint64_t magnitude = remainder < 0
        ? static_cast<std::uint64_t>(-(remainder + 1)) + 1U
        : static_cast<std::uint64_t>(remainder);
    const std::uint64_t threshold =
        static_cast<std::uint64_t>(positiveDenominator / 2)
        + static_cast<std::uint64_t>(positiveDenominator & 1);
    if (magnitude >= threshold && remainder != 0)
        quotient += numerator < 0 ? -1 : 1;
    if (quotient > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    if (quotient < std::numeric_limits<int>::min())
        return std::numeric_limits<int>::min();
    return static_cast<int>(quotient);
}

int canonicalFeatureIndex(
    Square pieceSquare,
    Color pieceColor,
    PieceType pieceType,
    Color perspective,
    Square perspectiveKingSquare) noexcept {
    if (!isValid(pieceSquare) || !isValid(pieceColor) || !isValid(pieceType)
        || !isValid(perspective) || !isValid(perspectiveKingSquare)) {
        return -1;
    }
    const int normalizedKing = normalizedSquare(
        perspectiveKingSquare, perspective, perspectiveKingSquare);
    const int bucket = (normalizedKing >> 3) * 4 + (normalizedKing & 7);
    const int relativeColor = index(pieceColor) ^ index(perspective);
    const int plane = relativeColor * PIECE_TYPE_NB + pieceTypeIndex(pieceType);
    const int normalizedPiece = normalizedSquare(
        pieceSquare, perspective, perspectiveKingSquare);
    return ((bucket * 12 + plane) * SQUARE_NB) + normalizedPiece;
}

Network::Network() noexcept = default;
Network::~Network() = default;

std::shared_ptr<const Network::Parameters> Network::snapshot() const noexcept {
    return std::atomic_load_explicit(&active_, std::memory_order_acquire);
}

LoadResult Network::load(std::string_view path) noexcept {
    LoadResult result;
    const char* unexpectedError = nullptr;
    try {
        std::lock_guard<std::mutex> lock(loadMutex_);
        Candidate candidate;
        std::string error;
        if (!validateAndDecode(path, candidate, error)) {
            lastError_ = error;
            const std::shared_ptr<const Parameters> previous = snapshot();
            if (previous) {
                result.message = "NNUE load failed: " + error
                    + "; retaining previous network";
                result.path = previous->sourcePath;
                result.generation = previous->generation;
            } else {
                result.message = "NNUE load failed: " + error
                    + "; classical fallback active";
            }
            return result;
        }

        std::uint64_t nextGeneration = lastGeneration_ + 1;
        if (nextGeneration == 0)
            nextGeneration = 1;
        auto published = std::make_shared<Parameters>();
        published->ftWeights = std::move(candidate.ftWeights);
        published->ftBias = candidate.ftBias;
        published->outputWeights = candidate.outputWeights;
        published->outputBias = candidate.outputBias;
        published->sourcePath = std::move(candidate.sourcePath);
        published->payloadDigest = std::move(candidate.payloadDigest);
        published->generation = nextGeneration;

        std::shared_ptr<const Parameters> immutable = std::move(published);
        LoadResult success;
        success.success = true;
        success.path = immutable->sourcePath;
        success.generation = immutable->generation;
        success.message = "NNUE loaded: " + immutable->sourcePath
            + " generation=" + std::to_string(immutable->generation);

        // Publication is deliberately the final operation that can affect the
        // transaction.  Every allocation needed by Parameters and LoadResult
        // has completed, and the remaining shared_ptr move/store, integer
        // assignment, string clear, and LoadResult move are non-throwing.
        lastError_.clear();
        std::atomic_store_explicit(&active_, immutable, std::memory_order_release);
        lastGeneration_ = nextGeneration;
        return success;
    } catch (const std::bad_alloc&) {
        unexpectedError = "OUT_OF_MEMORY";
    } catch (...) {
        unexpectedError = "LOAD_EXCEPTION";
    }

    // No publication can precede this path.  Error reporting itself is kept
    // best-effort and nested-noexcept-safe so an actual allocation failure
    // cannot escape this noexcept API or terminate the engine.
    result.success = false;
    result.path.clear();
    result.message.clear();
    const std::shared_ptr<const Parameters> previous = snapshot();
    result.generation = previous ? previous->generation : 0;
    try {
        std::lock_guard<std::mutex> lock(loadMutex_);
        lastError_ = unexpectedError ? unexpectedError : "LOAD_EXCEPTION";
        if (previous) {
            result.path = previous->sourcePath;
            result.message = "NNUE load failed: " + lastError_
                + "; retaining previous network";
        } else {
            result.message = "NNUE load failed: " + lastError_
                + "; classical fallback active";
        }
    } catch (...) {
        result.path.clear();
        result.message.clear();
        try {
            result.message = unexpectedError ? unexpectedError : "LOAD_EXCEPTION";
        } catch (...) {
            // A default/empty message is still a safe failed result.  The
            // active network and generation remain transactionally unchanged.
        }
    }
    return result;
}

void Network::unload() noexcept {
    std::lock_guard<std::mutex> lock(loadMutex_);
    std::shared_ptr<const Parameters> empty;
    std::atomic_store_explicit(&active_, empty, std::memory_order_release);
    lastError_.clear();
}

bool Network::ready() const noexcept {
    return static_cast<bool>(snapshot());
}

std::uint64_t Network::generation() const noexcept {
    const std::shared_ptr<const Parameters> parameters = snapshot();
    return parameters ? parameters->generation : 0;
}

std::string Network::loadedPath() const {
    const std::shared_ptr<const Parameters> parameters = snapshot();
    return parameters ? parameters->sourcePath : std::string{};
}

std::string Network::payloadSha256() const {
    const std::shared_ptr<const Parameters> parameters = snapshot();
    return parameters ? parameters->payloadDigest : std::string{};
}

std::string Network::lastError() const {
    std::lock_guard<std::mutex> lock(loadMutex_);
    return lastError_;
}

bool Network::inferWhiteScratch(
    const Position& position, Value& output) const noexcept {
    output = 0;
    const std::shared_ptr<const Parameters> parameters = snapshot();
    if (!parameters || !isValid(position.kingSquare(Color::White))
        || !isValid(position.kingSquare(Color::Black))) {
        return false;
    }
    WideAccumulator white{};
    WideAccumulator black{};
    if (!scratchPerspective(position, *parameters, Color::White, white)
        || !scratchPerspective(position, *parameters, Color::Black, black)) {
        return false;
    }
    output = scoreAccumulators(position, *parameters, white, black);
    return true;
}

bool Network::inferWhite(Position& position, Value& output) const noexcept {
    output = 0;
    const std::shared_ptr<const Parameters> parameters = snapshot();
    if (!parameters || !isValid(position.kingSquare(Color::White))
        || !isValid(position.kingSquare(Color::Black))) {
        return false;
    }

    if (ensureAccumulators(position, *parameters)) {
        const StateInfo* state = position.state();
        WideAccumulator white{};
        WideAccumulator black{};
        widenStored(state->accumulator.values[index(Color::White)], white);
        widenStored(state->accumulator.values[index(Color::Black)], black);
        output = scoreAccumulators(position, *parameters, white, black);
        return true;
    }

    WideAccumulator white{};
    WideAccumulator black{};
    if (!scratchPerspective(position, *parameters, Color::White, white)
        || !scratchPerspective(position, *parameters, Color::Black, black)) {
        return false;
    }
    output = scoreAccumulators(position, *parameters, white, black);
    return true;
}

bool Network::inferSideToMove(Position& position, Value& output) const noexcept {
    Value white = 0;
    if (!inferWhite(position, white)) {
        output = 0;
        return false;
    }
    output = position.sideToMove() == Color::White ? white : -white;
    return true;
}

bool Network::updateAccumulatorAfterMove(Position& position) const noexcept {
    const std::shared_ptr<const Parameters> parameters = snapshot();
    StateInfo* current = position.state();
    if (!parameters || !current || !current->previous)
        return false;

    const StateInfo* parent = current->previous;
    current->accumulator = parent->accumulator;
    if (current->nullMove) {
        if (current->accumulator.generation != parameters->generation)
            current->accumulator.validMask = 0;
        current->accumulator.generation = parameters->generation;
        return (current->accumulator.validMask & 3U) == 3U;
    }

    if (parent->accumulator.generation != parameters->generation) {
        current->accumulator.validMask = 0;
        current->accumulator.generation = parameters->generation;
        return false;
    }

    const Move move = current->move;
    const Piece movedPiece = current->movedPiece;
    const Color mover = colorOf(movedPiece);
    if (move.isNone() || !isValid(movedPiece) || !isValid(mover)) {
        current->accumulator.validMask = 0;
        current->accumulator.generation = parameters->generation;
        return false;
    }

    for (int perspectiveIndex = 0;
         perspectiveIndex < COLOR_NB;
         ++perspectiveIndex) {
        const Color perspective = static_cast<Color>(perspectiveIndex);
        const std::uint8_t mask = static_cast<std::uint8_t>(1U << perspectiveIndex);
        if ((parent->accumulator.validMask & mask) == 0) {
            current->accumulator.validMask &= static_cast<std::uint8_t>(~mask);
            continue;
        }

        if (typeOf(movedPiece) == PieceType::King && perspective == mover) {
            current->accumulator.validMask &= static_cast<std::uint8_t>(~mask);
            if (!refreshPerspective(position, *parameters, perspective))
                current->accumulator.validMask &= static_cast<std::uint8_t>(~mask);
            continue;
        }

        const Square perspectiveKing = position.kingSquare(perspective);
        NNUE::Accumulator& accumulator =
            current->accumulator.values[static_cast<std::size_t>(perspectiveIndex)];
        bool success = applyFeatureDelta(
            accumulator, *parameters, perspective, perspectiveKing,
            movedPiece, move.from(), -1);
        const Piece placedPiece = move.isPromotion()
            ? makePiece(mover, move.promotionType()) : movedPiece;
        success = success && applyFeatureDelta(
            accumulator, *parameters, perspective, perspectiveKing,
            placedPiece, move.to(), +1);

        if (isValid(current->capturedPiece)) {
            success = success && applyFeatureDelta(
                accumulator, *parameters, perspective, perspectiveKing,
                current->capturedPiece, current->capturedSquare, -1);
        }
        if (move.isCastle()) {
            const Piece rook = makePiece(mover, PieceType::Rook);
            success = success && applyFeatureDelta(
                accumulator, *parameters, perspective, perspectiveKing,
                rook, castleRookFrom(move), -1);
            success = success && applyFeatureDelta(
                accumulator, *parameters, perspective, perspectiveKing,
                rook, castleRookTo(move), +1);
        }
        if (!success)
            current->accumulator.validMask &= static_cast<std::uint8_t>(~mask);
    }

    current->accumulator.generation = parameters->generation;
    return (current->accumulator.validMask & 3U) == 3U;
}

AccumulatorCheck Network::verifyAccumulator(Position& position) const noexcept {
    AccumulatorCheck result;
    const std::shared_ptr<const Parameters> parameters = snapshot();
    if (!parameters || !position.state()
        || !isValid(position.kingSquare(Color::White))
        || !isValid(position.kingSquare(Color::Black))) {
        return result;
    }
    result.available = true;

    // Verification is diagnostic, not a recovery entry point.  Refreshing an
    // invalid cache here would make a missed/failed incremental update appear
    // exact.  Callers prime a root through inference, then every child must
    // arrive with the matching generation and both perspective bits valid.
    const StateInfo* state = position.state();
    if (state->accumulator.generation != parameters->generation
        || (state->accumulator.validMask & 3U) != 3U) {
        return result;
    }

    WideAccumulator scratch[COLOR_NB]{};
    if (!scratchPerspective(position, *parameters, Color::White,
                            scratch[index(Color::White)])
        || !scratchPerspective(position, *parameters, Color::Black,
                               scratch[index(Color::Black)])) {
        return result;
    }

    for (int perspective = 0; perspective < COLOR_NB; ++perspective) {
        for (std::size_t hidden = 0; hidden < static_cast<std::size_t>(HIDDEN); ++hidden) {
            const std::int64_t incremental =
                state->accumulator.values[static_cast<std::size_t>(perspective)]
                                         [static_cast<std::size_t>(hidden)];
            const std::int64_t expected =
                scratch[perspective][static_cast<std::size_t>(hidden)];
            if (incremental != expected) {
                result.mismatchPerspective = perspective;
                result.mismatchNeuron = static_cast<int>(hidden);
                result.incrementalValue = incremental;
                result.scratchValue = expected;
                return result;
            }
        }
    }

    WideAccumulator incremental[COLOR_NB]{};
    widenStored(state->accumulator.values[index(Color::White)],
                incremental[index(Color::White)]);
    widenStored(state->accumulator.values[index(Color::Black)],
                incremental[index(Color::Black)]);
    result.incrementalWhite = scoreAccumulators(
        position, *parameters,
        incremental[index(Color::White)], incremental[index(Color::Black)]);
    result.scratchWhite = scoreAccumulators(
        position, *parameters,
        scratch[index(Color::White)], scratch[index(Color::Black)]);
    result.incrementalSideToMove = position.sideToMove() == Color::White
        ? result.incrementalWhite : -result.incrementalWhite;
    result.scratchSideToMove = position.sideToMove() == Color::White
        ? result.scratchWhite : -result.scratchWhite;
    result.exact = result.incrementalWhite == result.scratchWhite;
    return result;
}

} // namespace Eunshin::NNUE
