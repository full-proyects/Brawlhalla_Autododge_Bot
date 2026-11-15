#include "Updater.h"

#include "config.h"
#include "gui.h"
#include "logger.h"
#include "paths.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

#include "GameVersionDetector.h"
#include "json.hpp"

#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif

namespace {

constexpr const char* kDefaultRepo = "full-proyects/Brawlhalla_Autododge_Bot";

// Zip pendiente de instalar. Solo lo toca el hilo del updater.
std::filesystem::path g_pendingZip;
std::string g_pendingVersion;

std::string Narrow(const std::wstring& text) {
    if (text.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), size);
    return out;
}

int DigitsToInt(const std::string& digits) {
    long long value = 0;
    for (const char c : digits) {
        if (c < '0' || c > '9') break;
        value = value * 10 + (c - '0');
        if (value > 1000000) break;
    }
    return static_cast<int>(value);
}

std::vector<int> ParseVersion(const std::string& version) {
    std::vector<int> parts;
    std::string digits;
    for (const char c : version) {
        if (c >= '0' && c <= '9') {
            digits.push_back(c);
            continue;
        }
        if (c == '.' || c == '-') {
            parts.push_back(DigitsToInt(digits));
            digits.clear();
            if (c == '-') break;
            continue;
        }
        break;
    }
    if (!digits.empty()) parts.push_back(DigitsToInt(digits));
    while (parts.size() < 4) parts.push_back(0);
    return parts;
}

bool EndsWithZip(const std::string& name) {
    if (name.size() < 4) return false;
    std::string tail = name.substr(name.size() - 4);
    std::transform(tail.begin(), tail.end(), tail.begin(), [](unsigned char c) { return static_cast<char>(::tolower(c)); });
    return tail == ".zip";
}

struct Response {
    DWORD status = 0;
    std::string body;
    std::string location;
};

