#pragma once

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>


// ==============================
// Abstract Auth Manager
// ==============================
class AuthManagerBase {
public:
    virtual ~AuthManagerBase() = default;

    virtual bool validate(const std::unordered_map<std::string, std::string> &headers) const = 0;
    // Get the type of the auth manager
    virtual std::string type() const = 0;
};

// ==============================
// X-API-Key Auth
// ==============================
class AuthManagerXApi final : public AuthManagerBase {
private:
    std::unordered_set<std::string> valid_keys_;

public:
    explicit AuthManagerXApi(std::vector<std::string> api_keys) {
        for (auto &key: api_keys) {
            if (!key.empty()) {
                valid_keys_.insert(std::move(key));
            }
        }
    }

    bool validate(const std::unordered_map<std::string, std::string> &headers) const override {
        auto it = headers.find("X-API-Key");
        if (it == headers.end()) {
            return false;
        }
        return valid_keys_.find(it->second) != valid_keys_.end();
    }

    std::string type() const override {
        return "X-API-Key";
    }
};

// ==============================
// Bearer Token Auth
// ==============================
class AuthManagerBearer final : public AuthManagerBase {
private:
    std::unordered_set<std::string> valid_tokens_;

public:
    explicit AuthManagerBearer(std::vector<std::string> tokens) {
        for (auto &token: tokens) {
            if (!token.empty()) {
                valid_tokens_.insert(std::move(token));
            }
        }
    }

    bool validate(const std::unordered_map<std::string, std::string> &headers) const override {
        auto it = headers.find("Authorization");
        if (it == headers.end()) {
            return false;
        }

        const std::string &auth = it->second;
        constexpr std::string_view prefix = "Bearer ";
        if (auth.substr(0, prefix.size()) != prefix) {
            return false;
        }

        std::string token = auth.substr(prefix.size());
        size_t start = token.find_first_not_of(" \t");
        size_t end = token.find_last_not_of(" \t");
        if (start == std::string::npos) {
            return false;
        }
        token = token.substr(start, end - start + 1);

        return valid_tokens_.find(token) != valid_tokens_.end();
    }

    std::string type() const override {
        return "Bearer";
    }
};

class AuthManagerAny final : public AuthManagerBase {
private:
    std::vector<std::shared_ptr<AuthManagerBase>> managers_;

public:
    explicit AuthManagerAny(std::vector<std::shared_ptr<AuthManagerBase>> managers)
        : managers_(std::move(managers)) {}

    bool validate(const std::unordered_map<std::string, std::string> &headers) const override {
        return std::any_of(managers_.begin(), managers_.end(),
                           [&headers](const auto &mgr) {
                               return mgr->validate(headers);
                           });
    }

    std::string type() const override {
        std::string types;
        for (size_t i = 0; i < managers_.size(); ++i) {
            if (i > 0) { types += "/";
}
            types += managers_[i]->type();
        }
        return types;
    }
};