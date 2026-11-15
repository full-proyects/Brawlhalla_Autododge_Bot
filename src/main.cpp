#include "AutododgeBot.h"
#include "gui.h"
#include "logger.h"
#include "Updater.h"
#include "config.h"
#include "startup.h"

#include <exception>
#include <filesystem>
#include <string>
#include <thread>

#include <shellapi.h>
#include <windows.h>

AutododgeBot* g_botInstance = nullptr;

BOOL WINAPI ConsoleHandler(DWORD eventType) {
    if (eventType == CTRL_C_EVENT || eventType == CTRL_BREAK_EVENT ||
        eventType == CTRL_CLOSE_EVENT || eventType == CTRL_LOGOFF_EVENT ||
        eventType == CTRL_SHUTDOWN_EVENT) {
        if (g_botInstance) g_botInstance->RestoreHook();
    }
    return FALSE;
}

namespace {

bool HasCommandLineFlag(const wchar_t* flag) {
    int count = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!argv) return false;
    bool found = false;
    for (int i = 1; i < count; ++i) {
        if (CompareStringOrdinal(argv[i], -1, flag, -1, TRUE) == CSTR_EQUAL) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

void EnableSoftwareRenderer() {
    // winit-software: rasterizador por CPU, sin OpenGL/D3D. Evita que la ventana
    // falle en equipos o maquinas virtuales sin aceleracion grafica usable.
    // Lista de backends: se intenta el de CPU y, si no existe, el normal.
    SetEnvironmentVariableW(L"SLINT_BACKEND", L"winit-software,winit");
    Startup::Log("renderer forzado: winit-software");
}

bool RelaunchWithSoftwareRenderer() {
    const auto exe = Startup::ExecutablePath();
    if (exe.empty()) return false;
    const std::wstring command = L"\"" + exe.wstring() + L"\" --software-renderer";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = command;
    const BOOL ok = CreateProcessW(exe.wstring().c_str(), mutableCommand.data(), nullptr, nullptr,
                                   FALSE, 0, nullptr, nullptr, &startup, &process);
    if (!ok) return false;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return true;
}

bool RuntimeFilesPresent(std::wstring* missingList) {
    const auto directory = Startup::ExecutablePath().parent_path();
    const wchar_t* required[] = {L"slint_cpp.dll", L"msvcp140.dll", L"vcruntime140.dll",
                                 L"vcruntime140_1.dll"};
    std::wstring missing;
    for (const wchar_t* name : required) {
        std::error_code ec;
        if (!std::filesystem::exists(directory / name, ec)) {
            if (!missing.empty()) missing += L"\n";
            missing += L"  - ";
            missing += name;
        }
    }
    if (missing.empty()) return true;
    if (missingList) *missingList = missing;
    return false;
}

int ReportFatal(const std::string& what) {
    Startup::Log("FATAL: " + what);
    const bool windowProblem = Startup::WindowFailedBefore();
    const bool alreadySoftware = HasCommandLineFlag(L"--software-renderer");
    if (windowProblem && !alreadySoftware) {
        // La ventana no pudo crearse: reintenta una vez con renderer por software.
        if (RelaunchWithSoftwareRenderer()) {
            Startup::Log("relanzado con --software-renderer");
            return 0;
        }
    }
    std::wstring message = L"XPE Bot no pudo iniciar.\n\n";
    message += Startup::Widen(what);
    message += L"\n\nDetalle completo en:\n  ";
    message += Startup::LogPath().wstring();
    message += L"\n\nSi el problema continua, abre una consola en la carpeta del programa "
               L"y ejecuta:\n  XPEBot.exe --software-renderer";
    Startup::ShowError(message);
    return 1;
}

} // namespace

int main() {
    Startup::Log("=== arranque de XPE Bot ===");

    const auto exePath = Startup::ExecutablePath();
    Startup::Log("exe: " + exePath.string());

    // Al lanzar "como administrador" desde Explorer el directorio de trabajo pasa a
    // ser System32; se fija a la carpeta del ejecutable para que data/ y config/ se
    // resuelvan siempre.
    {
        std::error_code cwdError;
        std::filesystem::current_path(exePath.parent_path(), cwdError);
        if (cwdError) Startup::Log("aviso: no se pudo fijar el directorio de trabajo: " + cwdError.message());
    }

    if (HasCommandLineFlag(L"--software-renderer")) {
        EnableSoftwareRenderer();
    } else if (Startup::WindowFailedBefore()) {
        Startup::Log("el arranque anterior no pudo crear la ventana: se usa renderer por software");
        EnableSoftwareRenderer();
    }

    std::wstring missing;
    if (!RuntimeFilesPresent(&missing)) {
        Startup::Log("faltan archivos de runtime junto al ejecutable");
        Startup::ShowError(L"Faltan archivos junto a XPEBot.exe:\n\n" + missing +
                           L"\n\nDescomprime el ZIP completo en una carpeta y ejecuta XPEBot.exe "
                           L"desde ahi. No lo ejecutes desde dentro del ZIP ni muevas solo el .exe.");
        return 1;
    }

    try {
        Logger::initialize();
        Startup::Log("logger inicializado");
        Config::instance().loadFromFile();
        Config::instance().sanitizeRuntime();
        Startup::Log("configuracion cargada");

        UiState ui;
        AutododgeBot bot(ui);
        g_botInstance = &bot;
        SetConsoleCtrlHandler(ConsoleHandler, TRUE);
        Startup::Log("bot creado");

        // La ventana abre de inmediato: init + hook corren en el hilo del bot.
        std::thread botThread([&bot] {
            try {
                if (bot.init()) bot.run();
            } catch (const std::exception& error) {
                Startup::Log(std::string("error en el hilo del bot: ") + error.what());
                Logger::error(std::string("hilo del bot: ") + error.what());
            } catch (...) {
                Startup::Log("error desconocido en el hilo del bot");
            }
        });

        // Auto-actualizacion: consulta GitHub Releases y aplica si hay version nueva.
        std::thread updateThread([&ui] {
            try {
                Updater::Run(ui);
            } catch (const std::exception& error) {
                Startup::Log(std::string("error en el updater: ") + error.what());
            } catch (...) {
                Startup::Log("error desconocido en el updater");
            }
        });

        Startup::Log("creando ventana");
        Gui gui;
        const int result = gui.run(ui);
        Startup::Log("ventana cerrada");

        ui.shutdown = true;
        ui.actions.request(AppAction::Shutdown);
        if (botThread.joinable()) botThread.join();
        if (updateThread.joinable()) updateThread.join();
        bot.RestoreHook();
        SetConsoleCtrlHandler(ConsoleHandler, FALSE);
        g_botInstance = nullptr;
        Logger::shutdown();
        Startup::Log("salida limpia");
        return result;
    } catch (const std::exception& error) {
        return ReportFatal(error.what());
    } catch (...) {
        return ReportFatal("excepcion desconocida durante el arranque");
    }
}