bool HttpOnce(const std::wstring& url, Response& response, const std::filesystem::path& saveTo, std::string* error) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    wchar_t host[256]{};
    wchar_t objectPath[4096]{};
    parts.lpszHostName = host;
    parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
    parts.lpszUrlPath = objectPath;
    parts.dwUrlPathLength = static_cast<DWORD>(std::size(objectPath));
    parts.dwSchemeLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        if (error) *error = "URL invalida";
        return false;
    }

    const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    const std::wstring path = (parts.dwUrlPathLength > 0) ? std::wstring(objectPath, parts.dwUrlPathLength) : std::wstring(L"/");
    const std::wstring query = (parts.dwExtraInfoLength > 0 && parts.lpszExtraInfo)
                                   ? std::wstring(parts.lpszExtraInfo, parts.dwExtraInfoLength)
                                   : std::wstring();
    const std::wstring target = path + query;

    HINTERNET session = WinHttpOpen(L"XPEBot-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        if (error) *error = "WinHttpOpen fallo";
        return false;
    }
    WinHttpSetTimeouts(session, 10000, 10000, 20000, 60000);

    bool ok = false;
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connection) {
        if (error) *error = "Sin conexion al servidor";
        WinHttpCloseHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr,
                                           WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request) {
        if (error) *error = "No se pudo crear la peticion HTTP";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* headers =
        L"User-Agent: XPEBot-Updater/1.0\r\n"
        L"Accept: application/vnd.github+json, application/octet-stream\r\n"
        L"Cache-Control: no-cache\r\n";

    if (!WinHttpSendRequest(request, headers, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request, nullptr)) {
        if (error) *error = "Fallo de red o TLS al contactar GitHub";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD status = 0;
    DWORD statusSize = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX);
    response.status = status;

    if (status >= 300 && status < 400) {
        wchar_t location[4096]{};
        DWORD locationSize = sizeof(location);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                                location, &locationSize, WINHTTP_NO_HEADER_INDEX)) {
            response.location = Narrow(location);
        }
        ok = !response.location.empty();
        if (!ok && error) *error = "Redireccion sin destino";
    } else if (status >= 400) {
        if (error) *error = "HTTP " + std::to_string(status);
    } else {
        std::ofstream file;
        if (!saveTo.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(saveTo.parent_path(), ec);
            file.open(saveTo, std::ios::binary | std::ios::trunc);
            if (!file) {
                if (error) *error = "No se pudo escribir el paquete en disco";
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return false;
            }
        }

        std::vector<char> buffer(64 * 1024);
        bool readOk = true;
        for (;;) {
            DWORD read = 0;
            if (!WinHttpReadData(request, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
                readOk = false;
                break;
            }
            if (read == 0) break;
            if (file.is_open()) {
                file.write(buffer.data(), static_cast<std::streamsize>(read));
            } else {
                response.body.append(buffer.data(), read);
            }
        }
        if (file.is_open()) file.close();
        ok = readOk;
        if (!readOk && error) *error = "Corte durante la descarga";
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return ok;
}

bool HttpGet(const std::wstring& url, Response& response, const std::filesystem::path& saveTo, std::string* error) {
    std::wstring current = url;
    for (int hop = 0; hop < 6; ++hop) {
        Response attempt;
        if (!HttpOnce(current, attempt, saveTo, error)) return false;
        if (attempt.status < 300 || attempt.status >= 400) {
            response = std::move(attempt);
            return true;
        }
        if (attempt.location.empty()) {
            if (error) *error = "Redireccion invalida";
            return false;
        }
        if (attempt.location.rfind("http", 0) == 0) {
            current = Widen(attempt.location);
        } else if (!attempt.location.empty() && attempt.location.front() == '/') {
            URL_COMPONENTS schemeParts{};
            schemeParts.dwStructSize = sizeof(schemeParts);
            schemeParts.dwSchemeLength = static_cast<DWORD>(-1);
            schemeParts.dwHostNameLength = static_cast<DWORD>(-1);
            wchar_t scheme[16]{};
            schemeParts.lpszScheme = scheme;
            if (!WinHttpCrackUrl(current.c_str(), 0, 0, &schemeParts)) {
                if (error) *error = "Redireccion no resoluble";
                return false;
            }
            const std::wstring prefix = std::wstring(scheme, schemeParts.dwSchemeLength) + L"://" +
                                        std::wstring(schemeParts.lpszHostName, schemeParts.dwHostNameLength);
            current = prefix + Widen(attempt.location);
        } else {
            if (error) *error = "Redireccion relativa no soportada";
            return false;
        }
    }
    if (error) *error = "Demasiadas redirecciones";
    return false;
}

bool DirectoryWritable(const std::filesystem::path& directory) {
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return false;
    const auto probe = directory / ".xpebot_write_test";
    {
        std::ofstream file(probe, std::ios::binary | std::ios::trunc);
        if (!file) return false;
    }
    std::filesystem::remove(probe, ec);
    return true;
}

bool WriteScriptFile(const std::filesystem::path& path, const std::string& contents, std::string* error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        if (error) *error = "No se pudo crear el script de actualizacion";
        return false;
    }
    // BOM UTF-8: PowerShell 5.x interpreta correctamente el archivo.
    file.write("\xEF\xBB\xBF", 3);
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.close();
    return true;
}

const char* kSwapScript = R"PS(param(
    [Parameter(Mandatory=$true)][int]$ProcessId,
    [Parameter(Mandatory=$true)][string]$Zip,
    [Parameter(Mandatory=$true)][string]$Target,
    [Parameter(Mandatory=$true)][string]$Exe
)

$ErrorActionPreference = 'SilentlyContinue'

# 1. Esperar a que el proceso actual termine.
for ($i = 0; $i -lt 240; $i++) {
    $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if ($null -eq $proc) { break }
    Start-Sleep -Milliseconds 500
}
Start-Sleep -Milliseconds 500

