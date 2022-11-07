#pragma once

#include <string>

namespace tmplt {

namespace flat {

//{0} == Prolog
//{1} == Outpath
std::string const hdr_prolog = R"(
#pragma once

{0}

#include "SRProxy/FlatBasicTypes.h"

#include "{1}FwdDeclare.h"

)";

//{0} == Type
//{1} == FlatType
//{2} == BaseClass
//{3} == Members
std::string const hdr_body = R"(
/// Flat encoding of \ref {0}
template<> class {1}{2}
{{
public:
  Flat(TTree* tr, const std::string& prefix, const std::string& totsize, const IBranchPolicy* policy);

  void Fill(const {0}& sr);
  void Clear();

protected:
{3}
}};
)";

//{0} == Header
//{1} == Input
std::string const cxx_prolog = R"(
#include "{0}"

#include "{1}"
)";

//{0} == FlatType
//{1} == Inits
//{2} == Type
//{3} == FillBody
//{4} == ClearBody
std::string const cxx_body = R"(
{0}::Flat(TTree* tr, const std::string& prefix, const std::string& totsize, const IBranchPolicy* policy) :
{1}
{{
}}

void {0}::Fill(const {2}& sr)
{{
{3}
}}

void {0}::Clear()
{{
{4}
}}
)";

std::string const fwd_prolog = R"(
#pragma once

namespace flat
{{
  template<class T> class Flat;
}}
)";

//{0} == FlatBaseType
std::string const base_init = "  {0}(tr, prefix, totsize, policy),\n";
//{0} == MemberName
std::string const member_init = "  {0}(tr, prefix+\".{0}\", totsize, policy),\n";

} // namespace flat

namespace proxy {

//{0} == Prolog
//{1} == Outpath
std::string const hdr_prolog = R"(
#pragma once

{0}

#include "SRProxy/BasicTypesProxy.h"

#include "{1}FwdDeclare.h"

)";

//{0} == Type
//{1} == ProxyType
//{2} == BaseClass
//{3} == Members
std::string const hdr_body = R"(
/// Proxy for \ref {0}
template<> class {1}{2}
{{
public:
  Proxy(TTree* tr, const std::string& name, long base, int offset);
  Proxy(TTree* tr, const std::string& name) : Proxy(tr, name, 0, 0) {{}}
  Proxy(const Proxy&) = delete;
  Proxy(const Proxy&&) = delete;
  Proxy& operator=(const {0}& x);

  void CheckEquals(const {0}& sr) const;

{3}
}};
)";

//{0} == Header
//{1} == Input
std::string const cxx_prolog = R"(
#include "{0}"

#include "{1}"

namespace
{{
  std::string Join(const std::string& a, const std::string& b)
  {{
    if(a.empty()) return b;
    return a+"."+b;
  }}
}}
)";

//{0} == ProxyType
//{1} == Inits
//{2} == Type
//{3} == AssignBody
//{4} == CheckEqualsBody
std::string const cxx_body = R"(
{0}::Proxy(TTree* tr, const std::string& name, long base, int offset) :
{1}
{{
}}

{0}& {0}::operator=(const {2}& sr)
{{
{3}
  return *this;
}}

void {0}::CheckEquals(const {2}& sr) const
{{
{4}
}}
)";

std::string const fwd_prolog = R"(
#pragma once

namespace caf
{{
  template<class T> class Proxy;
}}
)";

//{0} == ProxyBaseType
std::string const base_init = "  {0}(tr, name, base, offset),\n";

//{0} == MemberName
std::string const member_init = "  {0}(tr, Join(name, \"{0}\"), base, offset),\n";

} // namespace proxy

std::string const disclaimer = R"(// This file was generated automatically, do not edit it manually
// Generation details:
//   SRProxy Verion: {0}
//   datetime: {1:%Y-%m-%d %H:%M:%S} UTC
//   host: {2}
//   command: {3}
  )";

//{0} == Namespace
//{1} == Type
//{2} == ShortType
//{3} == ProxyType
std::string const fwd_body = R"(
namespace {0}
{{
  class {1};
  using {2} = {3};
}}
)";

//{0} == ProxyBaseType
std::string const assign_base_body = "  {0}::operator=(sr);\n";
//{0} == ProxyBaseType
std::string const checkequals_base_body = "  {0}::CheckEquals(sr);\n";
//{0} == ProxyBaseType
std::string const fill_base_body = "  {0}::Fill(sr);\n";
//{0} == ProxyBaseType
std::string const clear_base_body = "  {0}::Clear();\n";

//{0} == ProxyType
//{1} == MemberName
std::string const member_list = "  {0} {1};\n";
//{0} == MemberName
std::string const assign_member_body = "  {0} = sr.{0};\n";
//{0} == MemberName
std::string const checkequals_member_body = "  {0}.CheckEquals(sr.{0});\n";
//{0} == MemberName
std::string const fill_member_body = "  {0}.Fill(sr.{0});\n";
//{0} == MemberName
std::string const clear_member_body = "  {0}.Clear();\n";

} // namespace tmplt