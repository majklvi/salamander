# SPDX-FileCopyrightText: 2026 Open Salamander Authors
# SPDX-License-Identifier: GPL-2.0-or-later
# Offline test fixture only. Never replace a language module in a running host.
param(
    [Parameter(Mandatory=$true)][string]$SourceLanguage,
    [Parameter(Mandatory=$true)][string]$ProbeLanguage
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path -LiteralPath $SourceLanguage).Path
$destination=[IO.Path]::GetFullPath($ProbeLanguage)
if($source -eq $destination -or (Test-Path -LiteralPath $destination)) {
    throw 'Use a new, separate probe file; the shipping module must remain untouched.'
}
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class DirSyncDialogResourceProbe {
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool FreeLibrary(IntPtr module);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr FindResourceW(IntPtr module, IntPtr name, IntPtr type);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint SizeofResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
    [DllImport("kernel32.dll")] static extern IntPtr LockResource(IntPtr resource);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern IntPtr BeginUpdateResourceW(string path, bool delete);
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool UpdateResourceW(IntPtr update, IntPtr type, IntPtr name, ushort language, byte[] data, uint size);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool EndUpdateResourceW(IntPtr update, bool discard);
    public static byte[] Read(string path) {
        IntPtr module=LoadLibraryExW(path,IntPtr.Zero,2);
        if(module==IntPtr.Zero) throw new Win32Exception();
        try {
            IntPtr resource=FindResourceW(module,(IntPtr)101,(IntPtr)5);
            if(resource==IntPtr.Zero) throw new Win32Exception();
            uint size=SizeofResource(module,resource);
            IntPtr bytes=LockResource(LoadResource(module,resource));
            if(size==0 || bytes==IntPtr.Zero) throw new Win32Exception();
            byte[] data=new byte[checked((int)size)];
            Marshal.Copy(bytes,data,0,data.Length);
            return data;
        } finally { FreeLibrary(module); }
    }
    public static void Prepare(string source,string destination) {
        byte[] data=Read(source);
        if(data.Length<18 || BitConverter.ToUInt16(data,0)!=1 || BitConverter.ToUInt16(data,2)!=65535)
            throw new Exception("Expected shipping extended dialog resource 101");
        uint style=BitConverter.ToUInt32(data,8);
        if((style & 0x40000)!=0) throw new Exception("Shipping dialog already has WS_EX_APPWINDOW");
        byte[] changed=BitConverter.GetBytes(style | 0x40000);
        Array.Copy(changed,0,data,8,4);
        System.IO.File.Copy(source,destination,false);
        IntPtr update=BeginUpdateResourceW(destination,false);
        if(update==IntPtr.Zero) throw new Win32Exception();
        if(!UpdateResourceW(update,(IntPtr)5,(IntPtr)101,0x409,data,(uint)data.Length)) {
            int error=Marshal.GetLastWin32Error(); EndUpdateResourceW(update,true); throw new Win32Exception(error);
        }
        if(!EndUpdateResourceW(update,false)) throw new Win32Exception();
        byte[] actual=Read(destination);
        if(actual.Length!=data.Length) throw new Exception("Probe resource size changed");
        for(int i=0;i<data.Length;++i) if(actual[i]!=data[i]) throw new Exception("Unexpected resource difference");
    }
}
'@
[DirSyncDialogResourceProbe]::Prepare($source,$destination)
Write-Output "Probe ready: $destination (only IDD_INFORMATION WS_EX_APPWINDOW added; do not ship)"
