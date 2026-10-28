#pragma once

#include <filesystem>
#include <string>

namespace ygo::genesys {

void StartCsvUpdate(const std::filesystem::path& local_csv,
                    const std::string& remote_url);

} // namespace ygo::genesys
