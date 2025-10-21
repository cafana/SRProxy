#include "declaration_templates.h"

#include "TBaseClass.h"
#include "TClass.h"
#include "TDataMember.h"
#include "TInterpreter.h"
#include "TSystem.h"

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/os.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include <climits>
#include <unistd.h>

int verbose = 0;
bool order_alphabetically = false;
bool emit_python = false;

std::string GetVectorValueTypeName(std::string classname) {
  auto openb = classname.find_first_of('<');
  auto closeb = classname.find_last_of('>');
  return classname.substr(openb + 1, closeb - openb - 1);
}

bool IsSTLVector(TDataMember &dm) {
  return (dm.IsSTLContainer() == TDictionary::kVector);
}

bool IsStaticDatamember(TDataMember &dm, TClass *cls) {
  return (dm.GetOffset() > cls->GetClassSize());
}

bool IsSTLVector(TClass *cls) {
  return (cls->GetCollectionType() == ROOT::kSTLvector);
}

bool KnownType(std::string name) {
  return gInterpreter->TypeInfo_IsValid(
      gInterpreter->TypeInfo_Factory(name.c_str()));
}

bool KnownClass(std::string name) {
  if (name == "string") {
    return false; // pretend string is a primitive
  }
  return gInterpreter->ClassInfo_IsValid(
      gInterpreter->ClassInfo_Factory(name.c_str()));
}

bool IsStandardTypeOrEnum(TDataMember &dm) {
  return (dm.IsBasic() || dm.IsEnum() ||
          (std::string(dm.GetTypeName()) == "string"));
}

std::string QualifystdNS(std::string classname) {
  for (std::string const &stype : {"vector", "string"}) {
    size_t pos = classname.find(stype);
    while (pos != std::string::npos) {

      if (pos && (classname[pos - 1] != ':')) {
        classname.replace(pos, 6, std::string("std::") + stype);
      } else if (!pos) {
        classname.replace(pos, 6, std::string("std::") + stype);
      }

      pos = classname.find(stype, pos + 6);
    }
  }
  return classname;
}

std::string GetTypeName(TDataMember &dm) {
  std::stringstream tn;
  tn << dm.GetTypeName();

  if (dm.GetArrayDim()) {
    tn << " ";
  }

  for (int i = 0; i < dm.GetArrayDim(); ++i) {
    tn << "[" << dm.GetMaxIndex(i) << "]";
  }

  std::string name = QualifystdNS(tn.str());

  // remove C++03 spaces between angle-brackets
  size_t pos = name.find("> >");
  while (pos != std::string::npos) {
    name.replace(pos, 3, ">>");
    pos = name.find("> >");
  }

  return name;
}

std::string GetNS(std::string classname) {
  size_t lpos = classname.rfind("::");

  if (lpos != std::string::npos) {
    return classname.substr(0, lpos);
  }
  return "";
}
std::string GetClassName(std::string classname) {
  size_t lpos = classname.rfind("::");
  if (lpos != std::string::npos) {
    return classname.substr(lpos + 2, std::string::npos);
  }
  return classname;
}
std::string GetShortProxyType(std::string classname) {
  return GetClassName(classname) + "Proxy";
}
std::string GetShortFlatType(std::string classname) {
  return std::string("Flat") + GetClassName(classname);
}

std::string GetPythonClassName(std::string classname) {
  for (auto &c : classname) {
    if ((c == ':')) {
      c = '_';
    }
    if ((c == '<')) {
      c = 'L';
    }
    if ((c == '>')) {
      c = 'R';
    }
  }
  return classname;
}

