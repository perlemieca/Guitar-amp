cmake_minimum_required(VERSION 3.22.1)
project("guitarampengine")

find_package(oboe REQUIRED CONFIG)

add_library(guitarampengine SHARED
        native-lib.cpp)

target_link_libraries(guitarampengine
        oboe::oboe
        android
        log)

target_compile_features(guitarampengine PRIVATE cxx_std_17)
