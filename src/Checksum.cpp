#include "Checksum.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <algorithm>

std::string Checksum::compute_sha256(const std::filesystem::path& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file for checksum: " << file_path << std::endl;
    }

    // Initialize SHA-256 context
    SHA256_CTX sha256_ctx;
    SHA256_Init(&sha256_ctx);

    // Read file in chunks and update hash
    const size_t BUFFER_SIZE = 1024 * 1024;  // 1 MB chunks
    char buffer[BUFFER_SIZE];

    while (file.good()) {
        file.read(buffer, BUFFER_SIZE);
        size_t bytes_read = file.gcount();
        
        if (bytes_read > 0) {
            SHA256_Update(&sha256_ctx, buffer, bytes_read);
        }
    }

    file.close();

    unsigned char hash[SHA256_DIGEST_LENGTH];  // 32 bytes
    SHA256_Final(hash, &sha256_ctx);

     return bytes_to_hex(hash, SHA256_DIGEST_LENGTH);
}

std::string Checksum::bytes_to_hex(const unsigned char* data, size_t length) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < length; i++) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

std::string Checksum::to_lowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool Checksum::verify_sha256(const std::filesystem::path& file_path, 
                              const std::string& expected_hash) {
    // Compute actual hash
    std::string actual_hash = compute_sha256(file_path);
    
    if (actual_hash.empty()) {
        return false;  // Failed to compute hash
    }
    
    // Compare case-insensitively
    std::string actual_lower = to_lowercase(actual_hash);
    std::string expected_lower = to_lowercase(expected_hash);
    
    return actual_lower == expected_lower;
}




