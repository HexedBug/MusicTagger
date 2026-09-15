# MusicTagger

MusicTagger is a C++ command-line tool for fixing music files with missing or incorrect metadata. It reads the file’s existing tags and filename, searches MusicBrainz for possible matches, and lets the user choose the correct release before making any changes.

The selected metadata and available cover artwork are then written back to the music file using TagLib.

## What it does

* Reads existing metadata with TagLib
* Extracts artist and title information from filenames
* Removes filename text such as `Official Audio`, `Official Video`, and duplicate-download numbers such as `(1)`
* Asks for the artist or title when the file does not contain enough information
* Searches MusicBrainz for exact artist and title matches
* Finds the releases connected to each matching recording
* Lets the user select the correct album, single, or other release
* Retrieves front-cover artwork from the Cover Art Archive
* Shows a metadata preview before changing the file
* Writes updated tags and embeds the selected artwork
* Handles MusicBrainz rate limits and temporary service errors

## Metadata written

Depending on what MusicBrainz provides, MusicTagger can update:

* Title
* Artist
* Album
* Album artist
* Release date
* Release country
* Release type
* Track number and total
* Disc number and total
* MusicBrainz recording ID
* MusicBrainz release ID
* MusicBrainz release-track ID
* Front-cover artwork

## Requirements

* A C++17 compiler
* CMake 3.20 or newer
* vcpkg
* TagLib
* libcurl
* nlohmann/json

The required libraries are listed in `vcpkg.json`.

## Building on Windows

The project was developed using the MSYS2 UCRT64 MinGW toolchain with vcpkg.

From the project folder, run:

```powershell
cmake -S . -B build -G "MinGW Makefiles" `
    -DCMAKE_TOOLCHAIN_FILE=C:/dev/vcpkg/scripts/buildsystems/vcpkg.cmake `
    -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
    -DVCPKG_HOST_TRIPLET=x64-mingw-dynamic
```

Build the program:

```powershell
cmake --build build
```

If Windows reports that a required DLL is missing, copy the runtime files with:

```powershell
.\scripts\copy-runtime-dlls.ps1
```

## Running the program

Start MusicTagger without an argument:

```powershell
.\build\MusicTagger.exe
```

The program will ask for the path to a music file.

You can also provide the path when starting the program:

```powershell
.\build\MusicTagger.exe "C:\Music\Artist - Song.mp3"
```

MusicTagger will:

1. Read the file’s current tags.
2. Build search information from the tags and filename.
3. Search MusicBrainz for matching recordings.
4. Display the available releases.
5. Ask you to select the correct release.
6. Download cover artwork when it is available.
7. Preview the new metadata.
8. Ask for confirmation before writing anything.

## Running the tests

Build the project and run the tests with:

```powershell
cmake --build build
ctest --test-dir build --output-on-failure
```

The current tests cover filename parsing, search-candidate creation, whitespace handling, and text normalization without requiring an internet connection.

## Project layout

```text
MusicTagger/
├── include/
│   └── musictagger/
│       ├── AppConfig.hpp
│       ├── Application.hpp
│       ├── CoverArtClient.hpp
│       ├── FilenameParser.hpp
│       ├── HttpClient.hpp
│       ├── Models.hpp
│       ├── MusicBrainzClient.hpp
│       ├── StringUtils.hpp
│       └── TagWriter.hpp
├── scripts/
│   └── copy-runtime-dlls.ps1
├── src/
│   ├── Application.cpp
│   ├── CoverArtClient.cpp
│   ├── FilenameParser.cpp
│   ├── HttpClient.cpp
│   ├── MusicBrainzClient.cpp
│   ├── StringUtils.cpp
│   ├── TagWriter.cpp
│   └── main.cpp
├── tests/
│   └── MusicTaggerTests.cpp
├── CMakeLists.txt
└── vcpkg.json
```

## How the code is organized

The project is separated into smaller components instead of keeping everything in one source file:

* `Application` controls the command-line workflow.
* `FilenameParser` creates search candidates from filenames and existing tags.
* `HttpClient` handles text and binary HTTP requests through libcurl.
* `MusicBrainzClient` searches for recordings and retrieves release metadata.
* `CoverArtClient` downloads front-cover artwork.
* `TagWriter` reads and writes metadata with TagLib.
* `Models` contains the structures shared between these components.

## Current limitations

MusicTagger still depends on the file’s existing tags or filename to identify a song. It does not currently use audio fingerprinting, so files with no useful artist or title information may require manual input.

The program also handles one file at a time and requires an internet connection when searching MusicBrainz or downloading artwork.

## Possible improvements

* Add Chromaprint and AcoustID fingerprint matching
* Support tagging an entire folder at once
* Cache MusicBrainz responses
* Add saved JSON responses for offline API tests
* Add an automatic release-selection option for batch processing

## MusicBrainz user agent

Before distributing the program more widely, update `kUserAgent` in `include/musictagger/AppConfig.hpp` with the project’s GitHub URL or another contact identifier.
