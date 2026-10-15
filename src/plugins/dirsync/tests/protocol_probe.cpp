// SPDX-FileCopyrightText: 2026 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// Synthetic, manual NTFS experiment only: no engine, plugin or SDK dependency.
// Keeps all fixtures for inspection. Never accepts source/target paths from a journal.
#include <windows.h>
#include <bcrypt.h>
#include <sddl.h>
#include <winternl.h>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0, cases = 0;
struct Error { DWORD code; };
void Require(bool ok) { if (!ok) throw Error{GetLastError()}; }
void Check(bool ok, const char* what)
{ ++checks; if (!ok) throw std::runtime_error(what); }
struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) : h(value) {}
    ~Handle() { if (h != INVALID_HANDLE_VALUE && h != nullptr) CloseHandle(h); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
constexpr DWORD shared = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
HANDLE Open(const std::wstring& p, DWORD access, DWORD share = shared,
            DWORD disposition = OPEN_EXISTING, DWORD extra = 0)
{
    return CreateFileW(p.c_str(), access, share, nullptr, disposition,
                       FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS | extra, nullptr);
}
bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
void Dir(const std::wstring& p) { Require(CreateDirectoryW(p.c_str(), nullptr) != FALSE); }
void Write(HANDLE h, const std::string& data)
{
    DWORD n = 0; Require(WriteFile(h, data.data(), DWORD(data.size()), &n, nullptr) != FALSE);
    if (n != data.size()) throw Error{ERROR_WRITE_FAULT};
}
void Put(const std::wstring& p, const std::string& data, bool fresh = true)
{
    Handle h(Open(p, GENERIC_WRITE, shared, fresh ? CREATE_NEW : OPEN_EXISTING));
    Require(h.h != INVALID_HANDLE_VALUE); Write(h.h, data); Require(FlushFileBuffers(h.h) != FALSE);
}
std::string Read(HANDLE h)
{
    LARGE_INTEGER zero{}; Require(SetFilePointerEx(h, zero, nullptr, FILE_BEGIN) != FALSE);
    std::string out; char buf[4096]; DWORD n = 0;
    for (;;) { Require(ReadFile(h, buf, sizeof(buf), &n, nullptr) != FALSE); if (!n) return out; out.append(buf, n); }
}
std::string Read(const std::wstring& p)
{ Handle h(Open(p, GENERIC_READ)); Require(h.h != INVALID_HANDLE_VALUE); return Read(h.h); }
std::string Hash(const std::string& data)
{
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) throw Error{ERROR_GEN_FAILURE};
    unsigned char digest[32]{};
    NTSTATUS status = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
    if (status >= 0) status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), ULONG(data.size()), 0);
    if (status >= 0) status = BCryptFinishHash(hash, digest, sizeof(digest), 0);
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);
    if (status < 0) throw Error{ERROR_GEN_FAILURE};
    std::string result; const char* hex = "0123456789abcdef";
    for (unsigned char b : digest) { result += hex[b >> 4]; result += hex[b & 15]; }
    return result;
}
struct Id {
    DWORD volume = 0; uint64_t file = 0;
    bool operator==(const Id& b) const { return volume == b.volume && file == b.file; }
};
Id Identity(HANDLE h)
{
    BY_HANDLE_FILE_INFORMATION i{}; Require(GetFileInformationByHandle(h, &i) != FALSE);
    return {i.dwVolumeSerialNumber, (uint64_t(i.nFileIndexHigh) << 32) | i.nFileIndexLow};
}
Id Identity(const std::wstring& p)
{ Handle h(Open(p, FILE_READ_ATTRIBUTES)); Require(h.h != INVALID_HANDLE_VALUE); return Identity(h.h); }
std::string IdText(Id id) { return std::to_string(id.volume) + ":" + std::to_string(id.file); }
using Streams = std::map<std::wstring, std::string>;
Streams Contents(const std::wstring& p)
{
    Streams result; WIN32_FIND_STREAM_DATA data{};
    HANDLE f = FindFirstStreamW(p.c_str(), FindStreamInfoStandard, &data, 0);
    Require(f != INVALID_HANDLE_VALUE);
    DWORD error = ERROR_SUCCESS;
    do {
        std::wstring name = data.cStreamName;
        result[name] = Hash(Read(name == L"::$DATA" ? p : p + name));
    } while (FindNextStreamW(f, &data));
    error = GetLastError(); FindClose(f);
    if (error != ERROR_HANDLE_EOF) throw Error{error};
    return result;
}
std::vector<BYTE> Security(const std::wstring& p)
{
    DWORD n = 0; GetFileSecurityW(p.c_str(), DACL_SECURITY_INFORMATION, nullptr, 0, &n);
    Require(n != 0); std::vector<BYTE> buf(n);
    Require(GetFileSecurityW(p.c_str(), DACL_SECURITY_INFORMATION, buf.data(), n, &n) != FALSE);
    return buf;
}
std::wstring Dacl(const std::wstring& p)
{
    auto s = Security(p); LPWSTR text = nullptr;
    Require(ConvertSecurityDescriptorToStringSecurityDescriptorW(s.data(), SDDL_REVISION_1,
            DACL_SECURITY_INFORMATION, &text, nullptr) != FALSE);
    std::wstring result(text); LocalFree(text); return result;
}
void SetDacl(const std::wstring& p, const std::wstring& sddl)
{
    PSECURITY_DESCRIPTOR sd = nullptr;
    Require(ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &sd, nullptr) != FALSE);
    BOOL ok = SetFileSecurityW(p.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, sd);
    DWORD error = GetLastError(); LocalFree(sd); if (!ok) throw Error{error};
}
FILE_BASIC_INFO Basic(HANDLE h)
{ FILE_BASIC_INFO b{}; Require(GetFileInformationByHandleEx(h, FileBasicInfo, &b, sizeof(b)) != FALSE); return b; }
void Metadata(const std::wstring& p, FILE_BASIC_INFO b)
{
    Handle h(Open(p, FILE_WRITE_ATTRIBUTES)); Require(h.h != INVALID_HANDLE_VALUE);
    // Creation/access/change times deliberately not transported by D5.
    b.CreationTime.QuadPart = b.LastAccessTime.QuadPart = b.ChangeTime.QuadPart = 0;
    Require(SetFileInformationByHandle(h.h, FileBasicInfo, &b, sizeof(b)) != FALSE);
}
void Copy(const std::wstring& from, const std::wstring& to, HANDLE held, const std::string& fault,
          void (*half)(const std::string&) = nullptr, const std::string& phase = "")
{
    Handle out(Open(to, GENERIC_READ | GENERIC_WRITE | DELETE | WRITE_DAC, FILE_SHARE_READ, CREATE_NEW));
    Require(out.h != INVALID_HANDLE_VALUE);
    std::string data = Read(held);
    Write(out.h, data.substr(0, data.size() / 2));
    if (half) half(phase + "_HALF");
    if (fault == phase + "_STOP") throw Error{ERROR_CANCELLED};
    if (fault == phase + "_DISK_FULL") throw Error{ERROR_DISK_FULL}; // injected, not a full volume
    Write(out.h, data.substr(data.size() / 2));
    if (fault == phase + "_FLUSH") throw Error{ERROR_WRITE_FAULT}; // injected flush failure
    Require(FlushFileBuffers(out.h) != FALSE);
    // Named streams are copied independently, using the exact stream set from a stable fixture source.
    WIN32_FIND_STREAM_DATA stream{};
    HANDLE find = FindFirstStreamW(from.c_str(), FindStreamInfoStandard, &stream, 0);
    Require(find != INVALID_HANDLE_VALUE);
    DWORD error = 0;
    try {
        do {
            if (std::wstring(stream.cStreamName) != L"::$DATA")
                Put(to + stream.cStreamName, Read(from + stream.cStreamName));
        } while (FindNextStreamW(find, &stream));
        error = GetLastError();
    } catch (...) { FindClose(find); throw; }
    FindClose(find); if (error != ERROR_HANDLE_EOF) throw Error{error};
    if (fault == phase + "_VERIFY") {
        LARGE_INTEGER zero{}; Require(SetFilePointerEx(out.h, zero, nullptr, FILE_BEGIN) != FALSE);
        Write(out.h, "BAD!"); Require(FlushFileBuffers(out.h) != FALSE);
    }
    if (fault == phase + "_ADS_VERIFY")
        Put(to + (phase == "BACKUP" ? L":old" : L":fresh"), "BAD-ADS", false);
    // Read back through our handle: ordinary reopen would conflict with our DELETE access.
    if (Hash(Read(out.h)) != Hash(data)) throw Error{ERROR_CRC};
}
DWORD RenameRaw(HANDLE h, HANDLE parent, const std::wstring& name)
{
    std::vector<BYTE> buf(offsetof(FILE_RENAME_INFO, FileName) + (name.size() + 1) * sizeof(wchar_t));
    auto* r = reinterpret_cast<FILE_RENAME_INFO*>(buf.data());
    r->ReplaceIfExists = FALSE; r->RootDirectory = parent; r->FileNameLength = DWORD(name.size() * sizeof(wchar_t));
    std::copy(name.begin(), name.end(), r->FileName);
    return SetFileInformationByHandle(h, FileRenameInfo, r, DWORD(buf.size())) ? ERROR_SUCCESS : GetLastError();
}
DWORD Rename(HANDLE h, HANDLE parent, const std::wstring& name)
{
    // Explicit native relative candidate, never a silent fallback to weaker sharing.
    using SetInfo = NTSTATUS(NTAPI*)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
    using ToError = ULONG(NTAPI*)(NTSTATUS);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    auto setInfo = reinterpret_cast<SetInfo>(GetProcAddress(nt, "NtSetInformationFile"));
    auto toError = reinterpret_cast<ToError>(GetProcAddress(nt, "RtlNtStatusToDosError"));
    if (!setInfo || !toError) return ERROR_CALL_NOT_IMPLEMENTED;
    std::vector<BYTE> buf(offsetof(FILE_RENAME_INFO, FileName) + (name.size() + 1) * sizeof(wchar_t));
    auto* r = reinterpret_cast<FILE_RENAME_INFO*>(buf.data());
    r->ReplaceIfExists = FALSE; r->RootDirectory = parent; r->FileNameLength = DWORD(name.size() * sizeof(wchar_t));
    std::copy(name.begin(), name.end(), r->FileName);
    IO_STATUS_BLOCK io{};
    NTSTATUS status = setInfo(h, &io, r, ULONG(buf.size()), static_cast<FILE_INFORMATION_CLASS>(10));
    return status >= 0 ? ERROR_SUCCESS : toError(status);
}
// Experimental operational pins: sharing WRITE is necessary for rename on this NTFS.
// This weakens the read-only engine's parent contract; not approved for production.
HANDLE Parent(const std::wstring& p)
{ return Open(p, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE); }
std::wstring eventName, stopAt;
void Point(const std::string& state)
{
    if (std::wstring(state.begin(), state.end()) != stopAt) return;
    Handle e(OpenEventW(EVENT_MODIFY_STATE, FALSE, eventName.c_str())); Require(e.h != nullptr);
    Require(SetEvent(e.h) != FALSE); Sleep(INFINITE); // parent kills this process, no unwinding
}
struct Journal {
    Handle h; unsigned seq = 0; std::string failAt;
    Journal(const std::wstring& root, const std::string& fail) : h(Open(root + L"\\recovery\\journal.jsonl", GENERIC_WRITE, FILE_SHARE_READ, CREATE_NEW)), failAt(fail)
    { Require(h.h != INVALID_HANDLE_VALUE); }
    void Add(const std::string& state, Id old, Id stage)
    {
        if (state == failAt) throw Error{ERROR_WRITE_FAULT}; // injected unavailable durable log
        std::string payload = "{\"v\":1,\"seq\":" + std::to_string(++seq) + ",\"state\":\"" + state +
            "\",\"old\":\"" + IdText(old) + "\",\"stage\":\"" + IdText(stage) + "\"}";
        std::string record = "{\"bytes\":" + std::to_string(payload.size()) + ",\"sha256\":\"" + Hash(payload) +
            "\",\"record\":" + payload + "}\n";
        Write(h.h, record); Require(FlushFileBuffers(h.h) != FALSE); Point(state);
    }
};
std::wstring WorkPath(const std::wstring& root) { return root + L"\\target"; }
DWORD Worker(const std::wstring& root, const std::string& mode)
{
    try {
        const std::wstring dest = WorkPath(root), source = root + L"\\source\\source.bin";
        const std::wstring target = dest + L"\\item.bin", stage = dest + L"\\stage.bin", backup = root + L"\\recovery\\backup.bin";
        Handle parent(Parent(dest)), sourceParent(Parent(root + L"\\source")), recoveryParent(Parent(root + L"\\recovery"));
        Require(parent.h != INVALID_HANDLE_VALUE && sourceParent.h != INVALID_HANDLE_VALUE && recoveryParent.h != INVALID_HANDLE_VALUE);
        Handle src(Open(source, GENERIC_READ, FILE_SHARE_READ)); Require(src.h != INVALID_HANDLE_VALUE);
        Handle old(mode == "new" || mode == "new_collision" ? INVALID_HANDLE_VALUE : Open(target, GENERIC_READ | DELETE, FILE_SHARE_READ));
        if (mode != "new" && mode != "new_collision") Require(old.h != INVALID_HANDLE_VALUE);
        Id oldId{}, stageId{}; bool stopRequested = false;
        if (old.h != INVALID_HANDLE_VALUE) oldId = Identity(old.h);
        Journal log(root, mode == "log_after_old" ? "OLD_RELOCATED" : mode == "log_after_publish" ? "PUBLISHED" : "");
        log.Add("VALIDATED", oldId, stageId);
        if (old.h != INVALID_HANDLE_VALUE) {
            Copy(target, backup, old.h, mode, Point, "BACKUP");
            Metadata(backup, Basic(old.h));
            if (Contents(backup) != Contents(target) || Identity(backup) == oldId) throw Error{ERROR_CRC};
        }
        log.Add("BACKUP_READY", oldId, stageId);
        if (mode == "stop_backup") throw Error{ERROR_CANCELLED};
        if (mode != "delete") {
            Copy(source, stage, src.h, mode, Point, "STAGE");
            if (old.h != INVALID_HANDLE_VALUE) {
                auto security = Security(target);
                Require(SetFileSecurityW(stage.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, security.data()) != FALSE);
            }
            Metadata(stage, Basic(src.h));
            if (Contents(stage) != Contents(source)) throw Error{ERROR_CRC};
            stageId = Identity(stage);
        }
        log.Add("STAGED", oldId, stageId);
        if (mode == "stop_stage") throw Error{ERROR_CANCELLED};
        if (mode == "journal_flush") throw Error{ERROR_WRITE_FAULT};
        log.Add("COMMIT_INTENT", oldId, stageId);
        if (old.h != INVALID_HANDLE_VALUE) {
            DWORD err = Rename(old.h, parent.h, L"quarantine.bin"); if (err) throw Error{err};
            Point("UNLOGGED_OLD"); log.Add("OLD_RELOCATED", oldId, stageId);
            if (mode == "stop_commit") stopRequested = true; // injected cooperative Stop at this boundary
        }
        if (mode == "collision" || mode == "new_collision") Put(target, "FOREIGN");
        if (mode != "delete") {
            Handle staged(Open(stage, GENERIC_READ | DELETE | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ)); Require(staged.h != INVALID_HANDLE_VALUE);
            if (!(Identity(staged.h) == stageId)) throw Error{ERROR_FILE_INVALID};
            DWORD err = Rename(staged.h, parent.h, L"item.bin"); if (err) throw Error{err};
            // Rename can set Archive. Apply the intended final bits to the exact
            // published object, not a newly reopened path.
            auto finalBasic = Basic(src.h);
            finalBasic.CreationTime.QuadPart = finalBasic.LastAccessTime.QuadPart = finalBasic.ChangeTime.QuadPart = 0;
            Require(SetFileInformationByHandle(staged.h, FileBasicInfo, &finalBasic, sizeof(finalBasic)) != FALSE);
        }
        Point("UNLOGGED_PUBLISH"); log.Add("PUBLISHED", oldId, stageId);
        if (mode == "publish_verify") Put(target, "BAD!", false);
        if (mode != "delete" && (Contents(target) != Contents(source) || !(Identity(target) == stageId))) throw Error{ERROR_CRC};
        if (mode == "delete" && Exists(target)) throw Error{ERROR_FILE_EXISTS};
        log.Add("VERIFIED", oldId, stageId); log.Add("COMPLETE", oldId, stageId);
        // A synthetic Stop observed after OLD_RELOCATED is deferred until the
        // current short commit completes; there is no next operation in this probe.
        return stopRequested ? ERROR_CANCELLED : ERROR_SUCCESS;
    } catch (const Error& e) { return e.code; }
}
struct Fixture {
    std::wstring root; Id old{}; Streams source, original; std::wstring targetAcl; FILE_BASIC_INFO sourceBasic{};
    Fixture(const std::wstring& base, const std::wstring& name, bool absent = false) : root(base + L"\\" + name)
    {
        Dir(root); Dir(root + L"\\source"); Dir(root + L"\\target"); Dir(root + L"\\recovery");
        const auto s = root + L"\\source\\source.bin", t = root + L"\\target\\item.bin";
        Put(s, "NEW-DATA"); Put(s + L":fresh", "NEW-ADS"); Put(s + L":漢字", "UNICODE-ADS");
        // Distinct protected ACLs ensure we detect accidental propagation of source permissions.
        SetDacl(s, L"D:P(A;;FA;;;OW)(A;;FR;;;WD)");
        Handle src(Open(s, FILE_READ_ATTRIBUTES)); Require(src.h != INVALID_HANDLE_VALUE);
        sourceBasic = Basic(src.h); sourceBasic.LastWriteTime.QuadPart = 134050000000000000LL;
        sourceBasic.FileAttributes = FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE;
        Metadata(s, sourceBasic); source = Contents(s);
        if (!absent) {
            Put(t, "OLD-DATA"); Put(t + L":old", "OLD-ADS");
            SetDacl(t, L"D:P(A;;FA;;;OW)(A;;FR;;;BU)");
            old = Identity(t); original = Contents(t); targetAcl = Dacl(t);
        }
    }
};
std::wstring exe;
DWORD Run(const Fixture& f, const std::string& mode, const std::string& crash = "")
{
    std::wstring name = L"Local\\DirsyncProtocol-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++cases);
    Handle ready(CreateEventW(nullptr, TRUE, FALSE, name.c_str())); Require(ready.h != nullptr);
    std::wstring cmd = L"\"" + exe + L"\" --worker \"" + f.root + L"\" " + std::wstring(mode.begin(), mode.end()) +
        L" " + (crash.empty() ? L"-" : std::wstring(crash.begin(), crash.end())) + L" " + name;
    STARTUPINFOW si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
    Require(CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != FALSE);
    Handle process(pi.hProcess), thread(pi.hThread);
    if (!crash.empty()) {
        HANDLE events[]{ready.h, process.h}; DWORD wait = WaitForMultipleObjects(2, events, FALSE, 8000);
        if (wait != WAIT_OBJECT_0) {
            TerminateProcess(process.h, 250); WaitForSingleObject(process.h, 2000);
            throw std::runtime_error("worker did not reach crash boundary");
        }
        Require(TerminateProcess(process.h, 99) != FALSE);
    }
    DWORD wait = WaitForSingleObject(process.h, 8000);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process.h, 250); WaitForSingleObject(process.h, 2000); throw std::runtime_error("worker timeout"); }
    DWORD code = 0; Require(GetExitCodeProcess(process.h, &code) != FALSE);
    std::cout << "case " << cases << " " << mode << " " << crash << " exit=" << code << "\n";
    return code;
}
struct Recovery {
    std::string last, location, stage; bool damaged = false; unsigned records = 0;
    bool operator==(const Recovery& b) const
    { return last == b.last && location == b.location && stage == b.stage && damaged == b.damaged && records == b.records; }
};
Recovery Inspect(const Fixture& f)
{
    Recovery r; auto path = f.root + L"\\recovery\\journal.jsonl";
    std::string journal = Exists(path) ? Read(path) : "";
    size_t pos = 0;
    while (pos < journal.size()) {
        size_t end = journal.find('\n', pos);
        if (end == std::string::npos) { r.damaged = true; break; }
        std::string line = journal.substr(pos, end - pos); pos = end + 1;
        size_t payloadAt = line.find("\"record\":");
        if (payloadAt == std::string::npos || line.empty() || line.back() != '}') { r.damaged = true; break; }
        std::string payload = line.substr(payloadAt + 9, line.size() - payloadAt - 10);
        std::string prefix = "{\"bytes\":" + std::to_string(payload.size()) + ",\"sha256\":\"" + Hash(payload) + "\",\"record\":";
        std::string seq = "\"seq\":" + std::to_string(r.records + 1) + ",";
        size_t stateAt = payload.find("\"state\":\"");
        if (line != prefix + payload + "}" || payload.find(seq) == std::string::npos || stateAt == std::string::npos) { r.damaged = true; break; }
        stateAt += 9; size_t finish = payload.find('"', stateAt);
        if (finish == std::string::npos) { r.damaged = true; break; }
        r.last = payload.substr(stateAt, finish - stateAt); ++r.records;
        size_t stageAt = payload.find("\"stage\":\"");
        if (stageAt == std::string::npos) { r.damaged = true; break; }
        stageAt += 9; size_t stageEnd = payload.find('"', stageAt);
        if (stageEnd == std::string::npos) { r.damaged = true; break; }
        r.stage = payload.substr(stageAt, stageEnd - stageAt);
    }
    auto t = WorkPath(f.root) + L"\\item.bin", q = WorkPath(f.root) + L"\\quarantine.bin";
    if (!Exists(t)) r.location = "missing";
    else if (Identity(t) == f.old) r.location = "original";
    else if (IdText(Identity(t)) == r.stage && Contents(t) == f.source) r.location = "published-content";
    else r.location = "foreign-or-corrupt";
    if (Exists(q)) r.location += Identity(q) == f.old ? "+old-quarantine" : "+unknown-quarantine";
    return r;
}
std::string Inventory(const std::wstring& root)
{
    std::string result; WIN32_FIND_DATAW d{}; HANDLE find = FindFirstFileW((root + L"\\*").c_str(), &d);
    Require(find != INVALID_HANDLE_VALUE);
    std::vector<std::wstring> paths;
    do { if (wcscmp(d.cFileName, L".") && wcscmp(d.cFileName, L"..")) paths.push_back(d.cFileName); } while (FindNextFileW(find, &d));
    DWORD error = GetLastError(); FindClose(find); if (error != ERROR_NO_MORE_FILES) throw Error{error};
    std::sort(paths.begin(), paths.end());
    for (const auto& name : paths) {
        auto p = root + L"\\" + name;
        Handle h(Open(p, FILE_READ_ATTRIBUTES)); Require(h.h != INVALID_HANDLE_VALUE);
        auto basic = Basic(h.h);
        result += IdText(Identity(h.h)) + ":" + std::to_string(basic.LastWriteTime.QuadPart) + ":" +
                  std::to_string(basic.ChangeTime.QuadPart) + ":" + std::to_string(basic.FileAttributes);
        for (wchar_t c : name) result += std::to_string(unsigned(c)) + ",";
        if (basic.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) result += Inventory(p);
        else { for (const auto& s : Contents(p)) { for (wchar_t c : s.first) result += std::to_string(unsigned(c)) + ","; result += s.second; } }
    }
    return result;
}
Recovery RecoverTwice(const Fixture& f)
{
    std::string before = Inventory(f.root);
    Recovery a = Inspect(f), b = Inspect(f);
    Check(a == b, "recovery stable across repeated inspection");
    Check(before == Inventory(f.root), "read-only recovery changed fixture data or metadata");
    std::cout << "  recovery last=" << a.last << " actual=" << a.location << " damaged=" << a.damaged << "\n";
    return a;
}
void BackupCheck(const Fixture& f)
{
    auto backup = f.root + L"\\recovery\\backup.bin";
    Check(Exists(backup) && Contents(backup) == f.original, "complete independent backup streams");
    Check(!(Identity(backup) == f.old), "backup is not original hardlink");
}
void Success(const Fixture& f, const std::string& mode)
{
    Check(Run(f, mode) == 0, "successful publication/delete");
    auto r = RecoverTwice(f); Check(r.last == "COMPLETE" && !r.damaged, "complete journal");
    auto target = WorkPath(f.root) + L"\\item.bin";
    if (mode == "delete") Check(!Exists(target), "delete moved exact old object");
    else {
        Check(Contents(target) == f.source, "exact ADS set, no stale target stream");
        Handle h(Open(target, FILE_READ_ATTRIBUTES)); Require(h.h != INVALID_HANDLE_VALUE);
        auto basic = Basic(h.h);
        Check(basic.LastWriteTime.QuadPart == f.sourceBasic.LastWriteTime.QuadPart, "final mtime preserved");
        Check(basic.FileAttributes == f.sourceBasic.FileAttributes, "final basic attributes preserved");
        if (mode != "new") Check(Dacl(target) == f.targetAcl, "existing target DACL preserved");
        else {
            auto sibling = WorkPath(f.root) + L"\\acl-sibling"; Put(sibling, "ACL");
            Check(Dacl(sibling) == Dacl(target), "new target inherits destination ACL");
        }
        Check(Dacl(target) != Dacl(f.root + L"\\source\\source.bin"), "source DACL not propagated");
    }
    if (mode != "new") BackupCheck(f);
}
void PrimitiveTests(const std::wstring& base)
{
    std::wstring p = base + L"\\primitives"; Dir(p); Dir(p + L"\\parent");
    Handle parent(Open(p + L"\\parent", FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES, FILE_SHARE_READ)); Require(parent.h != INVALID_HANDLE_VALUE);
    Put(p + L"\\parent\\a", "A");
    Handle a(Open(p + L"\\parent\\a", GENERIC_READ | DELETE, FILE_SHARE_READ)); Require(a.h != INVALID_HANDLE_VALUE);
    Handle writer(Open(p + L"\\parent\\a", GENERIC_WRITE));
    Check(writer.h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION, "DELETE/read operational handle blocks writer");
    Handle rival(Open(p + L"\\parent\\a", DELETE));
    Check(rival.h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_SHARING_VIOLATION, "operational handle blocks rival rename/delete");
    Check(!MoveFileW((p + L"\\parent").c_str(), (p + L"\\moved").c_str()), "parent list pin blocks rename");
    DWORD relative = RenameRaw(a.h, parent.h, L"relative");
    std::cout << "relative-handle-rename error=" << relative << "\n";
    Check(relative == ERROR_INVALID_PARAMETER, "relative API capability changed; review candidate");
    DWORD strict = Rename(a.h, parent.h, L"renamed");
    std::cout << "native-relative-strict-parent error=" << strict << "\n";
    Check(strict == ERROR_SHARING_VIOLATION, "strict parent prevents native rename on this volume");
    CloseHandle(parent.h); parent.h = Parent(p + L"\\parent"); Require(parent.h != INVALID_HANDLE_VALUE);
    Check(!MoveFileW((p + L"\\parent").c_str(), (p + L"\\moved").c_str()), "experimental parent still excludes rename/delete");
    DWORD rename = Rename(a.h, parent.h, L"renamed");
    std::cout << "native-relative-write-sharing-parent error=" << rename << "\n";
    Check(rename == 0 && Exists(p + L"\\parent\\renamed"), "native relative rename with experimental parent sharing");
    Put(p + L"\\parent\\collision", "FOREIGN");
    DWORD collision = Rename(a.h, parent.h, L"collision");
    Check(collision != 0 && Read(p + L"\\parent\\collision") == "FOREIGN", "rename no-replace collision");
    Put(p + L"\\mapped", "MAP-DATA");
    Handle file(Open(p + L"\\mapped", GENERIC_READ | GENERIC_WRITE)); Require(file.h != INVALID_HANDLE_VALUE);
    Handle map(CreateFileMappingW(file.h, nullptr, PAGE_READWRITE, 0, 0, nullptr)); Require(map.h != nullptr);
    auto view = static_cast<char*>(MapViewOfFile(map.h, FILE_MAP_WRITE, 0, 0, 0)); Require(view != nullptr);
    CloseHandle(file.h); file.h = INVALID_HANDLE_VALUE;
    Handle stable(Open(p + L"\\mapped", GENERIC_READ, FILE_SHARE_READ));
    bool admitted = stable.h != INVALID_HANDLE_VALUE;
    DWORD mappingError = admitted ? ERROR_SUCCESS : GetLastError();
    if (admitted) {
        view[0] = 'X'; Require(FlushViewOfFile(view, 0) != FALSE);
        Check(Read(stable.h) == "XAP-DATA", "preexisting writable view can change held source");
    }
    UnmapViewOfFile(view);
    std::cout << "preexisting-writable-mapping admitted=" << admitted << " error=" << mappingError << "\n";
    // This is a capability observation, not evidence that read sharing stabilizes all mappings.
    Check(!admitted && mappingError == ERROR_SHARING_VIOLATION, "tested writable mapping denied by source sharing");
    Put(p + L"\\streams", "BASE"); Put(p + L"\\streams:existing", "OLD-ADS");
    {
        Handle held(Open(p + L"\\streams", GENERIC_READ, FILE_SHARE_READ)); Require(held.h != INVALID_HANDLE_VALUE);
        Put(p + L"\\streams:existing", "NEW-ADS", false); Put(p + L"\\streams:added", "ADDED");
        Check(Read(p + L"\\streams:existing") == "NEW-ADS" && Exists(p + L"\\streams:added"), "main-stream read pin does not freeze ADS");
        std::cout << "main-stream-pin allowed existing ADS write and new ADS creation\n";
    }
    {
        // WRITE sharing on a directory admits a metadata writer. This is deliberately
        // a negative result for the proposed immutable-parent guarantee.
        Handle other(Open(p + L"\\parent", FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE));
        Check(other.h != INVALID_HANDLE_VALUE, "experimental parent sharing admits attributes writer");
        auto b = Basic(other.h); b.FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
        Require(SetFileInformationByHandle(other.h, FileBasicInfo, &b, sizeof(b)) != FALSE);
        Check((GetFileAttributesW((p + L"\\parent").c_str()) & FILE_ATTRIBUTE_HIDDEN) != 0, "parent attributes actually mutable with experimental sharing");
    }
    Dir(p + L"\\empty"); Dir(p + L"\\nonempty"); Put(p + L"\\nonempty\\child", "KEEP");
    {
        Handle empty(Open(p + L"\\empty", DELETE, FILE_SHARE_READ)); Require(empty.h != INVALID_HANDLE_VALUE);
        FILE_DISPOSITION_INFO info{TRUE};
        Check(SetFileInformationByHandle(empty.h, FileDispositionInfo, &info, sizeof(info)) != FALSE, "empty directory handle disposition");
    }
    Check(!Exists(p + L"\\empty"), "empty dir removed after handle closes");
    Dir(p + L"\\empty-race");
    {
        Handle raced(Open(p + L"\\empty-race", DELETE, FILE_SHARE_READ)); Require(raced.h != INVALID_HANDLE_VALUE);
        Put(p + L"\\empty-race\\late-child", "FOREIGN");
        FILE_DISPOSITION_INFO info{TRUE};
        BOOL ok = SetFileInformationByHandle(raced.h, FileDispositionInfo, &info, sizeof(info)); DWORD error = GetLastError();
        Check(!ok && error == ERROR_DIR_NOT_EMPTY, "child created after directory open prevents deletion");
    }
    Check(Read(p + L"\\empty-race\\late-child") == "FOREIGN", "late child survives delete race");
    {
        Handle nonempty(Open(p + L"\\nonempty", DELETE, FILE_SHARE_READ)); Require(nonempty.h != INVALID_HANDLE_VALUE);
        FILE_DISPOSITION_INFO info{TRUE};
        BOOL ok = SetFileInformationByHandle(nonempty.h, FileDispositionInfo, &info, sizeof(info)); DWORD error = GetLastError();
        Check(!ok && error == ERROR_DIR_NOT_EMPTY, "nonempty directory disposition fails safely");
    }
    Check(Read(p + L"\\nonempty\\child") == "KEEP", "nonempty child preserved");
    Check(!CreateDirectoryW((p + L"\\nonempty").c_str(), nullptr) && GetLastError() == ERROR_ALREADY_EXISTS, "directory create collision");
    Dir(p + L"\\created");
    Check(Identity(p + L"\\created").file != 0, "new directory identity obtainable");
}
void Suite(const std::wstring& base)
{
    PrimitiveTests(base);
    Fixture normal(base, L"overwrite");
    Require(CreateHardLinkW((normal.root + L"\\old-alias").c_str(), (WorkPath(normal.root) + L"\\item.bin").c_str(), nullptr) != FALSE);
    DWORD aliasAttributes = GetFileAttributesW((normal.root + L"\\old-alias").c_str());
    Success(normal, "normal");
    Check(Contents(normal.root + L"\\old-alias") == normal.original, "overwrite preserves original hardlink data and ADS");
    Check(GetFileAttributesW((normal.root + L"\\old-alias").c_str()) == aliasAttributes, "tested original hardlink attributes unchanged");
    // Independence remains true even if the old object's other hardlink changes after handles close.
    Put(normal.root + L"\\old-alias", "CHANGED!", false); BackupCheck(normal);
    {
        // Model an explicit manual restore to a fresh sibling, never an automatic rollback.
        auto backup = normal.root + L"\\recovery\\backup.bin", restored = normal.root + L"\\manual-restore.bin";
        Handle saved(Open(backup, GENERIC_READ, FILE_SHARE_READ)); Require(saved.h != INVALID_HANDLE_VALUE);
        Copy(backup, restored, saved.h, ""); Metadata(restored, Basic(saved.h));
        Check(Contents(restored) == normal.original, "manual restore recovers old main data and ADS");
        Check(!(Identity(restored) == Identity(saved.h)), "manual restore is independent of backup");
        Check(Contents(WorkPath(normal.root) + L"\\item.bin") == normal.source, "manual restore does not roll back published target");
        auto occupied = normal.root + L"\\occupied-restore.bin"; Put(occupied, "FOREIGN");
        DWORD error = ERROR_SUCCESS;
        try { Copy(backup, occupied, saved.h, ""); } catch (const Error& e) { error = e.code; }
        Check(error == ERROR_FILE_EXISTS && Read(occupied) == "FOREIGN", "manual restore collision preserves foreign object");
    }
    Fixture fresh(base, L"new", true); Success(fresh, "new");
    Fixture del(base, L"delete"); Success(del, "delete");
    Fixture attributes(base, L"source-attributes");
    attributes.sourceBasic.FileAttributes = FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN;
    Metadata(attributes.root + L"\\source\\source.bin", attributes.sourceBasic);
    Success(attributes, "normal");
    Fixture stopCommit(base, L"stop-in-commit");
    Check(Run(stopCommit, "stop_commit") == ERROR_CANCELLED, "cooperative stop reported after current commit");
    Check(RecoverTwice(stopCommit).last == "COMPLETE", "cooperative stop finishes current publication");
    Check(Contents(WorkPath(stopCommit.root) + L"\\item.bin") == stopCommit.source, "stopped run records completed current operation");
    BackupCheck(stopCommit);
    const char* points[]{"VALIDATED", "BACKUP_HALF", "BACKUP_READY", "STAGE_HALF", "STAGED", "COMMIT_INTENT",
        "UNLOGGED_OLD", "OLD_RELOCATED", "UNLOGGED_PUBLISH", "PUBLISHED", "VERIFIED", "COMPLETE"};
    for (const char* point : points) {
        std::string s(point); Fixture f(base, L"crash-" + std::wstring(s.begin(), s.end()));
        Check(Run(f, "normal", s) == 99, "forced process termination exit");
        auto r = RecoverTwice(f);
        if (s == "VALIDATED" || s == "BACKUP_HALF" || s == "BACKUP_READY" || s == "STAGE_HALF" || s == "STAGED" || s == "COMMIT_INTENT")
            Check(r.location == "original", "pre-rename crash preserves old namespace");
        else if (s == "UNLOGGED_OLD" || s == "OLD_RELOCATED") Check(r.location == "missing+old-quarantine", "rename gap discovered from filesystem");
        else Check(r.location == "published-content+old-quarantine", "published state discovered without relying on log");
        if (s != "VALIDATED" && s != "BACKUP_HALF") BackupCheck(f);
        Check(!r.damaged, "flushed records intact after process termination");
    }
    for (const std::string mode : {"new", "delete"}) {
        const char* extra[]{"BACKUP_READY", "STAGED", "COMMIT_INTENT", "UNLOGGED_PUBLISH", "PUBLISHED", "COMPLETE"};
        for (const char* point : extra) {
            std::string label = mode + "-crash-" + point;
            Fixture f(base, std::wstring(label.begin(), label.end()), mode == "new");
            Check(Run(f, mode, point) == 99, "new/delete crash boundary");
            auto r = RecoverTwice(f);
            bool committed = std::string(point) == "UNLOGGED_PUBLISH" || std::string(point) == "PUBLISHED" || std::string(point) == "COMPLETE";
            Check(r.location == (mode == "new" ? (committed ? "published-content" : "missing") :
                (committed ? "missing+old-quarantine" : "original")), "new/delete recovery namespace");
            if (mode == "delete") BackupCheck(f);
        }
    }
    struct Fault { const char* mode; DWORD error; } faults[]{
        {"BACKUP_DISK_FULL", ERROR_DISK_FULL}, {"STAGE_DISK_FULL", ERROR_DISK_FULL},
        {"BACKUP_FLUSH", ERROR_WRITE_FAULT}, {"STAGE_FLUSH", ERROR_WRITE_FAULT}, {"journal_flush", ERROR_WRITE_FAULT},
        {"BACKUP_VERIFY", ERROR_CRC}, {"STAGE_VERIFY", ERROR_CRC},
        {"BACKUP_ADS_VERIFY", ERROR_CRC}, {"STAGE_ADS_VERIFY", ERROR_CRC},
        {"BACKUP_STOP", ERROR_CANCELLED}, {"STAGE_STOP", ERROR_CANCELLED},
        {"stop_backup", ERROR_CANCELLED}, {"stop_stage", ERROR_CANCELLED}};
    for (const auto& fault : faults) {
        std::string mode(fault.mode); Fixture f(base, L"fault-" + std::wstring(mode.begin(), mode.end()));
        Check(Run(f, mode) == fault.error, "expected injected failure");
        Check(RecoverTwice(f).location == "original", "failure must not publish or relocate original");
        Check(Contents(WorkPath(f.root) + L"\\item.bin") == f.original, "old data and ADS survive failure");
    }
    for (const std::string mode : {"collision", "new_collision", "publish_verify"}) {
        Fixture f(base, std::wstring(mode.begin(), mode.end()), mode == "new_collision");
        DWORD error = Run(f, mode); Check(error != 0, "post-intent error not reported as success");
        auto r = RecoverTwice(f); Check(r.location.find("foreign-or-corrupt") == 0, "foreign or corrupt target left for manual handling");
        if (mode != "new_collision") BackupCheck(f);
        if (mode != "publish_verify") Check(Read(WorkPath(f.root) + L"\\item.bin") == "FOREIGN", "foreign collision not overwritten");
    }
    for (const std::string mode : {"log_after_old", "log_after_publish"}) {
        Fixture f(base, std::wstring(mode.begin(), mode.end()));
        Check(Run(f, mode) == ERROR_WRITE_FAULT, "post-rename journal error reported");
        auto r = RecoverTwice(f);
        Check(r.location == (mode == "log_after_old" ? "missing+old-quarantine" : "published-content+old-quarantine"), "journal failure resolved from actual IDs");
        Check(r.last == (mode == "log_after_old" ? "COMMIT_INTENT" : "OLD_RELOCATED"), "journal did not claim missing durable state");
        BackupCheck(f);
    }
    Fixture quarantine(base, L"quarantine-collision"); Put(WorkPath(quarantine.root) + L"\\quarantine.bin", "FOREIGN");
    Check(Run(quarantine, "normal") != 0, "quarantine create collision blocks first rename");
    Check(Identity(WorkPath(quarantine.root) + L"\\item.bin") == quarantine.old, "quarantine collision retains original");
    Check(Read(WorkPath(quarantine.root) + L"\\quarantine.bin") == "FOREIGN", "foreign quarantine not overwritten");
    Fixture readonly(base, L"readonly-target");
    auto readonlyPath = WorkPath(readonly.root) + L"\\item.bin";
    Require(SetFileAttributesW(readonlyPath.c_str(), FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_ARCHIVE) != FALSE);
    Check(Run(readonly, "normal") == ERROR_SUCCESS, "read-only target can be renamed by held DELETE handle");
    BackupCheck(readonly);
    Check((GetFileAttributesW((readonly.root + L"\\recovery\\backup.bin").c_str()) & FILE_ATTRIBUTE_READONLY) != 0,
          "read-only flag retained in recovery backup");
    Check(Contents(readonlyPath) == readonly.source, "read-only is not an execution authorization barrier");
    Fixture denied(base, L"access-denied");
    auto dest = denied.root + L"\\recovery"; auto saved = Security(dest);
    SetDacl(dest, L"D:P(D;;FW;;;WD)(A;;FA;;;OW)");
    DWORD deniedCode = Run(denied, "normal");
    Require(SetFileSecurityW(dest.c_str(), DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, saved.data()) != FALSE);
    Check(deniedCode == ERROR_ACCESS_DENIED, "actual ACL denied journal creation");
    Check(Identity(WorkPath(denied.root) + L"\\item.bin") == denied.old, "denied recovery leaves target intact");
    Fixture sharing(base, L"sharing");
    {
        Handle busy(Open(WorkPath(sharing.root) + L"\\item.bin", GENERIC_READ, FILE_SHARE_READ)); Require(busy.h != INVALID_HANDLE_VALUE);
        Check(Run(sharing, "normal") == ERROR_SHARING_VIOLATION, "actual sharing violation before backup");
    }
    Check(RecoverTwice(sharing).location == "original", "sharing conflict preserves target");
    Fixture corrupt(base, L"damaged-journal"); Check(Run(corrupt, "normal", "UNLOGGED_OLD") == 99, "crash before old-state log");
    auto log = corrupt.root + L"\\recovery\\journal.jsonl";
    { Handle h(Open(log, FILE_APPEND_DATA)); Require(h.h != INVALID_HANDLE_VALUE); Write(h.h, "{\"bytes\":123"); Require(FlushFileBuffers(h.h) != FALSE); }
    Check(RecoverTwice(corrupt).damaged, "truncated record detected without writes");
    Put(log, "X", false);
    Check(RecoverTwice(corrupt).damaged, "corrupt record detected without writes");
    std::wstring deep = base;
    for (int i = 0; i < 8; ++i) { deep += L"\\漢字-žluťoučký-long-component-0123456789"; Dir(deep); }
    Fixture unicode(deep, L"emoji-😀"); Success(unicode, "normal");
    Check(unicode.root.size() > 260, "actual long UTF16 fixture path");
}
void NewRoot(const std::wstring& raw)
{
    // A fresh absolute local directory only. Never scan, mutate or clean an existing tree.
    if (raw.size() < 4 || raw[1] != L':' || raw[2] != L'\\' || raw.find(L"..") != std::wstring::npos ||
        raw.find_first_of(L"\"/*?") != std::wstring::npos || raw.find(L':', 2) != std::wstring::npos)
        throw std::runtime_error("expected a new absolute local fixture directory");
    auto drive = raw.substr(0, 3); wchar_t fs[32]{};
    Require(GetVolumeInformationW(drive.c_str(), nullptr, 0, nullptr, nullptr, nullptr, fs, DWORD(std::size(fs))) != FALSE);
    Check(GetDriveTypeW(drive.c_str()) == DRIVE_FIXED && std::wstring(fs) == L"NTFS", "fixed local NTFS required");
    size_t at = 3;
    while ((at = raw.find(L'\\', at)) != std::wstring::npos) {
        auto p = raw.substr(0, at++); DWORD attrs = GetFileAttributesW(p.c_str());
        Check(attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) && !(attrs & FILE_ATTRIBUTE_REPARSE_POINT), "existing non-reparse fixture ancestor");
    }
    Dir(L"\\\\?\\" + raw); // CREATE_NEW directory semantics: an existing leaf aborts.
}
} // namespace
int wmain(int argc, wchar_t** argv)
{
    try {
        if (argc == 6 && std::wstring(argv[1]) == L"--worker") {
            std::wstring root(argv[2]);
            // Worker can only be entered through a named event made by our parent harness.
            eventName = argv[5]; stopAt = argv[4];
            if (root.rfind(L"\\\\?\\C:\\Samandarin\\builds\\dirsync\\protocol-", 0) != 0 ||
                root.find(L"..") != std::wstring::npos || root.find(L':', 6) != std::wstring::npos ||
                eventName.rfind(L"Local\\DirsyncProtocol-", 0) != 0) return ERROR_INVALID_PARAMETER;
            Handle parentEvent(OpenEventW(SYNCHRONIZE, FALSE, eventName.c_str()));
            if (parentEvent.h == nullptr) return ERROR_ACCESS_DENIED;
            std::string mode;
            for (const wchar_t* c = argv[3]; *c; ++c) {
                if (*c > 127) return ERROR_INVALID_PARAMETER;
                mode += static_cast<char>(*c);
            }
            return int(Worker(root, mode));
        }
        if (argc != 2) { std::cerr << "usage: protocol_probe.exe C:\\Samandarin\\builds\\dirsync\\protocol-<new-run>\n"; return 2; }
        std::wstring raw(argv[1]);
        if (raw.rfind(L"C:\\Samandarin\\builds\\dirsync\\protocol-", 0) != 0) throw std::runtime_error("fixture must be under the dedicated protocol- path");
        NewRoot(raw);
        std::vector<wchar_t> module(32768); DWORD n = GetModuleFileNameW(nullptr, module.data(), DWORD(module.size()));
        Require(n != 0 && n < module.size()); exe.assign(module.data(), n);
        Suite(L"\\\\?\\" + raw);
        std::cout << "PASS " << checks << " assertions, " << cases << " child runs; fixtures retained\n"; return 0;
    } catch (const Error& e) { std::cerr << "Win32 failure " << e.code << ", checks=" << checks << "\n"; return 1; }
      catch (const std::exception& e) { std::cerr << "FAIL " << e.what() << ", checks=" << checks << "\n"; return 1; }
}
