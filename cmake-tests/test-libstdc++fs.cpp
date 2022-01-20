#include <filesystem>

auto main() -> int {
    auto path = std::filesystem::temp_directory_path();
    return 0;
}
