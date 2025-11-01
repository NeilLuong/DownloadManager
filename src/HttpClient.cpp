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


bool CurlHttpClient::download_file(std::string& url, std::string& output_path) {
    if(curl){
        std::filesystem::path final_path(output_path);
        std::filesystem::path temp_path = final_path;
        temp_path += ".part";

        if (!ensure_dir_exists(final_path)) {
            std::cerr << "\nFailed to create directory for: " << output_path << std::endl;
            return false;
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
            std::cerr << "Check permissions and disk space." << std::endl;
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
        
        /**
         * Reset attributes for new downloads
         */
        start_time = std::chrono::steady_clock::now();
        last_time = start_time;
        progress_complete = false; 


        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

        //Get the download file size
        curl_off_t download_size = 0;
        curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &download_size);

        fclose(fp);

        if(res == CURLE_OK){
            if(response_code == 200 || response_code == 206){
                std::cout << std::endl;  // Ensure we're on a new line

                if (resuming && response_code == 200) {
                    std::cout << "Server doesn't support resume. Restarting from beginning..." << std::endl;
                    //file is corrupted since the server will be sending the request for the full file again. 
                    //But the .part file will be open in append mode => appended full file to existing partial file
                    //Let user know to delete .part file and redownload
                    std::cerr << "Warning: Downloaded file may be corrupted. Please delete .part file and retry." << std::endl;
                }
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
                
                return true;
            }else if(response_code == 404){
                std::cout << "\nError 404: File not found." << std::endl;
                std::filesystem::remove(temp_path);
                return false;
            }else{
                std::cout << "\nHTTP Error: " << response_code << std::endl;
                std::filesystem::remove(temp_path);
                return false;
            }
        }else{
        std::cout << "\nCURL Error: " << curl_easy_strerror(res) << std::endl;
        std::filesystem::remove(temp_path);
        return false;
        }
    }
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