// src/Resources/resource.cpp
#include "resource.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace mcp::resources {

    void ResourceManager::register_resource(const Resource &resource) {
        resources_.push_back(resource);
    }

    void ResourceManager::register_resource_template(const ResourceTemplate &resourceTemplate) {
        resource_templates_.push_back(resourceTemplate);
    }

    std::vector<Resource> ResourceManager::get_resources() const {
        return resources_;
    }

    std::vector<ResourceTemplate> ResourceManager::get_resource_templates() const {
        return resource_templates_;
    }

    std::vector<ResourceContent> ResourceManager::read_resource(const std::string &uri) const {
        // todo accroding to the uri to read resources
        std::vector<ResourceContent> contents;

        // find matched resources
        for (const auto &resource: resources_) {
            if (resource.uri == uri) {
                ResourceContent content;
                content.uri = resource.uri;
                content.mimeType = resource.mimeType;

                // handle file resources
                if (uri.substr(0, 7) == "file://") {
                    std::string file_path = uri.substr(7);
                    std::ifstream file(file_path, std::ios::binary);
                    if (file.is_open()) {
                        // read file contents
                        std::ostringstream buffer;
                        buffer << file.rdbuf();
                        std::string file_content = buffer.str();

                        // tell the mime type by looking at the file extension
                        if (resource.mimeType.find("text/") == 0 ||
                            resource.mimeType == "application/json" ||
                            resource.mimeType == "application/xml") {
                            content.text = file_content;
                        } else {
                            //todo handle binary with base64
                            content.blob = "base64_encoded_data";
                        }
                    } else {
                        // cannot read file
                        content.text = "Error: Unable to read file " + file_path;
                    }
                } else {
                    // example content
                    if (resource.mimeType.find("text/") == 0) {
                        content.text = "Sample text content for " + uri;
                    } else {
                        content.blob = "c2FtcGxlIGJpbmFyeSBjb250ZW50";// "sample binary content" base64
                    }
                }

                contents.push_back(content);
                break;
            }
        }

        return contents;
    }

    void ResourceManager::subscribe(const std::string &uri, const ResourceUpdateCallback &callback) {
        subscriptions_[uri].push_back(callback);
    }

    void ResourceManager::unsubscribe(const std::string &uri) {
        subscriptions_.erase(uri);
    }

    void ResourceManager::notify_list_changed() {
        //todo inform that the resource list has changed with notifications/resources/list_changed
    }

    void ResourceManager::notify_resource_updated(const std::string &uri) {
        // inform the subscribers that the resource has been updated
        auto it = subscriptions_.find(uri);
        if (it != subscriptions_.end()) {
            for (const auto &callback: it->second) {
                callback(uri);
            }
        }
    }

}// namespace mcp::resources