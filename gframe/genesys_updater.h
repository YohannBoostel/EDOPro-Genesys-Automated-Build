#pragma once

#include "genesys_rules.h"

#include <string>

namespace ygo::genesys {

void StartCsvUpdate(Rules& rules,
                    const std::string& local_csv,
                    const std::string& remote_url);

} // namespace ygo::genesys
