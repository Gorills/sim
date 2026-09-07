add_library(sim_core STATIC
    src/habitat.cpp
    src/scenario.cpp
    src/snapshot.cpp
    src/species.cpp
    src/world.cpp
)
add_library(sim::core ALIAS sim_core)

target_include_directories(sim_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(sim_core PUBLIC cxx_std_20)
set_target_properties(sim_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

sim_set_warnings(sim_core)
sim_enable_sanitizers(sim_core)