# 2. Extraer el paquete en una carpeta temporal.
$staging = Join-Path $env:TEMP ('xpebot_stage_' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $staging -Force | Out-Null
try {
    Expand-Archive -LiteralPath $Zip -DestinationPath $staging -Force -ErrorAction Stop
} catch {
    Start-Process -FilePath $Exe -WorkingDirectory $Target
    Remove-Item -LiteralPath $PSCommandPath -Force
    exit 1
}

$payload = Get-ChildItem -LiteralPath $staging -Directory | Select-Object -First 1
if ($null -ne $payload) { $source = $payload.FullName } else { $source = $staging }

# 3. Preservar la configuracion del usuario.
$configPath = Join-Path $Target 'config\config.json'
$configBackup = Join-Path $env:TEMP ('xpebot_config_' + [guid]::NewGuid().ToString('N') + '.json')
$hadConfig = Test-Path -LiteralPath $configPath
if ($hadConfig) { Copy-Item -LiteralPath $configPath -Destination $configBackup -Force }

# 4. Copiar la version nueva sobre la instalacion.
try {
    Copy-Item -Path (Join-Path $source '*') -Destination $Target -Recurse -Force -ErrorAction Stop
} catch {
    Start-Sleep -Seconds 2
    Copy-Item -Path (Join-Path $source '*') -Destination $Target -Recurse -Force
}

if ($hadConfig) { Copy-Item -LiteralPath $configBackup -Destination $configPath -Force }
Remove-Item -LiteralPath $configBackup -Force
Remove-Item -LiteralPath $staging -Recurse -Force
Remove-Item -LiteralPath $Zip -Force

# 5. Relanzar la app actualizada.
Start-Process -FilePath $Exe -WorkingDirectory $Target

# 6. Borrarse a si mismo.
Start-Process -FilePath 'cmd.exe' -ArgumentList '/c','ping -n 3 127.0.0.1 >nul & del /f /q "' + $PSCommandPath + '"' -WindowStyle Hidden
exit 0
)PS";

std::filesystem::path ExecutablePath() {
    wchar_t buffer[MAX_PATH * 2]{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0) return {};
    return std::filesystem::path(std::wstring(buffer, length));
}

}  // namespace

namespace Updater {

std::string CurrentVersion() {
    const auto exe = ExecutablePath();
    if (exe.empty()) return "0.0.0.0";
    const std::string version = GameVersionDetector::ReadFileVersion(exe.wstring());
    if (version.empty() || version == "unknown") return "0.0.0.0";
    return version;
}

std::string NormalizeVersion(const std::string& raw) {
    std::string text = raw;
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(text.begin());
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' || text.back() == '\n')) {
        text.pop_back();
    }
    if (!text.empty() && (text.front() == 'v' || text.front() == 'V')) text.erase(text.begin());
    std::string cleaned;
    for (const char c : text) {
        if ((c >= '0' && c <= '9') || c == '.') cleaned.push_back(c);
        else break;
    }
    while (!cleaned.empty() && cleaned.back() == '.') cleaned.pop_back();
    if (cleaned.empty()) return "0.0.0";
    return cleaned;
}

int CompareVersions(const std::string& a, const std::string& b) {
    const auto left = ParseVersion(a);
    const auto right = ParseVersion(b);
    for (size_t i = 0; i < left.size() && i < right.size(); ++i) {
        if (left[i] < right[i]) return -1;
        if (left[i] > right[i]) return 1;
    }
    return 0;
}

std::string RepositorySlug() {
    const std::string configured = Config::instance().github_repo;
    return configured.empty() ? std::string(kDefaultRepo) : configured;
}

