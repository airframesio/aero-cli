find_library(libcorrect_LIBRARIES NAMES correct PATHS ${PC_libcorrect_LIBRARY_DIRS})

include_directories(${ZeroMQ_INCLUDE_DIRS})

add_executable(aero-decode main.cpp)
target_link_libraries(aero-decode PRIVATE ${ZeroMQ_LIBRARIES} ${libcorrect_LIBRARIES} Qt6::Concurrent Qt6::Core)
 
