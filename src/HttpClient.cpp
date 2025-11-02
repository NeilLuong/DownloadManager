#include <iostream>
#include <iomanip>
#include "HttpClient.h"


CurlHttpClient::CurlHttpClient() {
    std::cout << "Initializing CURL instance." << std::endl;
    curl = curl_easy_init();
    last_dlnow = 0;
    progress_complete = false; 
    resume_from = 0;
}

CurlHttpClient::~CurlHttpClient() {
    if(curl) {
        std::cout << "Cleaning up CURL resources." << std::endl;
        curl_easy_cleanup(curl);
    }
}

size_t CurlHttpClient::write_data(void *ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

bool CurlHttpClient::ensure_dir_exists(const std::filesystem::path& file_path) {
    try {
        std::filesystem::path dir = file_path.parent_path();
        
        if(dir.empty()) {
            return true;
        }

        if (std::filesystem::exists(dir)) {
            return true;
        }

        std::filesystem::create_directories(dir);
        std::cout << "Created directory: " << dir << std::endl;
        return true;
    } catch (const std::filesystem::filesystem_error e) {
        std::cerr << "\nError creating directory: " << e.what() << std::endl;
        return false;
    }
}

bool CurlHttpClient::check_disk_space(const std::filesystem::path& file_path, curl_off_t required_bytes) {
    try {
        std::filesystem::path dir = file_path.parent_path();

        if (dir.empty()) {
            dir = std::filesystem::current_path();
        }

        std::filesystem::space_info space = std::filesystem::space(dir);
        if (space.available < static_cast<uintmax_t>(required_bytes)) {
            std::cerr << "\nInsufficient disk space!" << std::endl;
            std::cerr << "Required: " << (required_bytes / (1024.0 * 1024.0)) << " MB" << std::endl;
            std::cerr << "Available: " << (space.available / (1024.0 * 1024.0)) << " MB" << std::endl;
            return false;
        }

        return true;

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "\nError checking disk space: " << e.what() << std::endl;
        // proceed anyway
        return true;
    }
}

ErrorType CurlHttpClient::classify_error(CURLcode curl_error, long http_code) {
    if (curl_error == CURLE_OK) {
        if (http_code >= 200 && http_code < 300) {
            return ErrorType::Success;
        }
    }

    //HTTP 4xx errors are permanent (client errors)
    if (http_code >= 400 && http_code < 500) {
        return ErrorType::Permanent;
    }

    //HTTP 5xx are transient (server erros which might recover)
    if (http_code >= 500 && http_code < 600) {
        return ErrorType::Transient;
    }

    switch (curl_error)
    {
    //transient errors
    case CURLE_OPERATION_TIMEDOUT: // Timeout reached
    case CURLE_COULDNT_CONNECT: // Failed to connect
    case CURLE_COULDNT_RESOLVE_HOST: // DNS lookup failed
    case CURLE_RECV_ERROR: // Failed receiving data
    case CURLE_SEND_ERROR: // Failed sending data
    case CURLE_PARTIAL_FILE: // Transfer ended prematurely
    case CURLE_GOT_NOTHING: // Server returned nothing
        return ErrorType::Transient;
    
    //permanent errors, dont retry
    case CURLE_URL_MALFORMAT:           // Invalid URL format
    case CURLE_UNSUPPORTED_PROTOCOL:    // Protocol not supported
    case CURLE_SSL_CONNECT_ERROR:       // SSL handshake failed
    case CURLE_SSL_CERTPROBLEM:         // SSL certificate problem
    case CURLE_PEER_FAILED_VERIFICATION: // SSL peer certificate failed
    case CURLE_HTTP_RETURNED_ERROR:     // HTTP error (handled above by http_code)
        return ErrorType::Permanent;

    default:
        return ErrorType::Transient;
    }
}


int CurlHttpClient::progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    CurlHttpClient* client = static_cast<CurlHttpClient*>(clientp);

    if (dltotal <= 0) {
        return 0;  // Continue download
    }

    // Skip if we've already printed completion
    if (client->progress_complete) {
        return 0;
    }

    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - client->last_time);

    if (time_since_last.count() < 1000 && dlnow < dltotal) {
        return 0;  // Don't update display yet. This is a special case
    }

    //Account for resumed downloads
    curl_off_t total_downloaded = client->resume_from + dlnow;
    curl_off_t total_size = client->resume_from + dltotal;

    double percentage = (total_downloaded * 100.0) / total_size;

    auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - client->start_time);
    double speed_bps = 0.0;

    if(total_elapsed.count() > 0){
        speed_bps = dlnow / static_cast<double>(total_elapsed.count());
    }
    
    double speed_display = 0.0;
    std::string speed_unit;

    if (speed_bps >= 1024 * 1024) {
        speed_display = speed_bps / (1024 * 1024);
        speed_unit = "MB/s";
    } else {
        speed_display = speed_bps / 1024;
        speed_unit = "KB/s";
    }

    int eta_seconds = 0;
    if (speed_bps > 0) {
        curl_off_t remaining_bytes = dltotal - dlnow;
        eta_seconds = static_cast<int>(remaining_bytes / speed_bps);
    }

    // auto format_bytes = [](curl_off_t bytes) -> std::string {
    // };

    int bar_width = 20;
    int filled = static_cast<int>(percentage / 100.0 * bar_width);
    std::string bar = "[";
    for(int i = 0; i < bar_width; ++i){
        if(i < filled){
            bar += "=";
        }else if(i == filled){
            bar += ">";
        }else{
            bar += " ";
        }
    }
    bar += "]";

    std::cout << "\r" << bar << " "
              << std::fixed << std::setprecision(1) << percentage << "% | "
              << format_bytes(total_downloaded) << " / " << format_bytes(total_size) << " | "
              << std::setprecision(2) << speed_display << speed_unit << " | "
              << "ETA: " << eta_seconds << "s      "
              << std::flush;

    client->last_time = now;
    client->last_dlnow = dlnow;

    if(dlnow >= dltotal && !client->progress_complete){
        std::cout << std::endl;
        client->progress_complete = true;
    }
    return 0;
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    char buffer[26];

    #ifdef _WIN32
        ctime_s(buffer, sizeof(buffer), &now_time_t);
    #else
        ctime_r(&now_time_t, buffer);
    #endif

    std::string timestamp(buffer);
    if (!timestamp.empty() && timestamp.back() == '\n') {
        timestamp.pop_back();
    }

    return timestamp;
}


