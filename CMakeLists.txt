cmake_minimum_required(VERSION 3.20)
project(PhononWindows LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_WIN32_EXECUTABLE ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_definitions(UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)

# --- OpenVINO ---------------------------------------------------------------
find_package(OpenVINO REQUIRED COMPONENTS Runtime)
message(STATUS "OpenVINO runtime: ${OpenVINO_VERSION}")

add_executable(phonon WIN32
  src/main.cpp
  src/app.cpp
  src/audio_capture.cpp
  src/resampler.cpp
  src/mel_frontend.cpp
  src/transcriber.cpp
  src/paster.cpp
  src/model_downloader.cpp
  src/normalizer.cpp
  src/tray_icon.cpp
  src/settings.cpp
)

target_include_directories(phonon PRIVATE src)
target_link_libraries(phonon PRIVATE
  openvino::runtime
  ole32 oleaut32 uuid user32 shell32 gdi32 comctl32 winhttp winmm
)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
endif()

# --- Tests ------------------------------------------------------------------
enable_testing()
add_subdirectory(tests)

install(TARGETS phonon RUNTIME DESTINATION .)
