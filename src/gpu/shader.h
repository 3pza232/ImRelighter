#pragma once

#include <filesystem>
#include <string>

namespace gpu {

class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool load(const std::filesystem::path& vertexPath,
              const std::filesystem::path& fragmentPath, std::string* error);
    void destroy();
    void use() const;


    void setInt(const char* name, int value) const;
    void setFloat(const char* name, float value) const;
    void setVec2(const char* name, float x, float y) const;
    void setVec3(const char* name, float x, float y, float z) const;
    void setVec3(const char* name, const float* values) const;
    void setFloatArray(const char* name, const float* values, int count) const;
    void setVec3Array(const char* name, const float* values, int count) const;
    void setIntArray(const char* name, const int* values, int count) const;

private:
    unsigned int id_ = 0;
};

} // namespace gpu