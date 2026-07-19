cmake_minimum_required(VERSION 3.20)

project(EunshinBishopPhase5Q LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(eunshin_q_core STATIC
    src/core/attacks.cpp
    src/core/bitboard.cpp
    src/core/random.cpp
    src/core/zobrist.cpp
    src/eval/classical.cpp
    src/eval/evaluator.cpp
    src/eval/nnue/network.cpp
    src/engine/engine.cpp
    src/engine/options.cpp
    src/position/fen.cpp
    src/position/movegen.cpp
    src/position/position.cpp
    src/search/history.cpp
    src/search/aegis.cpp
    src/search/limbo.cpp
    src/search/move_picker.cpp
    src/search/search.cpp
    src/search/see.cpp
    src/search/time_manager.cpp
    src/search/tt.cpp
    src/search/worker.cpp
)

target_include_directories(eunshin_q_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
target_link_libraries(eunshin_q_core PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(eunshin_q_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(eunshin_q_core PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    )
endif()

enable_testing()

add_executable(EunshinBishopQCoreTests
    tests/core_tests.cpp
)
target_link_libraries(EunshinBishopQCoreTests PRIVATE eunshin_q_core)

if(MSVC)
    target_compile_options(EunshinBishopQCoreTests PRIVATE /W4 /permissive-)
else()
    target_compile_options(EunshinBishopQCoreTests PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    )
endif()

add_test(NAME q_core_tests COMMAND EunshinBishopQCoreTests)

add_executable(EunshinBishop
    src/main.cpp
    src/uci/uci.cpp
)
target_link_libraries(EunshinBishop PRIVATE eunshin_q_core)

if(MSVC)
    target_compile_options(EunshinBishop PRIVATE /W4 /permissive-)
else()
    target_compile_options(EunshinBishop PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    )
endif()

add_executable(EunshinBishopQIntegrationTests
    tests/integration_tests.cpp
    src/uci/uci.cpp
)
target_link_libraries(EunshinBishopQIntegrationTests PRIVATE eunshin_q_core)

if(MSVC)
    target_compile_options(EunshinBishopQIntegrationTests PRIVATE
        /W4 /permissive-
    )
else()
    target_compile_options(EunshinBishopQIntegrationTests PRIVATE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    )
endif()

set(EUNSHIN_DEFAULT_NETWORK
    "${CMAKE_CURRENT_SOURCE_DIR}/networks/firstnet_v5_10b.snnue")
if(EXISTS "${EUNSHIN_DEFAULT_NETWORK}")
    add_custom_command(TARGET EunshinBishopQCoreTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EUNSHIN_DEFAULT_NETWORK}"
                "$<TARGET_FILE_DIR:EunshinBishopQCoreTests>/firstnet_v5_10b.snnue"
        COMMENT "Copying the default FIRST_NET v5 beside the executable"
        VERBATIM
    )
    add_custom_command(TARGET EunshinBishop POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EUNSHIN_DEFAULT_NETWORK}"
                "$<TARGET_FILE_DIR:EunshinBishop>/firstnet_v5_10b.snnue"
        COMMENT "Copying the default FIRST_NET v5 beside the engine"
        VERBATIM
    )
    add_custom_command(TARGET EunshinBishopQIntegrationTests POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EUNSHIN_DEFAULT_NETWORK}"
                "$<TARGET_FILE_DIR:EunshinBishopQIntegrationTests>/firstnet_v5_10b.snnue"
        COMMENT "Copying the default FIRST_NET v5 beside the integration tests"
        VERBATIM
    )
    add_test(NAME q_integration_tests
        COMMAND EunshinBishopQIntegrationTests "${EUNSHIN_DEFAULT_NETWORK}"
    )
    set_tests_properties(q_integration_tests PROPERTIES TIMEOUT 300)
    # POST_BUILD covers direct executable builds.  This default target also
    # runs copy_if_different on an otherwise up-to-date build, so replacing
    # only the weights cannot leave a stale runtime copy.
    add_custom_target(eunshin_q_network_copy ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${EUNSHIN_DEFAULT_NETWORK}"
            "$<TARGET_FILE_DIR:EunshinBishopQCoreTests>/firstnet_v5_10b.snnue"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EUNSHIN_DEFAULT_NETWORK}"
                "$<TARGET_FILE_DIR:EunshinBishop>/firstnet_v5_10b.snnue"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${EUNSHIN_DEFAULT_NETWORK}"
                "$<TARGET_FILE_DIR:EunshinBishopQIntegrationTests>/firstnet_v5_10b.snnue"
        DEPENDS
            "${EUNSHIN_DEFAULT_NETWORK}"
            EunshinBishopQCoreTests
            EunshinBishop
            EunshinBishopQIntegrationTests
        COMMENT "Synchronizing the default FIRST_NET v5 runtime copy"
        VERBATIM
    )
endif()
