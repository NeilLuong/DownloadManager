#include <iostream>
#include <iomanip>
#include "HttpClient.h"


CurlHttpClient::CurlHttpClient() {
    std::cout << "Initializing CURL instance." << std::endl;
    curl = curl_easy_init();
    last_dlnow = 0;
    progress_complete = false; 
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

    double percentage = (dlnow * 100.0) / dltotal;

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

    auto format_bytes = [](curl_off_t bytes) -> std::string {
        if (bytes >= 1024 * 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024 * 1024)) + "GB";
        } else if (bytes >= 1024 * 1024) {
            return std::to_string(bytes / (1024 * 1024)) + "MB";
        } else if (bytes >= 1024) {
            return std::to_string(bytes / 1024) + "KB";
        }
        return std::to_string(bytes) + "B";
    };

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
              << format_bytes(dlnow) << " / " << format_bytes(dltotal) << " | "
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
        FILE *fp = fopen(output_path.c_str(), "wb");
        if(!fp){
            return false;
        }
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);
        
        /**
         * Reset attributes for new downloads
         */
        start_time = std::chrono::steady_clock::now();
        last_time = start_time;
        progress_complete = false; 

        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if(res == CURLE_OK){
            if(response_code == 200){
                std::cout << std::endl;  // Ensure we're on a new line
                fclose(fp);
                return true;
            }else if(response_code == 404){
                std::cout << "\nError 404: File not found." << std::endl;
                fclose(fp);
                return false;
            }else{
                std::cout << "\nHTTP Error: " << response_code << std::endl;
                fclose(fp);
                return false;
            }
        }else{
        std::cout << "\nCURL Error: " << curl_easy_strerror(res) << std::endl;
        fclose(fp);
        return false;
        }
    }
    return false;
}