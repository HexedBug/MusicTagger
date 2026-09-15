cmake_minimum_required(VERSION 3.20)

project(MusicTagger VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(TagLib CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(MUSICTAGGER_SOURCES
    src/Application.cpp
    src/StringUtils.cpp
    src/FilenameParser.cpp
    src/HttpClient.cpp
    src/MusicBrainzClient.cpp
    src/CoverArtClient.cpp
    src/TagWriter.cpp
)

add_executable(MusicTagger
    src/main.cpp
    ${MUSICTAGGER_SOURCES}
)

target_include_directories(MusicTagger
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(MusicTagger
    PRIVATE
        TagLib::tag
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

if(MSVC)
    target_compile_options(MusicTagger PRIVATE /W4)
else()
    target_compile_options(MusicTagger PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(MusicTaggerTests
        tests/MusicTaggerTests.cpp
        src/StringUtils.cpp
        src/FilenameParser.cpp
    )

    target_include_directories(MusicTaggerTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if(MSVC)
        target_compile_options(MusicTaggerTests PRIVATE /W4)
    else()
        target_compile_options(MusicTaggerTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME MusicTaggerTests COMMAND MusicTaggerTests)
endif()
