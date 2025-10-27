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
        // curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);


        CURLcode res = curl_easy_perform(curl);
        long response_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
        if(res == CURLE_OK){
            if(response_code == 200){
                std::cout << response_code << std::endl;
                fclose(fp);
                return true;
            }else if(response_code == 404){
                std::cout << "Error 404: File not found." << std::endl;
                fclose(fp);
                return false;
            }else{
                std::cout << "HTTP Error: " << response_code << std::endl;
                fclose(fp);
                return false;
            }
        }else{
        std::cout << "CURL Error: " << curl_easy_strerror(res) << std::endl;
        fclose(fp);
        return false;
        }
    }
    return false;
}