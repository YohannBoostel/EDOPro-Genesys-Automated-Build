#pragma once

#include "genesys_rules.h"

#include <filesystem>
#include <string>

namespace ygo::genesys {

void StartCsvUpdate(Rules& rules,
                    const std::filesystem::path& local_csv,
                    const std::string& remote_url);

} // namespace ygo::genesys
