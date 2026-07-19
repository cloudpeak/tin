add_executable(simple simple.cc)
target_link_libraries(simple ${DEP_LIBS})

# Ensure simple uses the same MSVC runtime as tin/abseil (MultiThreadedDebugDLL).
# CMAKE_MSVC_RUNTIME_LIBRARY should handle this, but with the ClangCL toolset
# the generated <RuntimeLibrary> property can end up empty for executables.
if(WIN32)
  target_compile_options(simple PRIVATE
    "$<$<CONFIG:Debug>:/MDd>"
    "$<$<CONFIG:Release>:/MD>"
    "$<$<CONFIG:RelWithDebInfo>:/MD>"
    "$<$<CONFIG:MinSizeRel>:/MD>"
  )
endif()