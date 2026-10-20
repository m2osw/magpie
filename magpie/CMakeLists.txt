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


##
## magpie library
##
project(magpie)

configure_file(
    version.h.in
    ${PROJECT_BINARY_DIR}/version.h
)

add_library(${PROJECT_NAME} SHARED
    cache_impl.cpp
    cleanup_runner.cpp
    eviction_policy.cpp
    lock_guard.cpp
    lock_interface.cpp
    lock_mutex.cpp
    lock_recursive_rwlock.cpp
    lock_rwlock.cpp
    options.cpp
    utils.cpp
    version.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CPPTHREAD_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${MURMUR3_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    ${CPPTHREAD_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${MURMUR3_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${MAGPIE_VERSION_MAJOR}.${MAGPIE_VERSION_MINOR}

    SOVERSION
        ${MAGPIE_VERSION_MAJOR}
)

install(
    TARGETS
        ${PROJECT_NAME}

    LIBRARY DESTINATION
        lib
)

# Most of the headers are private (namespace detail)
install(
    FILES
        cache.h
        cache_impl.h
        cache_ref.h
        constraint.h
        exception.h
        lock_interface.h
        lock_guard.h
        options.h
        ${CMAKE_CURRENT_BINARY_DIR}/version.h

    DESTINATION
        include/magpie
)


# vim: ts=4 sw=4 et
