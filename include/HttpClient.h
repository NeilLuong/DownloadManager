#include <curl/curl.h>
#include <string>
#include <chrono>
class CurlHttpClient{
public:
    CurlHttpClient();
    ~CurlHttpClient();

    bool download_file(std::string& url, std::string& output_path);

    static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE* stream);

    static int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
    
private:
    CURL *curl;
    std::chrono::steady_clock::time_point start_time;
    curl_off_t last_dlnow;
    std::chrono::steady_clock::time_point last_time;
    bool progress_complete;
};