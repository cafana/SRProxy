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

std::string GetVectorValueTypeName(std::string classname) {
  auto openb = classname.find_first_of('<');
  auto closeb = classname.find_last_of('>');
  return classname.substr(openb + 1, closeb - openb - 1);
}

bool IsSTLVector(TDataMember &dm) {
  return (dm.IsSTLContainer() == TDictionary::kVector);
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

void WalkClass(TClass *cls, std::vector<std::string> &Declarations,
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
      WalkClass(TClass::GetClass(vvt.c_str()), Declarations, indent + "- ");
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
    WalkClass(bcls, Declarations, indent + "- ");
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
        WalkClass(TClass::GetClass(vvt.c_str()), Declarations, indent + "- ");
      }

    } else if (!IsStandardTypeOrEnum(dm)) {
      if (verbose) {
        fmt::print("{}Walking RTTI tree for class: \"{}\"\n", indent,
                   dm.GetTypeName());
      }
      WalkClass(TClass::GetClass(dm.GetTypeName()), Declarations,
                indent + "- ");
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
               fmt::ostream &out_impl, fmt::ostream &out_fwd) {

  std::stringstream inits;
  std::stringstream memberlist;

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
  }

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

    std::string mname = dm.GetName();

    if (base_members.count(
            mname)) { // Don't re-declare members of the base class
      continue;
    }

    std::string mptype = fmt::format(
        gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", GetTypeName(dm));

    inits << fmt::format(
        gen_flat ? tmplt::flat::member_init : tmplt::proxy::member_init, mname);

    memberlist << fmt::format(tmplt::member_list, mptype, mname);

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
}

void Usage(char const *argv[]) {
  fmt::print(R"([USAGE] {}  -i <header_file> -t <classname> -o <filename_stub> [args]

Required arguments:
  -i|--input <header_file>       : The C++ header file that defines the class
  -t|--target <classname>        : The class to generate a proxy for
  -o|--output <filename stub>    : Output filename stub

Optional arguments:
  -op|--output-path <path>       : A path to prepend to include statements in generated headers
  -od|--output-dir <path>        : The directory to write generated files to
  --prolog <file path>           : A file to include before the generated proxy class defintion
  --epilog <file path>           : A file to include after the generated proxy class definition
  --epilog-fwd <file path>       : A file to include after the list of generated forward declarations
  -I <path>                      : A directory to add to the include path
  -p|--include-path <path1[:p2]> : A PATH-like colon-separate list of directories to add to the include path
  --extra <classname> <file>     : A file to include in the definition of the proxy class for class <classname>
  --flat                         : Generate a 'flat' file reader rather than the objectified proxy class
  --order-alphabetically         : Emit datamembers in alphabetic, rather than declaration, order.

  -v|--verbose                   : Be louder
  -vv|--vverbose                 : Be even louder
  -h|-?|--help                   : Print this message
)",
             argv[0]);
}

void ParseOpts(int argc, char const *argv[]) {
  for (int opt_it = 1; opt_it < argc; opt_it++) {
    std::string arg = argv[opt_it];
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
    }

    if ((opt_it + 1) < argc) {
      if ((arg == "-i") || (arg == "--input")) {
        input_header = argv[++opt_it];
        continue;
      } else if ((arg == "-t") || (arg == "--target")) {
        target_class = argv[++opt_it];
        continue;
      } else if ((arg == "-o") || (arg == "--output")) {
        output_file = argv[++opt_it];
        continue;
      } else if ((arg == "-op") || (arg == "--output-path")) {
        output_path = argv[++opt_it];
        if (output_path.size() && (output_path.back() != '/')) {
          output_path += "/";
        }
        continue;
      } else if ((arg == "-od") || (arg == "--output-dir")) {
        output_dir = argv[++opt_it];
        if (output_dir.size() && (output_dir.back() != '/')) {
          output_dir += "/";
        }
        continue;
      } else if (arg == "--prolog") {
        prolog_file = argv[++opt_it];
        continue;
      } else if (arg == "--epilog") {
        epilog_file = argv[++opt_it];
        continue;
      } else if (arg == "--epilog-fwd") {
        epilog_fwd_file = argv[++opt_it];
        continue;
      } else if (arg == "-I") {
        includes.push_back(argv[++opt_it]);
        continue;
      } else if ((arg == "-p") || (arg == "--include-path")) {

        std::string ipath = argv[++opt_it];
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

    if ((opt_it + 2) < argc) {
      if (arg == "--extra") {
        std::string classname = argv[++opt_it];
        std::string deffile = argv[++opt_it];
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

  for (auto const &ip : includes) {
    if (verbose) {
      fmt::print("Adding include path: \"{}\"\n", ip);
    }
    gInterpreter->AddIncludePath(ip.c_str());
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
  if (verbose) {
    fmt::print("Walking RTTI tree for class: \"{}\"\n", target_class);
  }
  WalkClass(tcls, Declarations, "- ");

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

  for (auto classname : Declarations) {
    if (verbose) {
      fmt::print("Emitting proxy for class: \"{}\"\n", classname);
    }
    EmitClass(classname, out_hdr, out_impl, out_fwd);
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