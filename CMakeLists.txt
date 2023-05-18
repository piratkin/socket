cmake_minimum_required(VERSION 3.11)

project(socket VERSION 0.0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE include)
	
if (MSVC)
    target_compile_definitions(${PROJECT_NAME} 
        INTERFACE _WINSOCKAPI_
	    _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()	