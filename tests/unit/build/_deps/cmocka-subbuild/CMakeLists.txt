# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.16.3)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(cmocka-populate NONE)

include(ExternalProject)
ExternalProject_Add(cmocka-populate
                     "UPDATE_DISCONNECTED" "False" "GIT_REPOSITORY" "https://git.cryptomilk.org/projects/cmocka.git" "GIT_TAG" "cmocka-1.1.5" "GIT_SHALLOW" "1"
                    SOURCE_DIR          "/home/cseguret/Projects/app-ethereum/tests/unit/build/_deps/cmocka-src"
                    BINARY_DIR          "/home/cseguret/Projects/app-ethereum/tests/unit/build/_deps/cmocka-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
)
