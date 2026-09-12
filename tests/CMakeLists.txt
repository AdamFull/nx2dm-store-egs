
add_executable(nx_store_egs_tests
        test_store_egs_config.cpp
        test_store_egs_module.cpp
        test_store_egs_services.cpp
        test_store_egs_extras.cpp
        ../store_egs_config.cpp
        ../store_egs_module.cpp
        ../store_egs_platform.cpp
        ../store_egs_services.cpp
        ../store_egs_leaderboards.cpp
        ../store_egs_overlay.cpp
        ../store_egs_mods.cpp
        ../store_egs_scripting.cpp)

nx_module_registry(nx_store_egs_tests)

target_include_directories(nx_store_egs_tests PRIVATE
        "${NX_ENGINE_ROOT}/engine" "${NX_ENGINE_ROOT}/modules")

target_link_libraries(nx_store_egs_tests
        PRIVATE nx::test nx::foundation nx::engine nx::eossdk nx::compile_options)

set_target_properties(nx_store_egs_tests PROPERTIES FOLDER "tests")
nxtest_discover_tests(nx_store_egs_tests LABELS store_egs)
