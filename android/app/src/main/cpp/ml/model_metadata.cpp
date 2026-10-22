#include "ml/model_metadata.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <vector>

#include "io/image_loader.h"

namespace imrelighter::ml {
namespace {

class Cursor {
public:
    Cursor(const uint8_t* data, size_t begin, size_t end)
        : data_(data), position_(begin), end_(end) {}

    bool atEnd() const { return position_ >= end_; }
    size_t position() const { return position_; }
    size_t end() const { return end_; }

    bool varint(uint64_t& value) {
        value = 0;
        for (int shift = 0; shift < 64; shift += 7) {
            if (position_ >= end_) return false;
            const uint8_t byte = data_[position_++];
            value |= static_cast<uint64_t>(byte & 0x7f) << shift;
            if ((byte & 0x80) == 0) return true;
        }
        return false;
    }

    bool skip(uint64_t length) {
        if (length > end_ - position_) return false;
        position_ += static_cast<size_t>(length);
        return true;
    }

    bool skipField(uint32_t wire) {
        switch (wire) {
            case 0: {
                uint64_t ignored = 0;
                return varint(ignored);
            }
            case 1:
                return skip(8);
            case 2: {
                uint64_t length = 0;
                return varint(length) && skip(length);
            }
            case 5:
                return skip(4);
            default:
                return false;
        }
    }

private:
    const uint8_t* data_;
    size_t position_;
    size_t end_;
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
        if (!cursor.skipField(wire)) return false;
    }
    return true;
}

bool parseGraph(const uint8_t* data, size_t begin, size_t end, WeightInfo& info) {
    Cursor cursor(data, begin, end);
    while (!cursor.atEnd()) {
        uint64_t key = 0;
        if (!cursor.varint(key)) return false;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (field == 5 && wire == 2) {
            uint64_t length = 0;
            if (!cursor.varint(length) || length > cursor.end() - cursor.position()) {
                return false;
            }
            Cursor tensor(data, cursor.position(),
                          cursor.position() + static_cast<size_t>(length));
            if (!parseTensor(tensor, info)) return false;
            if (!cursor.skip(length)) return false;
            continue;
        }
        if (!cursor.skipField(wire)) return false;
    }
    return true;
}

bool readModelBytes(AAssetManager* assets, const std::string& path,
                    std::vector<uint8_t>& bytes) {
    if (!path.empty() && path.front() == '/') {
        std::ifstream stream(path, std::ios::binary);
        if (!stream) return false;
        bytes.assign(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
        return !bytes.empty();
    }
    std::string error;
    return io::readAsset(assets, path, bytes, error) && !bytes.empty();
}

}  // namespace

std::string detectModelType(AAssetManager* assets, const std::string& path) {
    // Match desktop priority: an explicit exported variant name is
    // authoritative; the initializer scan is the fallback for renamed models.
    std::string filename = path;
    const size_t slash = filename.find_last_of("/\\");
    if (slash != std::string::npos) {
        filename.erase(0, slash + 1);
    }
    for (char& c : filename) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (filename.find("q4f16") != std::string::npos) return "q4f16";
    if (filename.find("bnb4") != std::string::npos) return "bnb4";
    if (filename.find("int8") != std::string::npos) return "int8";
    if (filename.find("q4") != std::string::npos) return "q4";

    std::vector<uint8_t> bytes;
    if (!readModelBytes(assets, path, bytes)) {
        return "unknown";
    }

    WeightInfo weights;
    bool parsed = false;
    Cursor cursor(bytes.data(), 0, bytes.size());
    while (!cursor.atEnd()) {
        uint64_t key = 0;
        if (!cursor.varint(key)) break;
        const uint32_t field = static_cast<uint32_t>(key >> 3);
        const uint32_t wire = static_cast<uint32_t>(key & 7);
        if (field == 7 && wire == 2) {
            uint64_t length = 0;
            if (!cursor.varint(length) ||
                length > cursor.end() - cursor.position()) {
                break;
            }
            parsed = parseGraph(bytes.data(), cursor.position(),
                                cursor.position() + static_cast<size_t>(length),
                                weights);
            break;
        }
        if (!cursor.skipField(wire)) break;
    }
    if (!parsed) {
        return "unknown";
    }

    if (weights.int8 > 0 || weights.uint8 > 0) return "int8";
    if (weights.int4 > 0 || weights.uint4 > 0) {
        return weights.fp16 > 0 ? "q4f16" : "q4";
    }
    if (weights.fp16 > weights.fp32) return "fp16";
    if (weights.bf16 > 0) return "bf16";
    if (weights.fp32 > 0) return "fp32";
    return "unknown";
}

}  // namespace imrelighter::ml
