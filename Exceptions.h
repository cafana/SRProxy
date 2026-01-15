#pragma once

#include <stdexcept>

namespace caf
{
    class MissingBranchException : public std::runtime_error
    {
        public:
          MissingBranchException(std::string const & contextname, std::string const & brname, std::string const & treename)
            : std::runtime_error(contextname + ": Branch '" + brname + "' not found in tree '" + treename + "'.")
          {}
    };
}