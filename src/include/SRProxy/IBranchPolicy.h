#pragma once

#include <string>

namespace flat
{
  class IBranchPolicy
  {
  public:
    virtual bool Include(const std::string&) const = 0;
  };
}