void WalkClass(TClass *cls, std::vector<std::string> &Declarations,
               std::vector<std::string> &EDeclarations,
               std::string indent = "") {
  if (!cls) {
    std::cout << "[ERROR]: WalkClass was passed a nullptr." << std::endl;
    abort();
  }

  // We have already walked this type
  if (std::find(Declarations.begin(), Declarations.end(), cls->GetName()) !=
      Declarations.end()) {
    if (verbose > 1) {
      fmt::print("{}Already known class: \"{}\"\n", indent, cls->GetName());
    }
    return;
  }

  if (IsSTLVector(cls)) { // this enables datamembers that are vectors of
                          // vectors to be properly processed
    if (verbose > 1) {
      fmt::print("{}Found STL Vector type: \"{}\"\n", indent, cls->GetName());
    }
    auto vvt = GetVectorValueTypeName(cls->GetName());
    if (verbose > 1) {
      fmt::print("{}Determined value type as: \"{}\"\n", indent, vvt);
    }
    if (!KnownType(vvt)) {
      std::cout << "[ERROR]: TCling has no typeinfo for " << vvt << std::endl;
      abort();
    }

    if (KnownClass(vvt)) { // If the contained type is a class (as opposed to a
                           // primitive), then we should check that we know how
                           // to proxy the vector value type
      if (verbose) {
        fmt::print("{}Walking RTTI tree for class: \"{}\"\n", indent, vvt);
      }
      WalkClass(TClass::GetClass(vvt.c_str()), Declarations, EDeclarations,
                indent + "- ");
    }

    // We don't need to emit a proxy class for the vector template itself
    return;
  }

  if (verbose > 1) {
    fmt::print("{}Class {}, has {} base classes.\n", indent, cls->GetName(),
               cls->GetListOfBases()->GetEntries());
  }

  if (cls->GetListOfBases()->GetEntries() > 1) {
    std::cout
        << "[ERROR]: Class " << cls->GetName() << " has "
        << cls->GetListOfBases()->GetEntries()
        << " base classes, but we can currently only handle single inheritance."
        << std::endl;
    abort();
  }

  for (auto base_to : *cls->GetListOfBases()) {
    auto bcls = dynamic_cast<TBaseClass *>(base_to)->GetClassPointer();
    if (verbose) {
      fmt::print("{}Walking RTTI tree for base class: \"{}\"\n", indent,
                 bcls->GetName());
    }
    WalkClass(bcls, Declarations, EDeclarations, indent + "- ");
  }

  // Loop through this classes public data members, checking if we need to emit
  // proxies for any of their types

  std::vector<TDataMember *> DataMembers;

  for (auto dm_to : *cls->GetListOfAllPublicDataMembers()) {

    auto dm_ptr = dynamic_cast<TDataMember *>(dm_to);

    if (!dm_ptr->IsValid()) {
      std::cout << "[ERROR]: Failed to read type for data member "
                << dm_ptr->GetName() << " of class " << cls->GetName()
                << std::endl;
      abort();
    }

    DataMembers.push_back(dm_ptr);
  }

  // The pygccxml/castxml version traversed data members alphabetically rather
  // than in declaration order.
  if (order_alphabetically) {
    std::sort(DataMembers.begin(), DataMembers.end(),
              [](TDataMember const *l, TDataMember const *r) {
                return std::string(l->GetName()).compare(r->GetName()) < 0;
              });
  }

  for (auto dm_ptr : DataMembers) {
    auto &dm = *dm_ptr;

    if (verbose > 1) {
      fmt::print(
          "{}Examining data member: \"{}\" of type {} (Basic: {}, Enum: {})\n",
          indent, dm.GetName(), dm.GetTypeName(),
          (IsStandardTypeOrEnum(dm) ? "true" : "false"),
          (dm.IsEnum() ? "true" : "false"));
    }

    // If this data member's type is not a primitive, then we need to check if
    // we need to emit a proxy class for it, either the type itself, or the
    // value type of an STL vector.
    if (IsSTLVector(dm)) {

      if (verbose > 1) {
        fmt::print("{}Data member has STL Vector type: \"{}\"\n", indent,
                   dm.GetTypeName());
      }

      auto vvt = GetVectorValueTypeName(dm.GetTypeName());

      if (verbose > 1) {
        fmt::print("{}Determined value type as: \"{}\"\n", indent, vvt);
      }
      if (!KnownType(vvt)) {
        std::cout << "[ERROR]: TCling has no typeinfo for " << vvt << std::endl;
        abort();
      }

      if (KnownClass(vvt)) { // If the contained type is a class (as opposed to
                             // a primitive), then we should check that we know
                             // how to proxy the vector value type
        if (verbose) {
          fmt::print("{}Walking RTTI tree for class: \"{}\"\n", indent, vvt);
        }
        WalkClass(TClass::GetClass(vvt.c_str()), Declarations, EDeclarations,
                  indent + "- ");
      }

    } else if (!IsStandardTypeOrEnum(dm)) {
      if (verbose) {
        fmt::print("{}Walking RTTI tree for class: \"{}\"\n", indent,
                   dm.GetTypeName());
      }
      WalkClass(TClass::GetClass(dm.GetTypeName()), Declarations, EDeclarations,
                indent + "- ");
    } else if (dm.IsEnum()) {

      // Add this type to the list of types if we don't already know about it
      if (std::find(EDeclarations.begin(), EDeclarations.end(),
                    dm.GetTypeName()) == EDeclarations.end()) {
        if (verbose) {
          fmt::print("{}Storing declaration of enum: \"{}\"\n", indent,
                     dm.GetTypeName());
        }
        EDeclarations.push_back(dm.GetTypeName());
      }
    }
  }

  // Add this type to the list of types if we don't already know about it
  if (std::find(Declarations.begin(), Declarations.end(), cls->GetName()) ==
      Declarations.end()) {
    if (verbose) {
      fmt::print("{}Storing declaration of class: \"{}\"\n", indent,
                 cls->GetName());
    }
    Declarations.push_back(cls->GetName());
  }
}

