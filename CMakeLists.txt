cmake_minimum_required(VERSION 3.20)
project(greenfield VERSION 1.0.0 LANGUAGES C)

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

add_executable(greenfield src/main.c src/reference.c src/game_view.c)
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

add_library(greenfield_sim STATIC src/game.c src/save.c)
target_include_directories(greenfield_sim PUBLIC src)
if(NOT MSVC)
    target_compile_options(greenfield_sim PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    add_executable(game_tests tests/test_game.c tests/campaign.c)
    target_link_libraries(game_tests PRIVATE greenfield_sim)
    add_test(NAME simulation COMMAND game_tests)
    add_test(NAME balanced_campaign COMMAND game_tests --campaign)
endif()

add_custom_command(TARGET greenfield POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-Regular.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-Bold.ttf"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-LICENSE.txt"
        "$<TARGET_FILE_DIR:greenfield>/assets")
set_property(TARGET greenfield APPEND PROPERTY LINK_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-Regular.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-Bold.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/AtkinsonHyperlegible-LICENSE.txt")
if(BUILD_TESTING)
    add_executable(game_art_tests tests/test_art.c)
    target_link_libraries(game_art_tests PRIVATE raylib)
    add_test(NAME game_capture COMMAND greenfield --game-capture "${CMAKE_BINARY_DIR}/evidence/game-winter.png")
    set_tests_properties(game_capture PROPERTIES FIXTURES_SETUP game_image)
    add_test(NAME pixel_art COMMAND game_art_tests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/settlement-atlas.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-atlas.png"
        "${CMAKE_BINARY_DIR}/evidence/game-winter.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/citizen-braids-atlas.png")
    set_tests_properties(pixel_art PROPERTIES FIXTURES_REQUIRED game_image)
endif()
