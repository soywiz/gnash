// 
//   Copyright (C) 2007 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

/* $Id: ColorMatrixFilter.h,v 1.4 2007/12/29 20:15:25 strk Exp $ */

#ifndef GNASH_COLORMATRIXFILTER_H
#define GNASH_COLORMATRIXFILTER_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "BitmapFilter.h"
#include <vector>

namespace gnash {

// A color matrix effect filter.
class ColorMatrixFilter : public BitmapFilter
{
public:
    // Fill from a stream. See parser/filter_factory.cpp for the implementations.
    virtual bool read(stream& in);

    virtual ~ColorMatrixFilter() { return; }

    ColorMatrixFilter() : 
        m_matrix()
    { return; }

    ColorMatrixFilter(std::vector<float> a_matrix) :
        m_matrix(a_matrix)
    { return; }

protected:
    std::vector<float> m_matrix; // The color matrix
};

} // Namespace gnash

#endif // GNASH_CONVOLUTIONFILTER_H
