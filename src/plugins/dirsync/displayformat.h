// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <windows.h>
#include <string>
#include <array>
#include <cstdint>
#include "../../filenamecase.h"

namespace dirsync
{
// Unsigned subtraction avoids overflow even at the ends of the metadata range.
inline uint64_t MetadataDifference(uint64_t a,uint64_t b) { return a>b?a-b:b-a; }
inline std::wstring RelativeValue(std::wstring pattern,const std::wstring& value)
{
    const auto at=pattern.find(L"{0}");
    if(at!=std::wstring::npos) pattern.replace(at,3,value);
    return pattern;
}
// FILETIME durations are elapsed time, independent of local timezone/DST.
inline std::wstring RelativeDuration(uint64_t ticks,const std::array<std::wstring,4>& units,
                                     const std::wstring& subsecond)
{
    uint64_t seconds=ticks/10000000;
    if(!seconds) return subsecond;
    constexpr std::array<uint64_t,4> divisors{86400,3600,60,1};
    std::wstring text;
    for(size_t i=0;i<divisors.size();++i) {
        const auto count=seconds/divisors[i];seconds%=divisors[i];
        if(count) {if(!text.empty()) text+=L" ";text+=std::to_wstring(count)+L" "+units[i];}
    }
    return text;
}
// Same selection as InternalGetSize in salamdr4.cpp. -1 means NumberToStr.
inline int PanelSizeMode(int format) { return format==1?3:format==2?0:-1; }

// Unicode fallback for names that cannot round-trip through the SDK's ACP API.
// Adapted from AlterFileName in salamdr2.cpp; reuse the core UTF-8 case helper,
// including its no-growth rule. The normal lossless path calls the SDK directly.
inline std::string UnicodeNameCase(std::string name,int format,bool directory,
                                   const BYTE* lower,const BYTE* upper)
{
    if(format==6) format=directory?3:2;
    if(name.empty() || format<1 || format>7 || format==4) return name;
    if(format==5) {
        size_t at=0;
        while(at<name.size() && at<8 && name[at]!='.' && BYTE(name[at])==upper[BYTE(name[at])]) ++at;
        if(at<name.size() && name[at]=='.') ++at;
        else if(at<name.size()) return name;
        size_t count=0;
        while(at<name.size() && count<3 && name[at]!='.' && BYTE(name[at])==upper[BYTE(name[at])]) {++at;++count;}
        if(at!=name.size()) return name;
    }
    const size_t dot=name.find_last_of('.');
    const size_t extension=dot==std::string::npos?name.size():dot+1;
    std::string result(name.size()+1,'\0');
    char* source=&name[0];char* target=&result[0];bool capital=true;
    while(*source) {
        const char separator=*source;
        bool toUpper=format==3 || ((format==1 || format==5 || format==7) && capital);
        if(format==7 && size_t(source-name.data())>=extension) toUpper=false;
        CopyFileNameCharWithCase(target,source,toUpper,TRUE,lower,upper);
        if(format==1 || format==5 || format==7) {
            if(capital) { if(separator!=' ' || (format==1 && separator=='.')) capital=false; }
            else if(separator==' ' || (format==1 && separator=='.')) capital=true;
        }
    }
    result.resize(size_t(target-result.data()));return result;
}
template<class FormatName>
std::wstring PanelRelativePath(const std::wstring& path,bool directory,int format,FormatName name)
{
    std::wstring text;
    for(size_t start=0;start<path.size();) {
        size_t end=path.find(L'\\',start);bool parent=end!=std::wstring::npos;
        if(!parent) end=path.size();
        text+=name(path.substr(start,end-start),format,parent || directory);
        if(parent) text+=L'\\';start=end+1;
    }
    if(directory) text+=L'\\';return text;
}
}
