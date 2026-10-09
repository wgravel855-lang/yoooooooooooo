#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <cstdint>
#include <string>
#include <vector>

// External process memory interface. Opens a handle to the target by image
// name, resolves the main module base/size, and provides typed RPM helpers
// plus a signature scanner for re-locating offsets after a patch.
class Memory {
public:
    HANDLE   proc   = nullptr;
    DWORD    pid    = 0;
    uintptr_t base  = 0;   // main module base address in the target
    size_t   modSize = 0;  // main module image size

    ~Memory() {
        if (proc) CloseHandle(proc);
    }

    bool attach(const wchar_t* imageName) {
        pid = findPid(imageName);
        if (!pid) return false;

        proc = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE |
                           PROCESS_QUERY_INFORMATION, FALSE, pid);
        if (!proc) return false;

        return resolveModule(imageName);
    }

    template <typename T>
    T read(uintptr_t addr) const {
        T value{};
        ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(addr),
                          &value, sizeof(T), nullptr);
        return value;
    }

    bool readRaw(uintptr_t addr, void* buf, size_t size) const {
        return ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(addr),
                                 buf, size, nullptr) != 0;
    }

    template <typename T>
    void write(uintptr_t addr, const T& value) const {
        WriteProcessMemory(proc, reinterpret_cast<LPVOID>(addr),
                           &value, sizeof(T), nullptr);
    }

    // Walk a pointer chain: base + first offset, deref, + next offset, ...
    // The final offset is added without a dereference, yielding the address
    // of the target field.
    uintptr_t chain(uintptr_t start, const std::vector<uintptr_t>& offsets) const {
        uintptr_t addr = start;
        for (size_t i = 0; i < offsets.size(); ++i) {
            if (i + 1 == offsets.size()) {
                addr += offsets[i];
            } else {
                addr = read<uintptr_t>(addr + offsets[i]);
                if (!addr) return 0;
            }
        }
        return addr;
    }

    // IDA-style signature scan over the main module. Pattern is a byte string
    // with "?" wildcards, e.g. "48 8B 05 ? ? ? ? 48 8B 88". Returns the
    // absolute address of the first match, or 0.
    uintptr_t findPattern(const char* pattern) const {
        std::vector<int> bytes = patternToBytes(pattern);
        std::vector<uint8_t> buf(modSize);
        if (!readRaw(base, buf.data(), modSize)) return 0;

        size_t pLen = bytes.size();
        for (size_t i = 0; i + pLen <= modSize; ++i) {
            bool found = true;
            for (size_t j = 0; j < pLen; ++j) {
                if (bytes[j] != -1 && buf[i + j] != bytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) return base + i;
        }
        return 0;
    }

    // Resolve a RIP-relative reference (e.g. `mov rax, [rip+disp]`).
    // `addr` points at the instruction, `offsetToDisp` is the byte offset of
    // the 4-byte displacement, `instrLen` is the full instruction length.
    uintptr_t resolveRip(uintptr_t addr, int offsetToDisp, int instrLen) const {
        int32_t disp = read<int32_t>(addr + offsetToDisp);
        return addr + instrLen + disp;
    }

private:
    static DWORD findPid(const wchar_t* imageName) {
        DWORD result = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        PROCESSENTRY32W pe{};
        pe.dwSize = sizeof(pe);
        if (Process32FirstW(snap, &pe)) {
            do {
                if (_wcsicmp(pe.szExeFile, imageName) == 0) {
                    result = pe.th32ProcessID;
                    break;
                }
            } while (Process32NextW(snap, &pe));
        }
        CloseHandle(snap);
        return result;
    }

    bool resolveModule(const wchar_t* imageName) {
        HANDLE snap = CreateToolhelp32Snapshot(
            TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return false;

        MODULEENTRY32W me{};
        me.dwSize = sizeof(me);
        bool ok = false;
        if (Module32FirstW(snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, imageName) == 0) {
                    base    = reinterpret_cast<uintptr_t>(me.modBaseAddr);
                    modSize = me.modBaseSize;
                    ok = true;
                    break;
                }
            } while (Module32NextW(snap, &me));
        }
        CloseHandle(snap);
        return ok;
    }

    static std::vector<int> patternToBytes(const char* pattern) {
        std::vector<int> bytes;
        const char* p = pattern;
        while (*p) {
            if (*p == ' ') { ++p; continue; }
            if (*p == '?') {
                bytes.push_back(-1);
                ++p;
                if (*p == '?') ++p;
            } else {
                bytes.push_back(static_cast<int>(strtol(p, nullptr, 16)));
                while (*p && *p != ' ') ++p;
            }
        }
        return bytes;
    }
};
