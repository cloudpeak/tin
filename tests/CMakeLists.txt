# Minimal test framework for tin.
#
# We avoid a hard dependency on GoogleTest to keep the build self-contained.
# Each test file contains `static void TestXxx()` functions registered in
# `test_main.cc` via a simple registry. Tests use `CHECK()` / `DCHECK()`
# from abseil (already linked) for assertions.

add_executable(tin_tests
  test_main.cc
  status_test.cc
  config_test.cc
  ip_address_test.cc
  mutex_test.cc
  atomic_test.cc
)
target_link_libraries(tin_tests PRIVATE tin zcontext pthread rt)

# Register with CTest so `ctest` discovers the tests.
enable_testing()
add_test(NAME tin_tests COMMAND tin_tests)
