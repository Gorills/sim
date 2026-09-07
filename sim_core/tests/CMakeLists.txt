add_executable(sim_core_tests
    test_sim_core.cpp
)

target_link_libraries(sim_core_tests PRIVATE sim::core)
target_compile_features(sim_core_tests PRIVATE cxx_std_20)
sim_set_warnings(sim_core_tests)
sim_enable_sanitizers(sim_core_tests)

add_test(NAME sim_core_tests COMMAND sim_core_tests)
