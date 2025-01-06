find_package(SoapySDR 0.8.1 REQUIRED)

include_directories(${SoapySDR_INCLUDE_DIRS} ${ZeroMQ_INCLUDE_DIRS} ${COMMON_INCLUDE_DIR})

add_executable(aero-publish main.cpp publisher.cpp oscillator.cpp vfo.cpp zmqpublisher.cpp dsp.cpp halfbanddecimator.cpp firfilter.cpp ${COMMON_NOTIFIER_SOURCE_FILE})
target_link_libraries(aero-publish PRIVATE ${SoapySDR_LIBRARIES} ${ZeroMQ_LIBRARIES} Qt5::Concurrent Qt5::Core)