std::string input_header, target_class;
std::string output_file, output_dir;
std::vector<std::string> includes;
std::vector<std::string> defines;
std::string output_path;
std::string prolog_file, epilog_file, epilog_fwd_file;
std::string prolog_contents, epilog_contents, epilog_fwd_contents;

bool gen_flat = false;

std::string qualified_disclaimer;
std::map<std::string, std::string> additional_class_files;
std::map<std::string, std::string> additional_class_defintions;
std::map<std::string, bool> additional_class_definition_used;

std::string CutSStream(std::stringstream const &ss, size_t n) {
  std::string rtn = ss.str();
  return rtn.substr(0, rtn.length() - n);
}

void EmitClass(std::string classname, fmt::ostream &out_hdr,
               fmt::ostream &out_impl, fmt::ostream &out_fwd,
               std::ofstream &out_pyb,
               std::set<std::string> &py_emitted_vector_types) {

  std::stringstream inits;
  std::stringstream memberlist;

  std::stringstream memberlist_pyimpl;
  std::stringstream basicmemberlist_pyimpl;

  std::stringstream assign_body;
  std::stringstream checkequals_body;

  std::stringstream fill_body;
  std::stringstream clear_body;

  auto cls = TClass::GetClass(classname.c_str());

  std::string type = classname;
  std::string typename_noNS = GetClassName(classname);
  std::string ptype =
      fmt::format(gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", classname);

  std::string base_declaration;
  std::set<std::string> base_members;

  size_t nbases = 0;
  for (auto base_to : *cls->GetListOfBases()) {
    auto bcls = dynamic_cast<TBaseClass *>(base_to)->GetClassPointer();

    std::string pbtype = fmt::format(
        gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", bcls->GetName());

    inits << fmt::format(
        gen_flat ? tmplt::flat::base_init : tmplt::proxy::base_init, pbtype);
    assign_body << fmt::format(tmplt::assign_base_body, pbtype);
    checkequals_body << fmt::format(tmplt::checkequals_base_body, pbtype);

    fill_body << fmt::format(tmplt::fill_base_body, pbtype);
    clear_body << fmt::format(tmplt::clear_base_body, pbtype);

    base_declaration = fmt::format(" : public {}", pbtype);

    for (auto dm_to : *bcls->GetListOfAllPublicDataMembers()) {
      auto &dm = dynamic_cast<TDataMember &>(*dm_to);
      base_members.insert(dm.GetName());
    }

    nbases++;
  }

  // base classes need their Proxies to inherit from Lineage
  if (!gen_flat && (nbases == 0)) {
    inits << " Lineage(parent),\n";
    base_declaration = " : public caf::Lineage";
  }

  std::vector<TDataMember *> DataMembers;
  std::vector<std::string> vector_types;

  for (auto dm_to : *cls->GetListOfAllPublicDataMembers()) {

    auto dm_ptr = dynamic_cast<TDataMember *>(dm_to);

    if (!dm_ptr->IsValid()) {
      std::cout << "[ERROR]: Failed to read type for data member "
                << dm_ptr->GetName() << " of class " << cls->GetName()
                << std::endl;
      abort();
    }

    DataMembers.push_back(dm_ptr);
  }

  // The pygccxml/castxml version traversed data members alphabetically rather
  // than in declaration order.
  if (order_alphabetically) {
    std::sort(DataMembers.begin(), DataMembers.end(),
              [](TDataMember const *l, TDataMember const *r) {
                return std::string(l->GetName()).compare(r->GetName()) < 0;
              });
  }

  for (auto dm_ptr : DataMembers) {
    auto &dm = *dm_ptr;

    std::string mname = dm.GetName();

    if (base_members.count(
            mname)) { // Don't re-declare members of the base class
      continue;
    }

    // Check if the data member is static and skip if it is
    if (IsStaticDatamember(dm, cls)) {
      continue;
    }
    std::string mptype = fmt::format(
        gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", GetTypeName(dm));

    inits << fmt::format(
        gen_flat ? tmplt::flat::member_init : tmplt::proxy::member_init, mname);

    memberlist << fmt::format(tmplt::member_list, mptype, mname);

    if (emit_python) {
      if (!IsStandardTypeOrEnum(dm) || dm.GetArrayDim()) {
        if (IsSTLVector(dm)) {
          vector_types.push_back(GetTypeName(dm));
        }
        memberlist_pyimpl << fmt::format(R"--(
    .def_readonly("{0}",&caf::Proxy<{1}>::{0}) // {2})--",
                                         mname, classname, GetTypeName(dm));

      } else {
        basicmemberlist_pyimpl << fmt::format(R"(if(attr == "{0}"){{ // {1}
        return py::cast(prx.{0}.GetValue());
      }})",
                                              mname, GetTypeName(dm));
      }
    }

    assign_body << fmt::format(tmplt::assign_member_body, mname);
    checkequals_body << fmt::format(tmplt::checkequals_member_body, mname);

    fill_body << fmt::format(tmplt::fill_member_body, mname);
    clear_body << fmt::format(tmplt::clear_member_body, mname);
  }

  //{0} == Namespace
  //{1} == Type
  //{2} == ShortType
  //{3} == ProxyType
  out_fwd.print(
      tmplt::fwd_body, GetNS(classname), typename_noNS,
      gen_flat ? GetShortFlatType(classname) : GetShortProxyType(classname),
      fmt::format(gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", classname));

  //{0} == Type
  //{1} == ProxyType
  //{2} == BaseClass
  //{3} == AdditionalClasses
  //{4} == Members
  std::string additional_definitions = "";
  if (additional_class_defintions.count(typename_noNS)) {
    additional_class_definition_used[typename_noNS] = true;
    additional_definitions = additional_class_defintions[typename_noNS];
  }

  out_hdr.print(gen_flat ? tmplt::flat::hdr_body : tmplt::proxy::hdr_body, type,
                ptype, base_declaration, additional_definitions,
                CutSStream(memberlist, 1));

  //{0} == ProxyType
  //{1} == Inits
  //{2} == Type
  //{3} == AssignBody
  //{4} == CheckEqualsBody
  out_impl.print(gen_flat ? tmplt::flat::cxx_body : tmplt::proxy::cxx_body,
                 ptype,
                 // Trim off the last newline and comma from this list
                 CutSStream(inits, 2), type,
                 CutSStream(gen_flat ? fill_body : assign_body, 1),
                 CutSStream(gen_flat ? clear_body : checkequals_body, 1));

  if (emit_python) {

    for (auto const &vector_type : vector_types) {
      if (!py_emitted_vector_types.count(vector_type)) {
        out_pyb << fmt::format(R"--(
  py::class_<caf::Proxy<{0}>>(m, "{1}")
    .def("at",[](caf::Proxy<{0}> &prx, size_t i) -> caf::Proxy<{2}>&{{
      return prx.at(i);
    }})
    .def("__getitem__",[](caf::Proxy<{0}> &prx, size_t i) -> caf::Proxy<{2}>&{{
      return prx[i];
    }})
    .def("__iter__",
        [](caf::Proxy<{0}> &prx) {{ return py::make_iterator(prx.begin(), prx.end()); }});
)--",
                               vector_type, GetPythonClassName(vector_type),
                               GetVectorValueTypeName(vector_type));
        py_emitted_vector_types.insert(vector_type);
      }
    }

    out_pyb << fmt::format(R"--(
  py::class_<caf::Proxy<{0}>>(m, "{1}") )--",
                           classname, GetPythonClassName(classname));
    out_pyb << fmt::format(memberlist_pyimpl.str());
    out_pyb << fmt::format(R"(
    .def("__getattr__",[](caf::Proxy<{0}> &prx, std::string const &attr){{
      {1}
      return py::cast(nullptr);
    }});
)",
                           classname, basicmemberlist_pyimpl.str());
  }
}

