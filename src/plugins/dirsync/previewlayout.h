// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <array>

namespace dirsync
{
inline std::array<RECT,5> FilterBounds(RECT anchor,RECT compare,RECT synchronize,int gap,int dx)
{
    // Use the actual pixel height: equal DU widths/heights need not be square
    // after the dialog font has been mapped at the current DPI.
    const int side=compare.bottom-compare.top;
    const int left=anchor.left+dx/2;
    const bool single=left+5*side+4*gap<=compare.left+dx-gap;
    std::array<RECT,5> bounds{};
    for(size_t i=0;i<bounds.size();++i) {
        int column=int(single?i:i%3);
        int top=single || i<3?compare.top:synchronize.top;
        int x=left+column*(side+gap);
        bounds[i]={x,top,x+side,top+side};
    }
    return bounds;
}
}
