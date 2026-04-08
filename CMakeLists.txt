cmake_minimum_required (VERSION 3.19)


set(target_name "Logginator-Lib")

add_library(${target_name} STATIC)
target_compile_features(${target_name} PUBLIC cxx_std_23)


message(STATUS "->       Logginator-Lib:")
message(STATUS "########################")



target_include_directories(${target_name}
 PUBLIC  "${CMAKE_CURRENT_LIST_DIR}/inc"
)

target_sources(${target_name}
 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/logginator.cpp"
 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/logginator-manager.cpp"
 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/src/logginator-printer.cpp"
)

target_link_libraries(${target_name}
)



