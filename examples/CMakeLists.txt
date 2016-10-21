if(APPLE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -framework Cocoa")
endif()

add_subdirectory(simple)
set_property(TARGET simple PROPERTY FOLDER "examples")

add_subdirectory(echo)
set_property(TARGET echo PROPERTY FOLDER "examples")

