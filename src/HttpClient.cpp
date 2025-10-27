#include "HttpClient.h"
#include <iostream>

CurlHttpClient::CurlHttpClient() {
    std::cout << "Initializing CURL instance." << std::endl;
    curl = curl_easy_init();
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

bool CurlHttpClient::download_file(std::string& url, char* output_path) {
    if(curl){
        FILE *fp = fopen(output_path, "wb");
        if(!fp){
            return false;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK){
            fclose(fp);
            return false;
        }
        fclose(fp);
        return true;
    }
    return false;
}