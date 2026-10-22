#pragma once

#include <android/asset_manager.h>

#include <string>
#include <vector>

#include "core/types.h"

namespace imrelighter {

class ModelCatalog {
public:
    static constexpr const char* kDefaultId = "q4";

    explicit ModelCatalog(AAssetManager* assets);

    const std::vector<ModelInfo>& models() const { return models_; }
    const ModelInfo* find(const std::string& id) const;
    const ModelInfo& default_model() const;

    bool available(const std::string& id) const;

    void load_custom_model(const std::string& files_dir);
    // Returns the stable model id on success, or an empty string on failure.
    std::string register_custom_model(const std::string& files_dir,
                                      const std::string& absolute_path);
    bool remove_custom_model(const std::string& files_dir, const std::string& id);
    std::string load_selection(const std::string& files_dir) const;
    bool save_selection(const std::string& files_dir, const std::string& id) const;

private:
    AAssetManager* assets_ = nullptr;
    std::vector<ModelInfo> models_;
};

}  // namespace imrelighter
