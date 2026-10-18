#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/document.h"
#include "gpu/texture.h"

namespace app {

// Owns loaded documents and the GPU resources derived from them. Keeping this
// state out of App makes document lifetime rules explicit and testable.
class DocumentStore {
public:
    struct Limits {
        int maxTasks = 8;
        int memoryGb = 4;
        std::string taskLimitMessage;
        std::string memoryLimitMessage;
        std::string thumbnailFailedMessage;
    };

    struct CloseResult {
        bool closed = false;
        bool activeChanged = false;
        bool becameEmpty = false;
    };

    std::uint64_t add(const std::filesystem::path& path, const Limits& limits,
                      std::string* error);
    bool activate(std::uint64_t id);
    CloseResult close(std::uint64_t id);

    bool hasActive() const;
    std::size_t size() const { return documents_.size(); }
    int activeIndex() const { return activeIndex_; }

    const core::Document* document(std::size_t index) const;
    core::Document* document(std::size_t index);
    const core::Document* activeDocument() const;
    core::Document* activeDocument();

    gpu::Texture* thumbnail(std::uint64_t id);
    unsigned int activeTextureId() const { return activeTexture_.id(); }

    void releaseGpuResources();

private:
    void uploadActiveTexture();

    std::vector<std::unique_ptr<core::Document>> documents_;
    int activeIndex_ = -1;
    std::uint64_t nextDocumentId_ = 1;
    gpu::Texture activeTexture_;
    std::unordered_map<std::uint64_t, std::unique_ptr<gpu::Texture>> thumbnails_;
};

} // namespace app
