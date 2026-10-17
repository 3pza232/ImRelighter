#pragma once

#include <cstdint>

namespace gpu {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    void upload(const uint8_t* rgba, int width, int height);
    void destroy();

    unsigned int id() const { return id_; }
    int width() const { return width_; }
    int height() const { return height_; }

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

} // namespace gpu
