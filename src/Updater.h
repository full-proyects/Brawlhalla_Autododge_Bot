#pragma once

#include <filesystem>
#include <string>

struct UiState;

// Auto-actualizacion remota basada en GitHub Releases.
//
// Flujo: consulta /releases/latest del repositorio configurado, compara la
// version con la del propio ejecutable (recurso VERSIONINFO), descarga el
// asset .zip y, opcionalmente, lo instala relanzando el proceso a traves de un
// script de PowerShell que espera a que la app termine antes de reemplazar
// archivos.
namespace Updater {

struct ReleaseInfo {
    bool ok = false;
    std::string tag;
    std::string version;      // normalizada, sin la 'v' inicial
    std::string notes;
    std::string downloadUrl;
    std::string pageUrl;
    std::string error;
};

// Version del propio ejecutable, ej. "2.0.1.0".
std::string CurrentVersion();

std::string NormalizeVersion(const std::string& raw);

// -1 si a < b, 0 si son iguales, 1 si a > b. Los componentes finales en cero
// se ignoran: "2.0.1" y "2.0.1.0" son equivalentes.
int CompareVersions(const std::string& a, const std::string& b);

// "owner/repo": toma config.github_repo o el repositorio por defecto.
std::string RepositorySlug();

ReleaseInfo QueryLatestRelease(const std::string& repo);

// Carpeta donde se guarda el paquete descargado. Usa la carpeta del ejecutable
// y cae a %LOCALAPPDATA%\XPEBot si esa no es escribible.
std::filesystem::path UpdateDirectory();

bool DownloadRelease(const ReleaseInfo& release, std::filesystem::path* outZip, std::string* error);

// Copia actualizacion pendiente: escribe el script de intercambio y lo lanza
// desacoplado del proceso actual.
bool LaunchUpdater(const std::filesystem::path& zipPath, std::string* error);

void SetStatus(UiState& state, const std::string& text, const std::string& version = {});

// Hilo del updater: chequeo inicial y atencion de peticiones de la UI.
void Run(UiState& state);

}  // namespace Updater
