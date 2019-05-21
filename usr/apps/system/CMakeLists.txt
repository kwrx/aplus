cmake_minimum_required(VERSION 3.1)
project(sysutils)

file(GLOB apps RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
foreach(app ${apps})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${app})
    
        file(GLOB_RECURSE sources "${app}/*.c" "${app}/*.cpp" "${app}/*.s")
        add_executable(${app} ${sources})
        target_compile_definitions(${app} PRIVATE COMMIT="unknown")
        target_link_libraries(${app} aplus c m pthread ssp)

        install(TARGETS ${app} DESTINATION ${CMAKE_SOURCE_DIR}/root/usr/sbin)
    
    endif()
endforeach(app ${apps})