void Usage(char const *argv[]) {
  fmt::print(
      R"([USAGE] {}  -i <header_file> -t <classname> -o <filename_stub> [args]

Required arguments:
  -i|--input <header_file>       : The C++ header file that defines the class
  -t|--target <classname>        : The class to generate a proxy for
  -o|--output <filename stub>    : Output filename stub

Optional arguments:
  -I <path>                      : A directory to add to the include path
  -D <symbol>[=val]              : A symbol definition, with optional value, to the interpreter before parsing

  --flat                         : Generate a 'flat' file reader rather than the objectified proxy class

  --order-alphabetically         : Emit datamembers in alphabetic, rather than declaration, order.

  -p|--include-path <path1[:p2]> : A PATH-like colon-separate list of directories to add to the include path
  -op|--output-path <path>       : A path to prepend to include statements in generated headers
  -od|--output-dir <path>        : The directory to write generated files to
  --prolog <file path>           : A file to include before the generated proxy class defintion
  --epilog <file path>           : A file to include after the generated proxy class definition
  --epilog-fwd <file path>       : A file to include after the list of generated forward declarations
  --extra <classname> <file>     : A file to include in the definition of the proxy class for class <classname>

  --emit-python-bindings         : Write pybind11 python bindings to <-o>.pybind.cxx

  -v|--verbose                   : Be louder
  -vv|--vverbose                 : Be even louder
  -h|-?|--help                   : Print this message
)",
      argv[0]);
}