bool CurlHttpClient::download_file(std::string& url, std::string& output_path) {
    if(!curl) {
        return false;
    }

    std::filesystem::path final_path(output_path);
    std::filesystem::path temp_path = final_path;
    temp_path += ".part";

    if (!ensure_dir_exists(final_path)) {
        std::cerr << "\nFailed to create directory for: " << output_path << std::endl;
        return false;
    }

    for (int attempt = 0; attempt <= MAX_RETRIES; attempt++) {
        if (attempt > 0) {
            std::cout << "\n[" << get_timestamp() << "]"
                      << "Retry attempt " << attempt << "/" << MAX_RETRIES << "..." << std::endl;
        }

        CURL* head_curl = curl_easy_init();
        if (head_curl) {
            curl_easy_setopt(head_curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(head_curl, CURLOPT_NOBODY, 1L);  // HEAD request
            curl_easy_setopt(head_curl, CURLOPT_HEADER, 0L);
            
            CURLcode head_res = curl_easy_perform(head_curl);
            if (head_res == CURLE_OK) {
                curl_off_t file_size = 0;
                curl_easy_getinfo(head_curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &file_size);
        
                if (file_size > 0) {
                    // Check if we have enough disk space
                    if (!check_disk_space(final_path, file_size)) {
                        curl_easy_cleanup(head_curl);
                        return false;
                    }
                }
            }       
            curl_easy_cleanup(head_curl);
        }

        //resume scenario
        curl_off_t existing_size = 0;
        bool resuming = false;

        if (std::filesystem::exists(temp_path)) {
            existing_size = std::filesystem::file_size(temp_path);

            if (existing_size > 0) {
                resuming = true;
                resume_from = existing_size;
                std::cout << "\nFound partial download (" 
                          << format_bytes(existing_size)
                          << "). Resuming..." << std::endl; 
            } else {
                std::filesystem::remove(temp_path);
                existing_size = 0;
            }
        } else {
            resume_from = 0;
        }

        const char* file_mode = resuming ? "ab" : "wb";
        FILE *fp = fopen(temp_path.string().c_str(), file_mode);
        if(!fp){
            std::cerr << "\nFailed to open file for writing: " << temp_path << std::endl;
            return false;
        }
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        //Progress tracking
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

        if (resuming) {
            std::string range_header = std::to_string(existing_size) + "-";
            curl_easy_setopt(curl, CURLOPT_RANGE, range_header.c_str());
        }

        // Set timeout to avoid hanging forever
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);  // 5 minutes
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);  // 30 seconds to connect
            
        /**
         * Reset attributes for new downloads
         */
        start_time = std::chrono::steady_clock::now();
        last_time = start_time;
        progress_complete = false; 


        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);


        fclose(fp);

        ErrorType error_type = classify_error(res, response_code);

        if(error_type == ErrorType::Success){
            std::cout << std::endl;  // Ensure we're on a new line
            try
            {
                std::filesystem::rename(temp_path, final_path);
                std::cout << "Download complete: " << final_path << std::endl;
                return true;
            }
            catch(const std::filesystem::filesystem_error& e)
            {
                std::cerr << "Error renaming file: " << e.what() << std::endl;
                std::cerr << "Downloaded file is at: " << temp_path << std::endl;
                return false;
            }    
        }
        if (error_type == ErrorType::Permanent) {
            std::cout << std::endl; 
            if (res == CURLE_OK) {
                std::cerr << "\nHTTP Error " << response_code << ": ";
                if (response_code == 404) {
                    std::cerr << "File not found";
                } 
                else if (response_code == 403) {
                    std::cerr << "Access forbidden";
                } 
                else if (response_code == 401) {
                    std::cerr << "Authentication required";
                } else {
                    std::cerr << "Client error";
                }
            std::cerr << std::endl;
            } else {
                std::cerr << "\nError: " << curl_easy_strerror(res) << std::endl;
            }
            std::filesystem::remove(temp_path);
            return false;
        }

        if (error_type == ErrorType::Transient) {
            std::cout << std::endl;

            std::cerr << "[" << get_timestamp() << "] ";
            if (res == CURLE_OK) {
                std::cerr << "Server error (HTTP " << response_code << ")";
            } else {
                std::cerr << "Network error: " << curl_easy_strerror(res);
            }

            std::cerr << std::endl;

            if (attempt < MAX_RETRIES) {
                int delay = 1 << attempt;  // Exponential backoff: 2^attempt
                std::cout << "Waiting " << delay << " second(s) before retry..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(delay));
            }
            // continue to next iteration
            continue;
        }
    }

    std::cerr << "\nDownload failed after " << MAX_RETRIES << " retries." << std::endl;
    std::filesystem::remove(temp_path);
    return false;
}

std::string CurlHttpClient::format_bytes(curl_off_t bytes){
    if (bytes >= 1024 * 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024 * 1024)) + "GB";
        } else if (bytes >= 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024)) + "MB";
        } else if (bytes >= 1024) {
            return std::to_string(bytes / 1024) + "KB";
        }
        return std::to_string(bytes) + "B";
}