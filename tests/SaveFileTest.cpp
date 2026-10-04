// Standalone regression; no game assets, Diptych, or test framework are required.
// From the repository root (C++17 compiler):
//   c++ -std=c++17 tests/SaveFileTest.cpp -o /tmp/SaveFileTest
//   /tmp/SaveFileTest /tmp/save-file-test-new-directory
// Windows developer shell: cl /std:c++17 /EHsc tests/SaveFileTest.cpp
// Pass a nonexistent disposable directory; failure leaves evidence for inspection.
#include "../soh/soh/SaveFile.h"
#include "../soh/soh/Enhancements/QoL/AutosaveFeedback.h"

#include <future>
#include <thread>

#include <iostream>
#include <iterator>
#include <stdexcept>

namespace SaveFile = SohSaveFile;
namespace fs = std::filesystem;

static void Require(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

static std::string Read(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "cannot read test file");
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

static void CheckAutosaveFeedback(const fs::path& root) {
    AutosaveFeedback feedback;
    const auto gameThread = std::this_thread::get_id();
    std::vector<bool> notifications;
    auto process = [&] {
        feedback.ProcessResults([&](bool success) {
            Require(std::this_thread::get_id() == gameThread, "feedback left the game thread");
            notifications.push_back(success);
        });
    };

    // An outstanding autosave cannot borrow completion from a manual save or another autosave.
    std::promise<void> release;
    auto gate = release.get_future();
    void* delayedResult = feedback.BeginSave();
    auto delayed = std::async(std::launch::async, [&] {
        gate.wait();
        AutosaveFeedback::CompleteSave(SaveFile::Publish(root / "autosave", "saved"), delayedResult);
    });
    process();
    Require(notifications.empty(), "queued autosave reported success before publication");
    Require(SaveFile::Publish(root / "manual", "manual"), "manual-save fixture failed");
    process();
    Require(notifications.empty(), "manual save completed an unrelated autosave");

    const fs::path blocked = root / "blocked";
    Require(fs::create_directory(blocked), "cannot create blocked-publication fixture");
    void* failedResult = feedback.BeginSave();
    std::async(std::launch::async, [&] {
        AutosaveFeedback::CompleteSave(SaveFile::Publish(blocked, "failed"), failedResult);
    }).get();
    process();
    Require(notifications == std::vector<bool>{false}, "publication failure reported autosave success");
    release.set_value();
    delayed.get();
    process();
    process();
    Require(notifications == std::vector<bool>({false, true}), "delayed completion was lost or repeated");

    fs::remove(blocked);
    void* recoveryResult = feedback.BeginSave();
    std::async(std::launch::async, [&] {
        AutosaveFeedback::CompleteSave(SaveFile::Publish(blocked, "recovered"), recoveryResult);
    }).get();
    process();
    Require(notifications == std::vector<bool>({false, true, true}), "successful retry did not report success");

    // Scene transitions retain the request; a new loaded game discards only its presentation.
    void* oldSession = feedback.BeginSave();
    feedback.Clear();
    void* newSession = feedback.BeginSave();
    AutosaveFeedback::CompleteSave(true, oldSession);
    process();
    Require(notifications.size() == 3, "old-session result appeared in a new game");
    AutosaveFeedback::CompleteSave(false, newSession);
    process();
    Require(notifications.size() == 4 && !notifications.back(), "new-session request was not independent");
    void* orphan;
    {
        AutosaveFeedback unloading;
        orphan = unloading.BeginSave();
    }
    std::async(std::launch::async, [orphan] { AutosaveFeedback::CompleteSave(true, orphan); }).get();

    fs::remove(root / "autosave");
    fs::remove(root / "manual");
    fs::remove(blocked);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: SaveFileTest <nonexistent disposable directory>\n";
        return 2;
    }
    try {
        const fs::path root = fs::absolute(argv[1]);
        Require(fs::create_directory(root), "test directory already exists");
        const fs::path destination = root / "save.json";
        Require(SaveFile::Publish(destination, "old\n"), "initial publication failed");

        // Reserve the next temporary names with foreign directory and file content.
        const fs::path foreignDirectory = destination.string() + ".tmp-" +
                                          std::to_string(SaveFile::NextTemporary());
        const fs::path foreignFile = destination.string() + ".tmp-" +
                                     std::to_string(SaveFile::NextTemporary() + 1);
        Require(fs::create_directory(foreignDirectory), "cannot create collision fixture");
        Require(SaveFile::Write(foreignDirectory / "keep", "directory content"), "fixture write failed");
        Require(SaveFile::Write(foreignFile, "file content"), "fixture write failed");
        Require(SaveFile::Publish(destination, "new\n"), "existing destination replacement failed");
        Require(Read(destination) == "new\n", "replacement bytes differ");
        Require(Read(foreignDirectory / "keep") == "directory content", "foreign directory changed");
        Require(Read(foreignFile) == "file content", "foreign file changed");

        bool partialWritten = false;
        Require(!SaveFile::Publish(destination, "partial", [&](const auto& path, const auto& text) {
                    partialWritten = SaveFile::Write(path, text);
                    return false;
                }), "failed writer reported success");
        Require(partialWritten, "partial-write fixture failed");
        Require(Read(destination) == "new\n", "failed writer replaced destination");
        partialWritten = false;
        Require(!SaveFile::Publish(destination, "partial", [&](const auto& path, const auto& text) -> bool {
                    partialWritten = SaveFile::Write(path, text);
                    throw std::runtime_error("injected writer failure");
                }), "throwing writer reported success");
        Require(partialWritten, "throwing-write fixture failed");
        Require(Read(destination) == "new\n", "throwing writer replaced destination");
        Require(std::distance(fs::directory_iterator(root), fs::directory_iterator()) == 3,
                "owned temporary paths were not cleaned");

        CheckAutosaveFeedback(root);

        // Only these exact fixture leaves are removed; never traverse an existing directory.
        fs::remove(foreignDirectory / "keep");
        fs::remove(foreignDirectory);
        fs::remove(foreignFile);
        fs::remove(destination);
        fs::remove(root);
        std::cout << "Save publication and autosave feedback regression passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
