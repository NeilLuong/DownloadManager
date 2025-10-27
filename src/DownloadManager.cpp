#include <iostream>
#include <curl/curl.h>
#include <string>
#include "HttpClient.h"

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE* stream){
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int main(int argc, char* argv[]) {
    
    CURL *curl = curl_easy_init();
    FILE *fp = nullptr;
    std::string url = "https://static.wikia.nocookie.net/cswikia/images/3/39/Cs2-knife-butterfly-stock-market.png/revision/latest?cb=20230928180043";
    char out_filename[FILENAME_MAX] = "test.png";
    CurlHttpClient httpClient;
    httpClient.download_file(url, out_filename);

    std::cout << "Download Manager v1.0" << std::endl;
    return 0;
}

