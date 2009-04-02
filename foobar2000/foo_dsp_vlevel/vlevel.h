// Copyright 2003 Tom Felker
//
// This file is part of VLevel.
//
// VLevel is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// VLevel is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VLevel; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

// vlevel.h - included by all

#ifndef VLEVEL_H
#define VLEVEL_H

// Float actually is slightly less accurate (900/65536 in my tests),
// but is needed by LADSPA.

typedef float value_t;

#define VLEVEL_ABS(x) fabsf(x)

// same speed as above
//#define VLEVEL_ABS(x) ((x) > 0 ? (x) : -(x))

// a bit faster on 2.96, a bit slower(!) on 3.2
//#define VLEVEL_ABS(x) (x)

#endif // ndef VLEVEL_H
