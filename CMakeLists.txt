cmake_minimum_required(VERSION 3.10)

project(DownloadManager)

# Set C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# For editor integration (go-to-definition, auto-complete, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages installed by vcpkg
find_package(CURL REQUIRED)

add_executable(DownloadManager src/DownloadManager.cpp src/HttpClient.cpp)

# Link libraries
target_link_libraries(DownloadManager PRIVATE CURL::libcurl)

#to include header files
target_include_directories(DownloadManager PRIVATE include)
