#pragma once

// Diagnostico de arranque del bot.
//
// La aplicacion es de subsistema GUI: si algo falla antes de crear la ventana
// no aparece ningun mensaje y el proceso termina en silencio. Estas utilidades
// dejan siempre un rastro (log en %LOCALAPPDATA%\XPEBot\logs\startup.log) y un
// cuadro de error visible cuando el fallo es fatal.

#include <filesystem>
#include <string>

namespace Startup {

// Ruta del log de arranque, siempre escribible (%LOCALAPPDATA%\XPEBot\logs).
std::filesystem::path LogPath();

// Ruta completa del ejecutable actual.
std::filesystem::path ExecutablePath();

// Anade una linea al log de arranque y a OutputDebugString.
void Log(const std::string& message);

// Cuadro de error modal (visible aunque no haya ventana ni consola).
void ShowError(const std::wstring& message);

// Marca/limpia el fallo al crear la ventana; sirve para relanzar la proxima vez
// con el renderer por software.
void MarkWindowFailure();
void ClearWindowFailure();
bool WindowFailedBefore();

std::wstring Widen(const std::string& text);

} // namespace Startup
