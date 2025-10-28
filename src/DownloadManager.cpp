#include <iostream>
#include <curl/curl.h>
#include <string>
#include "HttpClient.h"

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE* stream){
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int main(int argc, char* argv[]) {
    std::cout << "Usage: DownloadManager [URL] [output_file]" << std::endl;
    if(argc != 3){
        std::cout << "Invalid arguements" << std::endl;
        return 1;
    }


    CURL *curl = curl_easy_init();
    FILE *fp = nullptr;
    std::string url = argv[1];
    std::string out_filename = argv[2];
    CurlHttpClient httpClient;
    httpClient.download_file(url, out_filename);

    std::cout << "Download Manager v1.0" << std::endl;
    return 0;
}

