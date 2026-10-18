#include "io/hash.h"

#include <windows.h>
#include <bcrypt.h>

#include <array>
#include <fstream>
#include <vector>

namespace io {
namespace {

std::string hex(const std::array<UCHAR, 32>& bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (UCHAR byte : bytes) {
        result.push_back(kDigits[byte >> 4]);
        result.push_back(kDigits[byte & 0x0f]);
    }
    return result;
}

bool appendFile(BCRYPT_HASH_HANDLE hash, const std::filesystem::path& path,
                std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        if (error) *error = "Cannot open model file.";
        return false;
    }
    std::array<char, 1 << 20> buffer{};
    while (stream) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = stream.gcount();
        if (count > 0 &&
            !BCRYPT_SUCCESS(BCryptHashData(
                hash, reinterpret_cast<PUCHAR>(buffer.data()),
                static_cast<ULONG>(count), 0))) {
            if (error) *error = "Cannot hash model file.";
            return false;
        }
    }
    return true;
}

} // namespace

std::string sha256Model(const std::filesystem::path& modelPath,
                        std::string* error) {
    if (modelPath.empty() || !std::filesystem::is_regular_file(modelPath)) {
        if (error) *error = "Model file is missing.";
        return {};
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<UCHAR, 32> digest{};
    DWORD objectLength = 0;
    DWORD digestLength = 0;
    DWORD result = 0;

    if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0))) {
        if (error) *error = "Cannot open SHA-256 provider.";
        return {};
    }
    if (!BCRYPT_SUCCESS(BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength), sizeof(objectLength),
            &result, 0)) ||
        !BCRYPT_SUCCESS(BCryptGetProperty(
            algorithm, BCRYPT_HASH_LENGTH,
            reinterpret_cast<PUCHAR>(&digestLength), sizeof(digestLength),
            &result, 0))) {
        if (error) *error = "Cannot query SHA-256 provider.";
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    std::vector<UCHAR> object(objectLength);
    if (!BCRYPT_SUCCESS(BCryptCreateHash(
            algorithm, &hash, object.data(), objectLength, nullptr, 0, 0))) {
        if (error) *error = "Cannot create SHA-256 hash.";
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    auto external = modelPath;
    external += L"_data";
    const bool ok = appendFile(hash, modelPath, error) &&
                    (!std::filesystem::is_regular_file(external) ||
                     appendFile(hash, external, error)) &&
                    BCRYPT_SUCCESS(BCryptFinishHash(
                        hash, digest.data(), digestLength, 0));
    if (!ok && error && error->empty()) *error = "Cannot finish SHA-256 hash.";

    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    return ok ? hex(digest) : std::string();
}

} // namespace io
