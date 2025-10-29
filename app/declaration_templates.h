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
//{3} == AdditionalClasses
//{4} == Members
std::string const hdr_body = R"(
/// Flat encoding of \ref {0}
template<> class {1}{2}
{{
public:
  Flat(TTree *tr, const std::string &prefix, const std::string &totsize, const IBranchPolicy *policy);

  void Fill(const {0}& sr);
  void Clear();

protected:
{3}
{4}
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
{0}::Flat(TTree *tr, const std::string &prefix, const std::string &totsize, const IBranchPolicy *policy) :
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
std::string const member_init =
    "  {0}(tr, prefix+\".{0}\", totsize, policy),\n";

} // namespace flat

namespace proxy {

//{0} == Prolog
//{1} == Outpath
std::string const hdr_prolog = R"(
#pragma once

{0}
#include "SRProxy/BasicTypesProxy.txx"

#include "{1}FwdDeclare.h"

)";

//{0} == Type
//{1} == ProxyType
//{2} == BaseClass
//{3} == AdditionalClasses
//{4} == Members
std::string const hdr_body = R"(
/// Proxy for \ref {0}
template<> class {1}{2}
{{
public:
  Proxy(TTree *tr, const std::string &name, const long &base, int offset, const Lineage * parent = nullptr);
  Proxy(TTree *tr, const std::string &name) : Proxy(tr, name, kDummyBase, 0) {{}}
  Proxy(const Proxy&) = delete;
  Proxy(const Proxy&&) = delete;
  Proxy& operator=(const {0}& x);

  void CheckEquals(const {0}& sr) const;
{3}
{4}
}};
)";

//{0} == Header
//{1} == Input
std::string const cxx_prolog = R"(
#include "{0}"

#include "{1}"

namespace
{{
  std::string Join(const std::string &a, const std::string &b)
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
{0}::Proxy(TTree* tr, const std::string& name, const long& base, int offset, const Lineage * parent) :
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
std::string const base_init = "  {0}(tr, name, base, offset, parent),\n";

//{0} == MemberName
std::string const member_init =
    "  {0}(tr, Join(name, \"{0}\"), base, offset, this),\n";

} // namespace proxy

std::string const disclaimer =
    R"(// This file was generated automatically, do not edit it manually
// Generation details:
//   SRProxy Verion: {0}
//   ROOT Version: {1}
//   ROOT Library Dir: {2}
//   datetime: {3:%Y-%m-%d %H:%M:%S} UTC
//   host: {4}
//   command: {5}
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

namespace python {

std::string const impl_frontmatter = R"(#include "{0}"
#include "{1}.h"

#include "SRProxy/python/ProxyFileReader.txx"

#include "pybind11/pybind11.h"
#include "pybind11/native_enum.h"

namespace py = pybind11;

)";

std::string const lineage_ancestor_type_cppdeclaration = R"--(
enum class Lineage_typename {)--";

std::string const lineage_ancestor_cpptype = R"--(
  {0}={1},)--";

std::string const module_declaration = R"(
PYBIND11_MODULE(py{0}, m) {{
)";


std::string const lineage_ancestor_type_pydeclaration = R"--(
  py::class_<caf::Lineage> pyLineage(m, "Lineage");
  py::native_enum<Lineage_typename>(m, "ProxyTypes", "enum.IntEnum"))--";

std::string const lineage_ancestor_pytype = R"--(
    .value("{0}", Lineage_typename::{0}) )--";

std::string const lineage_ancestor_type_pyfinalize = R"--(
    .finalize();
)--";

std::string const lineage_ancestor_function = R"--(
  pyLineage
    .def("Ancestor",
      [](caf::Lineage const &prx, Lineage_typename const & tname) -> pybind11::object {
        switch(tname) {{)--";

std::string const lineage_ancestor_case = R"--(
          case Lineage_typename::{0}: {{ auto anc = prx.Ancestor<caf::Proxy<{1}>>(); return anc ? py::cast(anc) : py::none(); }} )--";

std::string const lineage_default_rvp = R"--(
          default: return py::none();
        }}
      }, py::return_value_policy::reference);
)--";

std::string const class_declaration = R"--(
  py::class_<caf::Proxy<{0}>>(m, "{1}", pyLineage) )--";

std::string const datamember_proxy = R"--(
    .def_readonly("{0}",&caf::Proxy<{1}>::{0}) // {2})--";

std::string const datamember_basic_type = R"--(
    .def_property_readonly("{0}",[](caf::Proxy<{1}> &prx){{
        return prx.{0}.GetValue(); }}) // {2})--";

std::string const vector_of_proxies = R"--(
  py::class_<caf::Proxy<{0}>>(m, "{1}", pyLineage)
    .def("at",[](caf::Proxy<{0}> &prx, size_t i) -> caf::Proxy<{2}>&{{
      return prx.at(i);
    }}, py::return_value_policy::reference)
    .def("__getitem__",[](caf::Proxy<{0}> &prx, size_t i) -> caf::Proxy<{2}>&{{
      return prx[i];
    }}, py::return_value_policy::reference)
    .def("__iter__",
        [](caf::Proxy<{0}> &prx) {{ return py::make_iterator(prx.begin(), prx.end()); }});
)--";
std::string const vector_of_basic_types = R"--(
  py::class_<caf::Proxy<{0}>>(m, "{1}", pyLineage)
    .def("at",[](caf::Proxy<{0}> &prx, size_t i) {{
      return prx.at(i).GetValue();
    }})
    .def("__getitem__",[](caf::Proxy<{0}> &prx, size_t i){{
      return prx[i].GetValue();
    }})
    .def("__iter__",
        [](caf::Proxy<{0}> &prx) {{ return py::make_iterator(prx.begin_remove_proxy(), prx.end_remove_proxy()); }});
)--";

std::string const proxyfilereader = R"(
py::class_<ProxyFileReader<{0}>>(m, "{1}FileReader")
      .def(py::init<std::string const &, std::vector<std::string> const &>())
      .def(py::init<std::string const &, std::string const &>())
      .def("entries", &ProxyFileReader<{0}>::entries)
      .def("entry", &ProxyFileReader<{0}>::entry)
      .def(
          "__iter__",
          [](ProxyFileReader<{0}> &s) {{
            return py::make_iterator(begin(s), end(s));
          }},
          py::keep_alive<0, 1>());
)";
} // namespace python

} // namespace tmplt
