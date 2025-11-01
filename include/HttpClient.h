#include <curl/curl.h>
#include <string>
#include <chrono>
#include <filesystem> // cross platform file/dir operations
class CurlHttpClient{
public:
    CurlHttpClient();
    ~CurlHttpClient();

    bool download_file(std::string& url, std::string& output_path);

    static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE* stream);

    static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

    static std::string format_bytes(curl_off_t bytes); 


    
private:
    CURL *curl;
    std::chrono::steady_clock::time_point start_time;
    curl_off_t last_dlnow;
    std::chrono::steady_clock::time_point last_time;
    bool progress_complete;
    curl_off_t resume_from;

    bool ensure_dir_exists(const std::filesystem::path& file_path);
    bool check_disk_space(const std::filesystem::path& file_path, curl_off_t required_bytes);
};