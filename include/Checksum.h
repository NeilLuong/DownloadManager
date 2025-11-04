#pragma once
#include <string>
#include <filesystem>

class Checksum {
public:
    // Compute SHA-256 hash of a file
    static std::string compute_sha256(const std::filesystem::path& file_path);
    
    // Verify file against expected SHA-256 hash (case-insensitive)
    static bool verify_sha256(const std::filesystem::path& file_path, 
                             const std::string& expected_hash);
    
    // Convert bytes to hex string
    static std::string bytes_to_hex(const unsigned char* data, size_t length);
    
    // Convert hex string to lowercase for comparison
    static std::string to_lowercase(const std::string& str);
};