find_library(libcorrect_LIBRARIES NAMES correct PATHS ${PC_libcorrect_LIBRARY_DIRS})

pkg_check_modules(LIBACARS REQUIRED libacars-2)

include_directories(${LIBACARS_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIRS} ${COMMON_INCLUDE_DIR})

add_executable(
  aero-decode 
  main.cpp 
  output.cpp 
  decode.cpp 
  forwarder.cpp
  burstmskdemodulator.cpp
  burstoqpskdemodulator.cpp
  mskdemodulator.cpp
  oqpskdemodulator.cpp
  DSP.cpp
  jfft.cpp
  coarsefreqestimate.cpp
  fftwrapper.cpp
  fftrwrapper.cpp
  aerol.cpp
  jconvolutionalcodec.cpp
  databasetext.cpp
  hunter.cpp
  ${COMMON_NOTIFIER_SOURCE_FILE}
  ${COMMON_LOGGER_SOURCE_FILE}
)
target_link_libraries(aero-decode PRIVATE ${ZeroMQ_LIBRARIES} ${LIBACARS_LIBRARIES} ${libcorrect_LIBRARIES} Qt6::Concurrent Qt6::Core Qt6::Network)
 
