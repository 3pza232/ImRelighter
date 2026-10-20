#include "ml/model_metadata.h"

#include <cstdint>
#include <fstream>

namespace ml {
namespace {

class Cursor {
public:
    Cursor(std::ifstream& stream, uint64_t begin, uint64_t end)
        : stream_(stream), position_(begin), end_(end) {
        stream_.seekg(static_cast<std::streamoff>(begin));
    }

    bool atEnd() const { return position_ >= end_; }
    uint64_t position() const { return position_; }

    bool varint(uint64_t& value) {
        value = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (position_ >= end_) return false;
            const int byte = stream_.get();
            if (byte == EOF) return false;
            ++position_;
            value |= static_cast<uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) return true;
        }
        return false;
    }

    bool skip(uint64_t length) {
        if (length > end_ - position_) return false;
        position_ += length;
        stream_.seekg(static_cast<std::streamoff>(position_));
        return true;
    }

    bool skipField(uint32_t wireType, uint64_t& length) {
        switch (wireType) {
            case 0: {
                uint64_t ignored = 0;
                return varint(ignored);
            }
            case 1:
                return skip(8);
            case 2:
                return varint(length) && skip(length);
            case 5:
                return skip(4);
            default:
                return false;
        }
    }

    uint64_t end() const { return end_; }

private:
    std::ifstream& stream_;
    uint64_t position_;
    uint64_t end_;
};

bool parseTensor(Cursor& cursor, WeightInfo& info) {
    while (!cursor.atEnd()) {
        uint64_t key = 0;
        if (!cursor.varint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (field == 2 && wire == 0) {
            uint64_t type = 0;
            if (!cursor.varint(type)) return false;
            if (type == 10) ++info.fp16;
            else if (type == 1) ++info.fp32;
            else if (type == 16) ++info.bf16;
            else if (type == 3) ++info.int8;
            else if (type == 2) ++info.uint8;
            else if (type == 22) ++info.int4;
            else if (type == 21) ++info.uint4;
            else ++info.other;
            continue;
        }
        uint64_t length = 0;
        if (!cursor.skipField(wire, length)) return false;
    }
    return true;
}

bool parseGraph(const std::filesystem::path& path, uint64_t begin, uint64_t end,
                WeightInfo& info) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    Cursor cursor(stream, begin, end);
    while (!cursor.atEnd()) {
        uint64_t key = 0;
        if (!cursor.varint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (field == 5 && wire == 2) {
            uint64_t length = 0;
            if (!cursor.varint(length) || length > cursor.end() - cursor.position())
                return false;
            const uint64_t tensorEnd = cursor.position() + length;
            Cursor tensor(stream, cursor.position(), tensorEnd);
            if (!parseTensor(tensor, info)) return false;
            if (!cursor.skip(length)) return false;
            continue;
        }
        uint64_t length = 0;
        if (!cursor.skipField(wire, length)) return false;
    }
    return true;
}

} // namespace

bool inspectModelWeights(const std::filesystem::path& modelPath, WeightInfo& info,
                         std::string* error) {
    info = {};
    std::error_code ec;
    const auto size = std::filesystem::file_size(modelPath, ec);
    if (ec || size == 0) {
        if (error) *error = "Cannot read model file.";
        return false;
    }
    std::ifstream stream(modelPath, std::ios::binary);
    if (!stream) {
        if (error) *error = "Cannot open model file.";
        return false;
    }
    Cursor cursor(stream, 0, size);
    while (!cursor.atEnd()) {
        uint64_t key = 0;
        if (!cursor.varint(key)) break;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (field == 7 && wire == 2) {
            uint64_t length = 0;
            if (!cursor.varint(length) ||
                length > cursor.end() - cursor.position()) {
                if (error) *error = "Invalid ONNX graph.";
                return false;
            }
            return parseGraph(modelPath, cursor.position(),
                              cursor.position() + length, info);
        }
        uint64_t length = 0;
        if (!cursor.skipField(wire, length)) {
            if (error) *error = "Invalid ONNX protobuf.";
            return false;
        }
    }
    if (error) *error = "ONNX graph not found.";
    return false;
}

} // namespace ml
