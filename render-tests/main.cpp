#include "plugin_registry.hpp"

#include <mln/render_test.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

namespace {

namespace fs = std::filesystem;

bool isRepositoryRoot(const fs::path& path) {
    std::error_code error;
    return fs::is_directory(path / "plugins", error) && fs::is_regular_file(path / "settings.gradle.kts", error);
}

std::optional<fs::path> findRepositoryRoot(fs::path candidate) {
    std::error_code error;
    candidate = fs::absolute(std::move(candidate), error);
    if (error) return std::nullopt;
    while (!candidate.empty()) {
        if (isRepositoryRoot(candidate)) return candidate.lexically_normal();
        const auto parent = candidate.parent_path();
        if (parent == candidate) break;
        candidate = parent;
    }
    return std::nullopt;
}

std::optional<fs::path> repositoryRoot(const std::optional<fs::path>& requested, const char* executable) {
    if (requested) return findRepositoryRoot(*requested);

    if (const char* workspace = std::getenv("BUILD_WORKSPACE_DIRECTORY")) {
        if (auto root = findRepositoryRoot(workspace)) return root;
    }
    if (const char* testSourceDirectory = std::getenv("TEST_SRCDIR")) {
        if (const char* testWorkspace = std::getenv("TEST_WORKSPACE")) {
            if (auto root = findRepositoryRoot(fs::path(testSourceDirectory) / testWorkspace)) return root;
        }
    }
    if (auto root = findRepositoryRoot(fs::current_path())) return root;
    if (executable && *executable) {
        if (auto root = findRepositoryRoot(fs::path(executable).parent_path())) return root;
    }
    return std::nullopt;
}

bool registerPlugins() {
    const auto& registrations = mln::plugin::test::pluginRegistrations();
    if (registrations.empty()) {
        std::cerr << "No render-test plugin registrations were linked into the runner.\n";
        return false;
    }
    for (const auto& registration : registrations) {
        char error[512]{};
        const auto status = registration.function(&mln_plugin_register_v1, error, sizeof(error));
        if (status != MLN_PLUGIN_STATUS_OK && status != MLN_PLUGIN_STATUS_ALREADY_REGISTERED) {
            std::cerr << "Unable to register plugin through " << registration.symbol << ": " << error << '\n';
            return false;
        }
    }
    return true;
}

std::vector<fs::path> discoverManifests(const fs::path& root) {
    std::vector<fs::path> manifests;
    std::error_code error;
    const fs::recursive_directory_iterator end;
    for (fs::recursive_directory_iterator entry(root / "plugins", error); !error && entry != end;
         entry.increment(error)) {
        if (!entry->is_regular_file(error) || entry->path().filename() != "manifest.json" ||
            entry->path().parent_path().filename() != "render-tests") {
            continue;
        }
        manifests.push_back(entry->path().lexically_normal());
    }
    std::sort(manifests.begin(), manifests.end());
    return manifests;
}

std::vector<std::string> manifestArguments(const char* executable,
                                           const std::vector<std::string>& forwarded,
                                           const fs::path& manifest) {
    std::vector<std::string> arguments;
    arguments.reserve(forwarded.size() + 3);
    arguments.emplace_back(executable ? executable : "plugin-render-tests");
    arguments.insert(arguments.end(), forwarded.begin(), forwarded.end());
    arguments.emplace_back("--manifestPath");
    arguments.push_back(manifest.string());
    return arguments;
}

int runManifest(const char* executable, const std::vector<std::string>& forwarded, const fs::path& manifest) {
    auto arguments = manifestArguments(executable, forwarded, manifest);

    std::vector<char*> rawArguments;
    rawArguments.reserve(arguments.size());
    for (auto& argument : arguments) rawArguments.push_back(argument.data());

    std::cout << "\n=== Plugin render tests: " << manifest.parent_path().parent_path().filename().string() << " ===\n";
    return mln::runRenderTests(static_cast<int>(rawArguments.size()), rawArguments.data(), {});
}

int runManifestProcess(const char* executable, const std::vector<std::string>& forwarded, const fs::path& manifest) {
    auto arguments = manifestArguments(executable, forwarded, manifest);
    std::vector<char*> rawArguments;
    rawArguments.reserve(arguments.size() + 1);
    for (auto& argument : arguments) rawArguments.push_back(argument.data());
    rawArguments.push_back(nullptr);

    std::cout << "Starting isolated plugin render suite: " << manifest.parent_path().parent_path().filename().string()
              << '\n'
              << std::flush;

    const auto child = fork();
    if (child < 0) {
        std::cerr << "Unable to start render tests for " << manifest << ": " << std::strerror(errno) << '\n';
        return 70;
    }
    if (child == 0) {
        execvp(rawArguments.front(), rawArguments.data());
        std::cerr << "Unable to execute " << rawArguments.front() << ": " << std::strerror(errno) << '\n';
        _exit(127);
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::cerr << "Unable to wait for render tests for " << manifest << ": " << std::strerror(errno) << '\n';
        return 70;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) {
        std::cerr << "Render tests for " << manifest << " terminated by signal " << WTERMSIG(status) << '\n';
        return 128 + WTERMSIG(status);
    }
    std::cerr << "Render tests for " << manifest << " ended with an unknown process status\n";
    return 70;
}

} // namespace

int main(int argc, char** argv) {
    std::optional<fs::path> requestedRoot;
    std::optional<fs::path> explicitManifest;
    bool listOnly = false;
    std::vector<std::string> forwarded;

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument{argv[i]};
        if (argument == "--plugin-test-root") {
            if (++i >= argc) {
                std::cerr << "--plugin-test-root requires a path\n";
                return 64;
            }
            requestedRoot = argv[i];
        } else if (argument.starts_with("--plugin-test-root=")) {
            requestedRoot = std::string(argument.substr(std::string_view{"--plugin-test-root="}.size()));
        } else if (argument == "--list-plugin-tests") {
            listOnly = true;
        } else if (argument == "--manifestPath" || argument == "-p") {
            if (++i >= argc) {
                std::cerr << argument << " requires a path\n";
                return 64;
            }
            explicitManifest = argv[i];
        } else if (argument.starts_with("--manifestPath=")) {
            explicitManifest = std::string(argument.substr(std::string_view{"--manifestPath="}.size()));
        } else if (argument.starts_with("-p=")) {
            explicitManifest = std::string(argument.substr(3));
        } else {
            forwarded.emplace_back(argument);
        }
    }

    if (!registerPlugins()) return 4;

    std::vector<fs::path> manifests;
    if (explicitManifest) {
        std::error_code error;
        const auto absoluteManifest = fs::absolute(*explicitManifest, error);
        if (!error) manifests.push_back(absoluteManifest.lexically_normal());
    } else {
        const auto root = repositoryRoot(requestedRoot, argc > 0 ? argv[0] : nullptr);
        if (!root) {
            std::cerr << "Unable to locate the plugin-template repository. Use "
                         "--plugin-test-root <path>.\n";
            return 66;
        }
        manifests = discoverManifests(*root);
    }

    if (manifests.empty()) {
        std::cerr << "No plugin render-test manifests were found.\n";
        return 66;
    }
    if (listOnly) {
        for (const auto& manifest : manifests) std::cout << manifest.string() << '\n';
        return 0;
    }

    int result = 0;
    for (const auto& manifest : manifests) {
        const auto manifestResult = explicitManifest
                                        ? runManifest(argc > 0 ? argv[0] : nullptr, forwarded, manifest)
                                        : runManifestProcess(argc > 0 ? argv[0] : nullptr, forwarded, manifest);
        result = std::max(result, manifestResult);
    }
    return result;
}
