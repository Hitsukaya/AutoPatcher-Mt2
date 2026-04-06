#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct ManifestEntry
{
    std::string path;
    std::uint64_t size = 0;
};

static std::string normalize_path(const fs::path &path)
{
    return path.generic_string();
}

static std::vector<ManifestEntry> collect_files(const fs::path &root)
{
    std::vector<ManifestEntry> files;

    if (!fs::exists(root))
    {
        throw std::runtime_error("Client root does not exist: " + root.string());
    }

    if (!fs::is_directory(root))
    {
        throw std::runtime_error("Client root is not a directory: " + root.string());
    }

    for (const auto &entry : fs::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file())
            continue;

        const fs::path relative = fs::relative(entry.path(), root);
        const std::uint64_t size =
            static_cast<std::uint64_t>(fs::file_size(entry.path()));

        files.push_back(ManifestEntry{
            normalize_path(relative),
            size});
    }

    std::sort(files.begin(), files.end(), [](const ManifestEntry &a, const ManifestEntry &b)
              { return a.path < b.path; });

    return files;
}

static void write_manifest(
    const std::string &output_path,
    const std::string &base_url,
    const std::vector<ManifestEntry> &files)
{
    std::ofstream out(output_path, std::ios::trunc);
    if (!out.is_open())
    {
        throw std::runtime_error("Cannot open output file: " + output_path);
    }

    out << "{\n";
    out << "  \"base_url\": \"" << base_url << "\",\n";
    out << "  \"files\": [\n";

    for (std::size_t i = 0; i < files.size(); ++i)
    {
        const auto &file = files[i];

        out << "    {\n";
        out << "      \"path\": \"" << file.path << "\",\n";
        out << "      \"size\": " << file.size << "\n";
        out << "    }";

        if (i + 1 < files.size())
            out << ",";

        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
}

int main()
{
    try
    {
        const fs::path client_root = R"(C:\Users\Personal\Desktop\Nyx2)";

        const std::string base_url =
            "https://eu2.contabostorage.com/daabf82772a64933a29f62dacfdb6091:hitsukaya-doragon/Nyx2Patcher/Client/";

        const std::string output_path = "manifest.json";

        const auto files = collect_files(client_root);
        write_manifest(output_path, base_url, files);

        std::cout << "manifest.json generated successfully.\n";
        std::cout << "Files: " << files.size() << "\n";
        std::cout << "Output: " << output_path << "\n";

        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
}