cmake_minimum_required(VERSION 3.20)

# Placeholder modules - create minimal stub libraries
set(STUB_MODULES video sequence image properties)

foreach(MODULE ${STUB_MODULES})
    set(MODULE_SOURCES ${MODULE}_stub.cpp)
    
    # Create stub source file if it doesn't exist
    if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}_stub.cpp)
        file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/${MODULE}_stub.cpp 
            "// Stub implementation for ASI camera ${MODULE} module\n"
            "#include \"atom/log/loguru.hpp\"\n"
            "namespace lithium::device::asi::camera::${MODULE} {\n"
            "void initialize() { LOG_F(INFO, \"ASI camera ${MODULE} module initialized (stub)\"); }\n"
            "}\n")
    endif()
    
    add_library(asi_camera_${MODULE} SHARED ${MODULE_SOURCES})
    set_property(TARGET asi_camera_${MODULE} PROPERTY POSITION_INDEPENDENT_CODE 1)
    target_link_libraries(asi_camera_${MODULE} PUBLIC ${COMMON_LIBS})
    
    # Installation
    install(TARGETS asi_camera_${MODULE}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endforeach()
