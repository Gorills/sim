add_executable(sim_cli
    src/main.cpp
)

target_link_libraries(sim_cli PRIVATE sim::core)
target_compile_features(sim_cli PRIVATE cxx_std_20)
sim_set_warnings(sim_cli)
sim_enable_sanitizers(sim_cli)