void ParseOpts(int argc, char const *argv[]) {
  std::vector<std::string> opt_split = {
      argv[0],
  };

  // split up -Dsymbol and -I/path/ style compiler flags so that the parser
  // below can be homogeneous while also accepting standard format of compiler
  // flags
  for (int opt_it = 1; opt_it < argc; opt_it++) {
    std::string arg = argv[opt_it];
    if (arg.length() > 2) {
      if (arg.substr(0, 2) == "-D") {
        opt_split.push_back("-D");
        opt_split.push_back(arg.substr(2));
      } else if (arg.substr(0, 2) == "-I") {
        opt_split.push_back("-I");
        opt_split.push_back(arg.substr(2));
      } else {
        opt_split.push_back(arg);
      }
    } else {
      opt_split.push_back(arg);
    }
  }

  for (int opt_it = 1; opt_it < opt_split.size(); opt_it++) {
    std::string arg = opt_split[opt_it];
    if ((arg == "-h") || (arg == "-?") || (arg == "--help")) {
      Usage(argv);
      exit(0);
    }

    if (arg == "--flat") {
      gen_flat = true;
      continue;
    } else if ((arg == "-v") || (arg == "--verbose")) {
      verbose = 1;
      continue;
    } else if ((arg == "-vv") || (arg == "--vverbose")) {
      verbose = 2;
      continue;
    } else if (arg == "--order-alphabetically") {
      order_alphabetically = true;
      continue;
    } else if (arg == "--emit-python-bindings") {
      emit_python = true;
      continue;
    }

    if ((opt_it + 1) < opt_split.size()) {
      if ((arg == "-i") || (arg == "--input")) {
        input_header = opt_split[++opt_it];
        continue;
      } else if ((arg == "-t") || (arg == "--target")) {
        target_class = opt_split[++opt_it];
        continue;
      } else if ((arg == "-o") || (arg == "--output")) {
        output_file = opt_split[++opt_it];
        continue;
      } else if ((arg == "-op") || (arg == "--output-path")) {
        output_path = opt_split[++opt_it];
        if (output_path.size() && (output_path.back() != '/')) {
          output_path += "/";
        }
        continue;
      } else if ((arg == "-od") || (arg == "--output-dir")) {
        output_dir = opt_split[++opt_it];
        if (output_dir.size() && (output_dir.back() != '/')) {
          output_dir += "/";
        }
        continue;
      } else if (arg == "--prolog") {
        prolog_file = opt_split[++opt_it];
        continue;
      } else if (arg == "--epilog") {
        epilog_file = opt_split[++opt_it];
        continue;
      } else if (arg == "--epilog-fwd") {
        epilog_fwd_file = opt_split[++opt_it];
        continue;
      } else if (arg == "-I") {
        includes.push_back(opt_split[++opt_it]);
        continue;
      } else if (arg == "-D") {
        defines.push_back(opt_split[++opt_it]);
        continue;
      } else if ((arg == "-p") || (arg == "--include-path")) {

        std::string ipath = opt_split[++opt_it];
        size_t colon = ipath.find_first_of(':');
        while (colon != std::string::npos) {
          if (colon != 0) {
            includes.push_back(ipath.substr(0, colon));
          }
          ipath = ipath.substr(colon + 1, std::string::npos);
          colon = ipath.find_first_of(':');
        }

        if (ipath.size()) {
          includes.push_back(ipath);
        }
        continue;
      }
    }

    if ((opt_it + 2) < opt_split.size()) {
      if (arg == "--extra") {
        std::string classname = opt_split[++opt_it];
        std::string deffile = opt_split[++opt_it];
        additional_class_files[classname] = deffile;
        continue;
      }
    }

    std::cout
        << "[ERROR]: Unknown option, or incorrect number of arguments for \""
        << arg << "\"" << std::endl;

    Usage(argv);
    exit(1);
  }
  if (!input_header.length() || !target_class.length() ||
      !output_file.length()) {
    std::cerr
        << "[ERROR]: Not all required options recieved: (-i, -t, -o, -op)."
        << std::endl;
    Usage(argv);
    exit(1);
  }
}

