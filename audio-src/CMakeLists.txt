include_directories(${ZeroMQ_INCLUDE_DIRS} ${COMMON_INCLUDE_DIR})

add_executable(
  aero-audio-src 
  main.cpp
  sink.cpp 
  ${COMMON_NOTIFIER_SOURCE_FILE}
  ${COMMON_LOGGER_SOURCE_FILE}
)
target_link_libraries(aero-audio-src PRIVATE ${ZeroMQ_LIBRARIES} Qt6::Concurrent Qt6::Core Qt6::Multimedia)
 
