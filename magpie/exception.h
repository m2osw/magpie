// Copyright (c) 2026  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/magpie
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

// libexcept
//
#include    <libexcept/exception.h>



namespace magpie
{



DECLARE_LOGIC_ERROR(logic_error);
DECLARE_LOGIC_ERROR(not_implemented);

DECLARE_OUT_OF_RANGE(out_of_range);

DECLARE_MAIN_EXCEPTION(magpie_exception);

DECLARE_EXCEPTION(magpie_exception, empty_key_not_accepted);
DECLARE_EXCEPTION(magpie_exception, incompatible_lock);
DECLARE_EXCEPTION(magpie_exception, incompatible_type);
DECLARE_EXCEPTION(magpie_exception, invalid_parameter);
DECLARE_EXCEPTION(magpie_exception, value_not_found);
DECLARE_EXCEPTION(magpie_exception, update_failed);



} // namespace magpie
// vim: ts=4 sw=4 et
