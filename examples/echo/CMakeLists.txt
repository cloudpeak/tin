add_executable(echo echo.cc)
target_link_libraries(echo ${DEP_LIBS})

# Ensure echo uses the same MSVC runtime as tin/abseil (MultiThreadedDebugDLL).
# CMAKE_MSVC_RUNTIME_LIBRARY should handle this, but with the ClangCL toolset
# the generated <RuntimeLibrary> property can end up empty for executables.
if(WIN32)
  target_compile_options(echo PRIVATE
    "$<$<CONFIG:Debug>:/MDd>"
    "$<$<CONFIG:Release>:/MD>"
    "$<$<CONFIG:RelWithDebInfo>:/MD>"
    "$<$<CONFIG:MinSizeRel>:/MD>"
  )
endif()