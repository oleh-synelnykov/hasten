#pragma once

#include <map>
#include <string>

#include "source_file.hpp"

namespace hasten::frontend {

struct Program {
    using Files = std::map<std::string, SourceFile>;
    Files files;
};

} // namespace hasten::frontend