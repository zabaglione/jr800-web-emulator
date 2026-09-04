// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace jr800::tools {

inline std::filesystem::path resolve_path_identity(
    const std::filesystem::path& path,
    std::error_code& error
) {
    auto resolved = std::filesystem::absolute(path, error);
    if (error) {
        return {};
    }
    for (std::size_t depth = 0; depth < 40U; ++depth) {
        const auto status = std::filesystem::symlink_status(resolved, error);
        if (error) {
            if (error != std::errc::no_such_file_or_directory) {
                return {};
            }
            error.clear();
            return std::filesystem::weakly_canonical(resolved, error);
        }
        if (!std::filesystem::is_symlink(status)) {
            return std::filesystem::weakly_canonical(resolved, error);
        }
        auto target = std::filesystem::read_symlink(resolved, error);
        if (error) {
            return {};
        }
        if (target.is_relative()) {
            target = resolved.parent_path() / target;
        }
        resolved = std::filesystem::absolute(target, error).lexically_normal();
        if (error) {
            return {};
        }
    }
    error = std::make_error_code(std::errc::too_many_symbolic_link_levels);
    return {};
}

inline std::string collision_key(const std::filesystem::path& identity) {
    auto key = identity.generic_string();
    for (auto& character : key) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return key;
}

enum class PathInsertResult {
    inserted,
    duplicate,
    inspection_error,
};

class DistinctPathSet final {
public:
    [[nodiscard]] PathInsertResult insert(const std::filesystem::path& path) {
        std::error_code error;
        const auto absolute = std::filesystem::absolute(path, error);
        if (error) {
            return PathInsertResult::inspection_error;
        }
        const auto identity = resolve_path_identity(absolute, error);
        if (error) {
            return PathInsertResult::inspection_error;
        }
        const auto key = collision_key(identity);
        const auto exists = std::filesystem::exists(absolute, error);
        if (error) {
            return PathInsertResult::inspection_error;
        }

        for (const auto& entry : entries_) {
            if (identity == entry.identity || key == entry.collision_key) {
                return PathInsertResult::duplicate;
            }
            if (exists && entry.exists) {
                const auto equivalent = std::filesystem::equivalent(
                    absolute,
                    entry.absolute,
                    error
                );
                if (error) {
                    return PathInsertResult::inspection_error;
                }
                if (equivalent) {
                    return PathInsertResult::duplicate;
                }
            }
        }
        entries_.push_back(Entry{absolute, identity, key, exists});
        return PathInsertResult::inserted;
    }

private:
    struct Entry {
        std::filesystem::path absolute;
        std::filesystem::path identity;
        std::string collision_key;
        bool exists{};
    };

    std::vector<Entry> entries_;
};

}  // namespace jr800::tools
