cmake_minimum_required(VERSION 3.20)
project(greenfield VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(raylib
    URL https://codeload.github.com/raysan5/raylib/tar.gz/dbc56a87da87d973a9c5baa4e7438a9d20121d28
    URL_HASH SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126
)
FetchContent_MakeAvailable(raylib)
# raylib 6.0's config.h supports numeric feature overrides directly.
target_compile_definitions(raylib PRIVATE
    SUPPORT_FILEFORMAT_JPG=1 SUPPORT_MODULE_RAUDIO=0 SUPPORT_MODULE_RMODELS=0)

set(GREENFIELD_VIEW_SOURCES
    src/view/actions.c src/view/assets.c src/view/hud.c src/view/input.c
    src/view/panels.c src/view/selection.c src/view/screens.c src/view/widgets.c src/view/world.c)
add_executable(greenfield src/main.c src/reference.c src/game_view.c ${GREENFIELD_VIEW_SOURCES})
target_link_libraries(greenfield PRIVATE raylib greenfield_sim)
if(NOT MSVC)
    target_compile_options(greenfield PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(greenfield PRIVATE m)
endif()

# Place assets beside the executable; running never depends on the shell's cwd.
set_target_properties(greenfield PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(APPLE)
    set_target_properties(greenfield PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME Greenfield
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
endif()
add_custom_command(TARGET greenfield POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:greenfield>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/ui.jpeg"
        "$<TARGET_FILE_DIR:greenfield>/assets/ui.jpeg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/overlay-repair.png"
        "$<TARGET_FILE_DIR:greenfield>/assets/overlay-repair.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/title-repair.png"
        "$<TARGET_FILE_DIR:greenfield>/assets/title-repair.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/src/scene.fs"
        "$<TARGET_FILE_DIR:greenfield>/assets/scene.fs")
# A shader edit must also refresh the executable's asset copy.
set_property(TARGET greenfield APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scene.fs"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/overlay-repair.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/title-repair.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui.jpeg")

include(CTest)
if(BUILD_TESTING)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/evidence")
    add_test(NAME scene_fidelity COMMAND greenfield --verify
        "${CMAKE_BINARY_DIR}/evidence")
endif()

add_library(greenfield_sim STATIC
    src/game.c src/save.c
    src/sim/definitions.c src/sim/validation.c src/sim/world.c
    src/sim/citizens.c src/sim/buildings.c src/sim/economy.c src/sim/jobs.c
    src/sim/workshop.c src/sim/farming.c src/sim/survival.c src/sim/rivals.c
    src/sim/orders.c src/sim/production.c src/sim/progression.c src/sim/combat.c src/sim/ai.c)
target_include_directories(greenfield_sim PUBLIC src)
if(NOT MSVC)
    target_compile_options(greenfield_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    add_executable(game_tests tests/rts_tests.c)
    target_link_libraries(game_tests PRIVATE greenfield_sim)
    target_compile_definitions(game_tests PRIVATE
        OLD_SAVE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/campaign-5000.gfv1")
    add_test(NAME simulation COMMAND game_tests)
    add_executable(game_campaign tests/campaign.c)
    target_link_libraries(game_campaign PRIVATE greenfield_sim)
    add_test(NAME treaty_campaign COMMAND game_campaign treaty)
    add_test(NAME military_campaign COMMAND game_campaign military)
    add_executable(game_capacity_tests tests/test_capacity.c)
    target_link_libraries(game_capacity_tests PRIVATE greenfield_sim)
    add_test(NAME population_capacity COMMAND game_capacity_tests)
    # Exercise the same input/command adapters with controlled input, without
    # opening a window or sending events to the player's running game.
    add_executable(game_view_tests tests/test_view.c
        ${GREENFIELD_VIEW_SOURCES})
    target_link_libraries(game_view_tests PRIVATE raylib greenfield_sim)
    target_compile_definitions(game_view_tests PRIVATE
        VIEW_TEST_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/"
        GetApplicationDirectory=TestApplicationDirectory
        IsKeyPressed=TestKeyPressed IsKeyDown=TestKeyDown
        IsMouseButtonPressed=TestMousePressed IsMouseButtonDown=TestMouseDown
        GetMouseWheelMove=TestMouseWheel GetMouseDelta=TestMouseDelta
        GetScreenWidth=TestScreenWidth GetScreenHeight=TestScreenHeight
        ToggleBorderlessWindowed=TestFullscreen)
    if(NOT MSVC)
        target_compile_options(game_view_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_link_libraries(game_view_tests PRIVATE m)
    endif()
    add_test(NAME view_input COMMAND game_view_tests)
    add_test(NAME hud_interactions COMMAND game_view_tests --render)
endif()

add_custom_command(TARGET greenfield POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/bronze-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-Semibold.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-LICENSE.md"
        "$<TARGET_FILE_DIR:greenfield>/assets")
set_property(TARGET greenfield APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/bronze-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-Semibold.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/SourceSans3-LICENSE.md")
if(BUILD_TESTING)
    add_executable(game_art_tests tests/test_art.c)
    target_link_libraries(game_art_tests PRIVATE raylib)
    add_test(NAME game_capture COMMAND greenfield --game-capture "${CMAKE_BINARY_DIR}/evidence/game-winter.png")
    set_tests_properties(game_capture PROPERTIES FIXTURES_SETUP game_image)
    add_test(NAME pixel_art COMMAND game_art_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
        "${CMAKE_BINARY_DIR}/evidence/game-winter.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/bronze-atlas.png")
    set_tests_properties(pixel_art PROPERTIES FIXTURES_REQUIRED game_image)
endif()
