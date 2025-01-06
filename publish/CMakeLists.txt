find_package(SoapySDR 0.8.1 REQUIRED)

include_directories(${SoapySDR_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIRS})

add_executable(aero-publish main.cpp publisher.cpp)
target_link_libraries(aero-publish PRIVATE ${SoapySDR_LIBRARIES} ${ZeroMQ_LIBRARIES} Qt5::Concurrent Qt5::Core)