ReleaseInfo QueryLatestRelease(const std::string& repo) {
    ReleaseInfo info;
    if (repo.empty()) {
        info.error = "repositorio no configurado";
        return info;
    }
    const std::wstring url = L"https://api.github.com/repos/" + Widen(repo) + L"/releases/latest";

    Response response;
    std::string error;
    if (!HttpGet(url, response, {}, &error)) {
        info.error = error;
        return info;
    }
    if (response.status != 200) {
        info.error = "HTTP " + std::to_string(response.status);
        return info;
    }

    try {
        const auto json = nlohmann::json::parse(response.body);
        info.tag = json.value("tag_name", std::string{});
        info.version = NormalizeVersion(info.tag);
        info.notes = json.value("body", std::string{});
        info.pageUrl = json.value("html_url", std::string{});
        if (json.contains("assets") && json["assets"].is_array()) {
            for (const auto& asset : json["assets"]) {
                if (!asset.is_object()) continue;
                const std::string name = asset.value("name", std::string{});
                if (!EndsWithZip(name)) continue;
                info.downloadUrl = asset.value("browser_download_url", std::string{});
                break;
            }
        }
    } catch (const std::exception& ex) {
        info.error = std::string("respuesta invalida: ") + ex.what();
        return info;
    }

    if (info.version.empty() || info.downloadUrl.empty()) {
        info.error = "release sin paquete .zip";
        return info;
    }
    info.ok = true;
    return info;
}

std::filesystem::path UpdateDirectory() {
    const auto exeDir = ExecutableDirectory();
    const auto preferred = exeDir / "update";
    if (DirectoryWritable(preferred)) return preferred;

    wchar_t localAppData[MAX_PATH * 2]{};
    const DWORD localLength = GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData,
                                                      static_cast<DWORD>(std::size(localAppData)));
    if (localLength > 0 && localLength < std::size(localAppData)) {
        const auto fallback = std::filesystem::path(std::wstring(localAppData, localLength)) / "XPEBot" / "update";
        if (DirectoryWritable(fallback)) return fallback;
    }
    return preferred;
}

bool DownloadRelease(const ReleaseInfo& release, std::filesystem::path* outZip, std::string* error) {
    if (!release.ok || release.downloadUrl.empty()) {
        if (error) *error = "release sin URL de descarga";
        return false;
    }
    const auto directory = UpdateDirectory();
    const auto target = directory / ("XPEBot-v" + release.version + ".zip");
    std::filesystem::remove(target);

    Response response;
    std::string httpError;
    if (!HttpGet(Widen(release.downloadUrl), response, target, &httpError)) {
        if (error) *error = httpError;
        std::filesystem::remove(target);
        return false;
    }
    if (response.status != 200) {
        if (error) *error = "HTTP " + std::to_string(response.status);
        std::filesystem::remove(target);
        return false;
    }
    std::error_code ec;
    const auto size = std::filesystem::file_size(target, ec);
    if (ec || size < 1024) {
        if (error) *error = "el paquete descargado esta vacio";
        std::filesystem::remove(target);
        return false;
    }
    if (outZip) *outZip = target;
    return true;
}

bool LaunchUpdater(const std::filesystem::path& zipPath, std::string* error) {
    if (!std::filesystem::exists(zipPath)) {
        if (error) *error = "no hay paquete descargado";
        return false;
    }
    const auto exePath = ExecutablePath();
    const auto installDir = exePath.parent_path();

    if (!DirectoryWritable(installDir)) {
        if (error) *error = "la carpeta de instalacion es de solo lectura";
        return false;
    }

    wchar_t tempBuffer[MAX_PATH * 2]{};
    const DWORD tempLength = GetTempPathW(static_cast<DWORD>(std::size(tempBuffer)), tempBuffer);
    const std::filesystem::path tempDir = (tempLength > 0)
                                              ? std::filesystem::path(std::wstring(tempBuffer, tempLength))
                                              : std::filesystem::temp_directory_path();

    const auto scriptPath = tempDir / ("xpebot-update-" + std::to_string(GetCurrentProcessId()) + ".ps1");
    if (!WriteScriptFile(scriptPath, kSwapScript, error)) return false;

    const std::wstring command = L"powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass"
                                 L" -WindowStyle Hidden -File \"" + scriptPath.wstring() + L"\"" +
                                 L" -ProcessId " + std::to_wstring(GetCurrentProcessId()) +
                                 L" -Zip \"" + zipPath.wstring() + L"\"" +
                                 L" -Target \"" + installDir.wstring() + L"\"" +
                                 L" -Exe \"" + exePath.wstring() + L"\"";

    std::wstring mutableCommand = command;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    const BOOL created = CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                                        CREATE_NO_WINDOW, nullptr, installDir.wstring().c_str(),
                                        &startup, &process);
    if (!created) {
        if (error) *error = "CreateProcess fallo (" + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Logger::info("Updater: script de instalacion lanzado para " + Narrow(zipPath.filename().wstring()));
    return true;
}

