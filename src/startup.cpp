#include "startup.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <mutex>

#include <windows.h>

namespace {

std::mutex& LogMutex() {
    static std::mutex mutex;
    return mutex;
}

std::string ToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                        nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size,
                        nullptr, nullptr);
    return out;
}

std::filesystem::path FailureMarkerPath() {
    return Startup::LogPath().parent_path() / "window-init-failed.flag";
}

} // namespace

namespace Startup {

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                        nullptr, 0);
    if (size <= 0) return {};
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

std::filesystem::path ExecutablePath() {
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) return {};
    return std::filesystem::path(std::wstring(buffer, length));
}

std::filesystem::path LogPath() {
    wchar_t buffer[MAX_PATH * 2] = {};
    const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer,
                                                 static_cast<DWORD>(std::size(buffer)));
    std::filesystem::path base;
    if (length > 0 && length < std::size(buffer)) {
        base = std::filesystem::path(std::wstring(buffer, length));
    } else {
        base = ExecutablePath().parent_path();
        if (base.empty()) base = std::filesystem::current_path();
    }
    return base / "XPEBot" / "logs" / "startup.log";
}

void Log(const std::string& message) {
    SYSTEMTIME now{};
    GetLocalTime(&now);
    wchar_t stamp[64] = {};
    swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u", now.wYear, now.wMonth, now.wDay,
               now.wHour, now.wMinute, now.wSecond);
    const std::wstring wide = std::wstring(stamp) + L" " + Widen(message) + L"\r\n";

    {
        std::lock_guard<std::mutex> lock(LogMutex());
        const auto path = LogPath();
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::app | std::ios::binary);
        if (out) {
            const std::string utf8 = ToUtf8(wide);
            out.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
            out.flush();
        }
    }
    OutputDebugStringW(wide.c_str());
}

void ShowError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"XPE Bot",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TOPMOST);
}

void MarkWindowFailure() {
    const auto path = FailureMarkerPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::trunc);
    if (out) out << "1";
}

void ClearWindowFailure() {
    std::error_code ec;
    std::filesystem::remove(FailureMarkerPath(), ec);
}

bool WindowFailedBefore() {
    std::error_code ec;
    return std::filesystem::exists(FailureMarkerPath(), ec);
}

} // namespace Startup
