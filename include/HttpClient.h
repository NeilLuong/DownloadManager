#include <curl/curl.h>
#include <string>

class CurlHttpClient{
public:
    CurlHttpClient();
    ~CurlHttpClient();

    bool download_file(std::string& url, char* output_path);

    static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE* stream);
    
private:
    CURL *curl;
};