void SetStatus(UiState& state, const std::string& text, const std::string& version) {
    {
        std::lock_guard<std::mutex> lock(state.textMutex);
        state.updateStatus = text;
        if (!version.empty()) state.updateVersion = version;
    }
    Logger::info("Updater: " + text);
}

namespace {

void CheckAndMaybeApply(UiState& state, bool allowAutoApply) {
    state.updateBusy = true;
    SetStatus(state, "Updates: buscando nueva version...");

    const std::string repo = RepositorySlug();
    const ReleaseInfo release = QueryLatestRelease(repo);
    if (!release.ok) {
        SetStatus(state, "Updates: no se pudo consultar " + repo + " (" + release.error + ")");
        state.updateBusy = false;
        return;
    }

    const std::string current = CurrentVersion();
    if (CompareVersions(release.version, current) <= 0) {
        SetStatus(state, "Updates: al dia (v" + current + ")", release.version);
        state.updateAvailable = false;
        state.updateReady = false;
        state.updateBusy = false;
        return;
    }

    state.updateAvailable = true;
    state.updateVersion = release.version;
    SetStatus(state, "Updates: v" + release.version + " disponible (instalada v" + current + ")", release.version);

    std::filesystem::path zip;
    std::string error;
    SetStatus(state, "Updates: descargando v" + release.version + "...", release.version);
    if (!DownloadRelease(release, &zip, &error)) {
        SetStatus(state, "Updates: fallo la descarga (" + error + ")", release.version);
        state.updateBusy = false;
        return;
    }

    g_pendingZip = zip;
    g_pendingVersion = release.version;
    state.updateReady = true;
    state.updateBusy = false;
    SetStatus(state, "Updates: v" + release.version + " lista para instalar", release.version);

    const bool wantAutoInstall = allowAutoApply && Config::instance().auto_update;
    if (!wantAutoInstall) return;

    std::string applyError;
    if (LaunchUpdater(zip, &applyError)) {
        g_pendingZip.clear();
        g_pendingVersion.clear();
        SetStatus(state, "Updates: instalando v" + release.version + " y reiniciando...", release.version);
        state.shutdown = true;
        state.actions.request(AppAction::Shutdown);
        return;
    }
    SetStatus(state, "Updates: no se pudo instalar automaticamente (" + applyError + ")", release.version);
}

}  // namespace

void Run(UiState& state) {
    // Margen para que la GUI abra y el bot cargue sus datos.
    for (int i = 0; i < 30 && !state.shutdown.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (state.shutdown.load()) return;

    if (Config::instance().auto_update) {
        CheckAndMaybeApply(state, true);
    } else {
        SetStatus(state, "Updates: busqueda automatica desactivada (v" + CurrentVersion() + ")");
    }

    while (!state.shutdown.load()) {
        if (state.checkUpdates.exchange(false)) {
            CheckAndMaybeApply(state, false);
        }
        if (state.applyUpdateRequested.exchange(false)) {
            if (g_pendingZip.empty() || !std::filesystem::exists(g_pendingZip)) {
                SetStatus(state, "Updates: no hay paquete pendiente, busca de nuevo");
            } else {
                std::string error;
                if (LaunchUpdater(g_pendingZip, &error)) {
                    SetStatus(state, "Updates: instalando v" + g_pendingVersion + " y reiniciando...", g_pendingVersion);
                    g_pendingZip.clear();
                    state.shutdown = true;
                    state.actions.request(AppAction::Shutdown);
                    return;
                }
                SetStatus(state, "Updates: no se pudo instalar (" + error + ")");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}

}  // namespace Updater
