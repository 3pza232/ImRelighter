#include "gpu/shader.h"

#include <glad/glad.h>

#include <fstream>
#include <sstream>
#include <vector>

namespace gpu {
namespace {

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return {};
    std::ostringstream text;
    text << stream.rdbuf();
    return text.str();
}

unsigned int compile(GLenum type, const std::string& source,
                     const std::filesystem::path& path, std::string* error) {
    const unsigned int shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (success == GL_TRUE) return shader;

    int length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::vector<char> log(static_cast<size_t>(length > 1 ? length : 1));
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    if (error) {
        *error = "Shader compile failed: " + path.filename().string() + "\n" +
                 log.data();
    }
    glDeleteShader(shader);
    return 0;
}

} // namespace

Shader::~Shader() {
    destroy();
}

void Shader::destroy() {
    if (id_ != 0) {
        glDeleteProgram(id_);
        id_ = 0;
    }
}

bool Shader::load(const std::filesystem::path& vertexPath,
                  const std::filesystem::path& fragmentPath, std::string* error) {
    const std::string vertexSource = readText(vertexPath);
    const std::string fragmentSource = readText(fragmentPath);
    if (vertexSource.empty() || fragmentSource.empty()) {
        if (error) *error = "Shader source is missing: " + vertexPath.string();
        return false;
    }

    const unsigned int vertex = compile(GL_VERTEX_SHADER, vertexSource,
                                        vertexPath, error);
    if (vertex == 0) return false;
    const unsigned int fragment = compile(GL_FRAGMENT_SHADER, fragmentSource,
                                          fragmentPath, error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        return false;
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (success != GL_TRUE) {
        int length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<size_t>(length > 1 ? length : 1));
        glGetProgramInfoLog(program, length, nullptr, log.data());
        if (error) *error = "Shader link failed:\n" + std::string(log.data());
        glDeleteProgram(program);
        return false;
    }

    destroy();
    id_ = program;
    return true;
}

void Shader::use() const {
    glUseProgram(id_);
}

void Shader::setInt(const char* name, int value) const {
    glUniform1i(glGetUniformLocation(id_, name), value);
}

void Shader::setFloat(const char* name, float value) const {
    glUniform1f(glGetUniformLocation(id_, name), value);
}

void Shader::setVec2(const char* name, float x, float y) const {
    glUniform2f(glGetUniformLocation(id_, name), x, y);
}

void Shader::setVec3(const char* name, float x, float y, float z) const {
    glUniform3f(glGetUniformLocation(id_, name), x, y, z);
}

void Shader::setVec3(const char* name, const float* values) const {
    glUniform3fv(glGetUniformLocation(id_, name), 1, values);
}

void Shader::setFloatArray(const char* name, const float* values, int count) const {
    glUniform1fv(glGetUniformLocation(id_, name), count, values);
}

void Shader::setVec3Array(const char* name, const float* values, int count) const {
    glUniform3fv(glGetUniformLocation(id_, name), count, values);
}

void Shader::setIntArray(const char* name, const int* values, int count) const {
    glUniform1iv(glGetUniformLocation(id_, name), count, values);
}

} // namespace gpu