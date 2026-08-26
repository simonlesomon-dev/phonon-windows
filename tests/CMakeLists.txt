# --- Unit tests (no external deps) ------------------------------------------
add_executable(unit_tests
  test_unit.cpp
  ../src/resampler.cpp
  ../src/normalizer.cpp
  ../src/mel_frontend.cpp
)
target_include_directories(unit_tests PRIVATE ../src)
set_target_properties(unit_tests PROPERTIES WIN32_EXECUTABLE FALSE)

add_test(NAME unit_tests COMMAND unit_tests)

# --- Integration: real transcription (needs OpenVINO + converted model) ------
if(OpenVINO_FOUND)
  add_executable(transcription_test
    test_transcription.cpp
    ../src/transcriber.cpp
    ../src/mel_frontend.cpp
    ../src/resampler.cpp
    ../src/normalizer.cpp
  )
  target_include_directories(transcription_test PRIVATE ../src)
  target_link_libraries(transcription_test PRIVATE openvino::runtime)
  set_target_properties(transcription_test PROPERTIES WIN32_EXECUTABLE FALSE)
  add_test(NAME transcription_test COMMAND transcription_test)
endif()
