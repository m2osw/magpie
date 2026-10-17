# - Find Magpie library
#
# MAGPIE_FOUND        - System has Magpie
# MAGPIE_INCLUDE_DIRS - The Magpie include directories
# MAGPIE_LIBRARIES    - The libraries needed to use Magpie
# MAGPIE_DEFINITIONS  - Compiler switches required for using Magpie
#
# License:
#
# Copyright (c) 2011-2026  Made to Order Software Corp.  All Rights Reserved
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

find_path(
    MAGPIE_INCLUDE_DIR
        magpie/magpie.h

    PATHS
        ENV MAGPIE_INCLUDE_DIR
)
find_library(
    MAGPIE_LIBRARY
        addr

    PATHS
        ${MAGPIE_LIBRARY_DIR}
        ENV MAGPIE_LIBRARY
)

mark_as_advanced(
    MAGPIE_INCLUDE_DIR
    MAGPIE_LIBRARY
)

set(MAGPIE_INCLUDE_DIRS ${MAGPIE_INCLUDE_DIR})
set(MAGPIE_LIBRARIES    ${MAGPIE_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Magpie
    REQUIRED_VARS
        MAGPIE_INCLUDE_DIR
        MAGPIE_LIBRARY
)

# vim: ts=4 sw=4 et
