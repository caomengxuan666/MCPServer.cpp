// src/Resources/resource.h
#pragma once

#include "nlohmann/json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace mcp::resources {

    // Resource struct, represents a resource
    struct Resource {
        std::string uri;              // Unique identifier of the resource
        std::string name;             // Human-readable name
        std::string description;      // Optional description
        std::string mimeType;         // Optional MIME type
    };

    // Resource template, used for dynamic resources
    struct ResourceTemplate {
        std::string uriTemplate;      // URI template following RFC 6570
        std::string name;             // Human-readable name for this type
        std::string description;      // Optional description
        std::string mimeType;         // MIME type for all matching resources (optional)
    };

    // Resource content struct
    struct ResourceContent {
        std::string uri;              // URI of the resource
        std::string mimeType;         // Optional MIME type
        std::string text;             // For text resources
        std::string blob;             // For binary resources (base64 encoded)
    };

    // Resource read callback function signature
    using ReadResourceCallback = std::function<void(const std::vector<ResourceContent>&)>;

    // Resource update callback function signature
    using ResourceUpdateCallback = std::function<void(const std::string& uri)>;

    class ResourceManager {
    public:
        ResourceManager() = default;
        
        // Register static resource
        void register_resource(const Resource& resource);
        
        // Register resource template
        void register_resource_template(const ResourceTemplate& resourceTemplate);
        
        // Get all registered resources
        std::vector<Resource> get_resources() const;
        
        // Get all registered resource templates
        std::vector<ResourceTemplate> get_resource_templates() const;
        
        // Read resource content
        std::vector<ResourceContent> read_resource(const std::string& uri) const;
        
        // Subscribe to resource updates
        void subscribe(const std::string& uri, const ResourceUpdateCallback& callback);
        
        // Unsubscribe from resource updates
        void unsubscribe(const std::string& uri);
        
        // Notify resource list change
        void notify_list_changed();
        
        // Notify resource content update
        void notify_resource_updated(const std::string& uri);

    private:
        std::vector<Resource> resources_;
        std::vector<ResourceTemplate> resource_templates_;
        std::unordered_map<std::string, std::vector<ResourceUpdateCallback>> subscriptions_;
    };

} // namespace mcp::resources