int main(int argc, char const *argv[]) {
  ParseOpts(argc, argv);

  // these "helpful" behaviors of TCling
  // are anything but
  gInterpreter->SetClassAutoloading(false);
  gInterpreter->SetClassAutoparsing(false);

  for (auto const &ip : includes) {
    if (verbose) {
      fmt::print("Adding include path: \"{}\"\n", ip);
    }
    gInterpreter->AddIncludePath(ip.c_str());
  }

  for (auto def : defines) {
    if (verbose) {
      fmt::print("Adding symbol definition: \"{}\"\n", def);
    }

    auto eq_pos = def.find_first_of('=');
    if (eq_pos != std::string::npos) {
      if (verbose) {
        std::string mdef = def;
        mdef[eq_pos] = ' ';
        fmt::print("  \"{}\" => \"{}\"\n", def, mdef);
      }
      def[eq_pos] = ' ';
    }

    gInterpreter->LoadText(fmt::format("#define {}", def).c_str());
  }

  if (verbose) {
    fmt::print("Interpreting header: \"{}\"\n", input_header);
  }

  if (!gInterpreter->LoadText(fmt::format("#include \"{}\"", input_header)
                                  .c_str())) { // returns int(true) on failure
    std::cout << "[ERROR]: TCling failed read: " << input_header << std::endl;
    return 1;
  }

  if (verbose) {
    fmt::print("Requesting RTTI for class: \"{}\"\n", target_class);
  }

  auto tcls = TClass::GetClass(target_class.c_str());

  if (!tcls) {
    std::cout << "[ERROR]: TCling failed to find class: " << target_class
              << " declaration in: " << input_header << std::endl;
    return 2;
  }

  for (auto const &acf : additional_class_files) {
    if (verbose) {
      fmt::print(
          "Loading additional implementation file: \"{}\" for class {}\n",
          acf.second, acf.first);
    }
    std::ifstream acf_file_stream(acf.second.c_str());
    if (!acf_file_stream.is_open()) {
      std::cout << "[ERROR]: Failed to read file: " << acf.second << std::endl;
      return 3;
    }
    std::stringstream ss;
    ss << acf_file_stream.rdbuf();
    additional_class_defintions[acf.first] = ss.str();
    additional_class_definition_used[acf.first] = false;
  }

  std::vector<std::string> Declarations;
  std::vector<std::string> EDeclarations;
  if (verbose) {
    fmt::print("Walking RTTI tree for class: \"{}\"\n", target_class);
  }
  WalkClass(tcls, Declarations, EDeclarations, "- ");

  if (prolog_file.size()) {
    if (verbose) {
      fmt::print("Reading prolog file: \"{}\"\n", prolog_file);
    }
    std::ifstream prolog_file_stream(prolog_file.c_str());
    if (!prolog_file_stream.is_open()) {
      std::cout << "[ERROR]: Failed to read file: " << prolog_file << std::endl;
      return 3;
    }
    std::stringstream ss;
    ss << prolog_file_stream.rdbuf();
    prolog_contents = ss.str();
  }

  if (epilog_file.size()) {
    if (verbose) {
      fmt::print("Reading epilog file: \"{}\"\n", epilog_file);
    }
    std::ifstream epilog_file_stream(epilog_file.c_str());
    if (!epilog_file_stream.is_open()) {
      std::cout << "[ERROR]: Failed to read file: " << epilog_file << std::endl;
      return 3;
    }
    std::stringstream ss;
    ss << epilog_file_stream.rdbuf();
    epilog_contents = ss.str();
  }

  if (epilog_fwd_file.size()) {
    if (verbose) {
      fmt::print("Reading epilog fwd file: \"{}\"\n", epilog_fwd_file);
    }
    std::ifstream epilog_fwd_file_stream(epilog_fwd_file.c_str());
    if (!epilog_fwd_file_stream.is_open()) {
      std::cout << "[ERROR]: Failed to read file: " << epilog_fwd_file
                << std::endl;
      return 3;
    }
    std::stringstream ss;
    ss << epilog_fwd_file_stream.rdbuf();
    epilog_fwd_contents = ss.str();
  }

  auto out_hdr = fmt::output_file(output_dir + output_file + ".h");
  auto out_impl = fmt::output_file(output_dir + output_file + ".cxx");
  auto out_fwd = fmt::output_file(output_dir + "FwdDeclare.h");
  std::unique_ptr<std::ofstream> out_pyb;
  if (emit_python) {
    out_pyb = std::make_unique<std::ofstream>(output_dir + output_file +
                                              ".pybind.cxx");

    (*out_pyb) << fmt::format(R"(#include "{0}"
#include "{1}.h"

#include "SRProxy/python/ProxyFileReader.txx"

#include "pybind11/pybind11.h"

namespace py = pybind11;

PYBIND11_MODULE(py{1}, m) {{
)",
                              input_header, output_file);
  }

  //   SRProxy Verion: {0}
  //   datetime: {1}
  //   host: {2}
  //   command: {3}
  std::stringstream command_buffer;
  for (int i = 0; i < argc; ++i) {
    command_buffer << argv[i] << ((i + 1 != argc) ? " " : "");
  }

  std::stringstream generator_host;
  char hostname[HOST_NAME_MAX];
  char username[LOGIN_NAME_MAX];
  bool have_user = false;
  if (!getlogin_r(username, LOGIN_NAME_MAX)) {
    generator_host << username;
    have_user = true;
  }
  if (!gethostname(hostname, HOST_NAME_MAX)) {
    generator_host << (have_user ? "@" : "") << hostname;
  }

  qualified_disclaimer =
      fmt::format(tmplt::disclaimer, SRProxy_VERSION, BUILD_ROOT_VERSION,
                  BUILD_ROOT_LIBRARY_DIR, fmt::gmtime(std::time(nullptr)),
                  generator_host.str(), command_buffer.str());

  out_hdr.print(qualified_disclaimer);
  out_impl.print(qualified_disclaimer);
  out_fwd.print(qualified_disclaimer);

  out_fwd.print(gen_flat ? tmplt::flat::fwd_prolog : tmplt::proxy::fwd_prolog);

  //{0} == Prolog
  //{1} == Outpath
  out_hdr.print(gen_flat ? tmplt::flat::hdr_prolog : tmplt::proxy::hdr_prolog,
                prolog_contents, output_path);

  //{0} == Header
  //{1} == Input
  out_impl.print(gen_flat ? tmplt::flat::cxx_prolog : tmplt::proxy::cxx_prolog,
                 fmt::format("{}{}.h", output_path, output_file), input_header);

  for (auto enumname : EDeclarations) {
    if (verbose) {
      fmt::print("Emitting explicit template instantiation for enum: \"{}\"\n",
                 enumname);
    }
    out_impl.print("template class {}<{}>;\n",
                   gen_flat ? "flat::Flat" : "caf::Proxy", enumname);
  }

  std::set<std::string> py_emitted_vector_types;
  for (auto classname : Declarations) {
    if (verbose) {
      fmt::print("Emitting proxy for class: \"{}\"\n", classname);
    }
    EmitClass(classname, out_hdr, out_impl, out_fwd, *out_pyb,
              py_emitted_vector_types);
  }

  if (emit_python) {

    (*out_pyb) << fmt::format(R"(
py::class_<ProxyFileReader<{0}>>(m, "{1}FileReader")
      .def(py::init<std::string const &, std::vector<std::string> const &>())
      .def("entries", &ProxyFileReader<{0}>::entries)
      .def("entry", &ProxyFileReader<{0}>::entry)
      .def(
          "iter",
          [](ProxyFileReader<{0}> &s) {{
            return py::make_iterator(begin(s), end(s));
          }},
          py::keep_alive<0, 1>());
)", target_class, GetClassName(target_class));

    (*out_pyb) << "}\n";
  }

  if (epilog_contents.size()) {
    if (verbose) {
      fmt::print("Writing epilog/\n");
    }
    out_hdr.print("{}", epilog_contents);
  }
  if (epilog_fwd_contents.size()) {
    if (verbose) {
      fmt::print("Writing epilog for fwd declare/\n");
    }
    out_fwd.print("{}", epilog_fwd_contents);
  }

  for (auto acu : additional_class_definition_used) {
    if (!acu.second) {
      std::cout << "[WARN]: --extra class argument: " << acu.first
                << " was not used." << std::endl;
    }
  }
}
