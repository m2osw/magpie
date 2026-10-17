# Copyright (c) 2026  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/magpie
# contact@m2osw.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.10.2)

project(magpie_project)

message("CMAKE_MODULE_PATH = [${CMAKE_MODULE_PATH}]")

#
# Include support modules
# (must be included in a project even though it is not made
# specific to that project)
#
find_package(CppThread        REQUIRED)
find_package(LibExcept        REQUIRED)
find_package(Murmur3          REQUIRED)
find_package(SnapCMakeModules REQUIRED)
find_package(SnapDev          REQUIRED)

SnapGetVersion(MAGPIE ${CMAKE_CURRENT_SOURCE_DIR})

enable_language(C)
enable_language(CXX)

include_directories(
    ${PROJECT_BINARY_DIR}
    ${PROJECT_SOURCE_DIR}
)

add_subdirectory(cmake)
add_subdirectory(magpie)
add_subdirectory(doc)
add_subdirectory(tests)

# vim: ts=4 sw=4 et
