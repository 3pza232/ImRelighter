#include "app/document_store.h"

#include <algorithm>

#include "io/image_io.h"
#include "io/paths.h"

namespace app {

std::uint64_t DocumentStore::add(const std::filesystem::path& path,
                                 const Limits& limits, std::string* error) {
    const int taskLimit = std::clamp(limits.maxTasks, 1, 500);
    if (documents_.size() >= static_cast<std::size_t>(taskLimit)) {
        if (error) {
            *error = limits.taskLimitMessage + " (" + std::to_string(taskLimit) +
                     ")";
        }
        return 0;
    }

    auto image = io::loadImage(path, error, limits.maxImagePixels);
    if (!image) return 0;

    std::uint64_t used = 0;
    for (const auto& document : documents_) {
        used += document->color.size() +
                document->depth.capacity() * sizeof(float) +
                document->thumbnail.size();
    }
    const std::uint64_t incoming =
        image->rgba.size() +
        static_cast<std::uint64_t>(image->width) * image->height * sizeof(float) +
        128ull * 1024ull;
    const int memoryGb = std::clamp(limits.memoryGb, 1, 16);
    const std::uint64_t memoryBudget =
        static_cast<std::uint64_t>(memoryGb) * 1024ull * 1024ull * 1024ull;
    if (used + incoming > memoryBudget) {
        if (error) {
            *error = limits.memoryLimitMessage + " (" + std::to_string(memoryGb) +
                     " GB)";
        }
        return 0;
    }

    auto doc = std::make_unique<core::Document>();
    doc->id = nextDocumentId_++;
    doc->path = path;
    doc->name = paths::fileNameUtf8(path);
    doc->width = image->width;
    doc->height = image->height;

    if (!io::makeThumbnail(*image, 160, doc->thumbnail, doc->thumbWidth,
                           doc->thumbHeight)) {
        if (error) *error = limits.thumbnailFailedMessage;
        return 0;
    }

    doc->color = std::move(image->rgba);

    const std::uint64_t id = doc->id;
    auto texture = std::make_unique<gpu::Texture>();
    texture->upload(doc->thumbnail.data(), doc->thumbWidth, doc->thumbHeight);
    thumbnails_[id] = std::move(texture);

    documents_.push_back(std::move(doc));
    return id;
}

bool DocumentStore::activate(std::uint64_t id) {
    const auto it = std::find_if(
        documents_.begin(), documents_.end(),
        [&](const std::unique_ptr<core::Document>& doc) { return doc->id == id; });
    if (it == documents_.end()) return false;

    const int index = static_cast<int>(std::distance(documents_.begin(), it));
    if (index == activeIndex_) return false;

    activeIndex_ = index;
    uploadActiveTexture();
    return true;
}

DocumentStore::CloseResult DocumentStore::close(std::uint64_t id) {
    CloseResult result;
    const auto it = std::find_if(
        documents_.begin(), documents_.end(),
        [&](const std::unique_ptr<core::Document>& doc) { return doc->id == id; });
    if (it == documents_.end()) return result;

    const int index = static_cast<int>(std::distance(documents_.begin(), it));
    const bool wasActive = index == activeIndex_;
    result.closed = true;

    thumbnails_.erase(id);
    documents_.erase(it);

    if (documents_.empty()) {
        activeIndex_ = -1;
        activeTexture_.destroy();
        result.activeChanged = true;
        result.becameEmpty = true;
        return result;
    }

    if (index < activeIndex_) {
        --activeIndex_;
    }

    if (wasActive) {
        activeIndex_ = std::min(index, static_cast<int>(documents_.size()) - 1);
        uploadActiveTexture();
        result.activeChanged = true;
    }
    return result;
}

bool DocumentStore::hasActive() const {
    return activeIndex_ >= 0 && activeIndex_ < static_cast<int>(documents_.size());
}

const core::Document* DocumentStore::document(std::size_t index) const {
    return index < documents_.size() ? documents_[index].get() : nullptr;
}

core::Document* DocumentStore::document(std::size_t index) {
    return index < documents_.size() ? documents_[index].get() : nullptr;
}

const core::Document* DocumentStore::activeDocument() const {
    return hasActive() ? documents_[static_cast<std::size_t>(activeIndex_)].get()
                       : nullptr;
}

core::Document* DocumentStore::activeDocument() {
    return hasActive() ? documents_[static_cast<std::size_t>(activeIndex_)].get()
                       : nullptr;
}

gpu::Texture* DocumentStore::thumbnail(std::uint64_t id) {
    const auto it = thumbnails_.find(id);
    return it == thumbnails_.end() ? nullptr : it->second.get();
}

void DocumentStore::releaseGpuResources() {
    activeTexture_.destroy();
    thumbnails_.clear();
}

void DocumentStore::uploadActiveTexture() {
    const core::Document* doc = activeDocument();
    if (!doc || doc->color.empty()) {
        activeTexture_.destroy();
        return;
    }
    activeTexture_.upload(doc->color.data(), doc->width, doc->height);
}

} // namespace app
