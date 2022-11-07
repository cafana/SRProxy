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

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

#include <climits>
#include <unistd.h>

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

bool IsBasic(TDataMember &dm) {
  return (dm.IsBasic() || (std::string(dm.GetTypeName()) == "string"));
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

  return QualifystdNS(tn.str());
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

void WalkClass(TClass *cls, std::vector<std::string> &Declarations) {
  if (!cls) {
    std::cout << "[ERROR]: WalkClass was passed a nullptr." << std::endl;
    abort();
  }

  // We have already walked this type
  if (std::find(Declarations.begin(), Declarations.end(), cls->GetName()) !=
      Declarations.end()) {
    return;
  }

  if (IsSTLVector(cls)) { // this enables datamembers that are vectors of
                          // vectors to be properly processed
    auto vvt = GetVectorValueTypeName(cls->GetName());
    if (!KnownType(vvt)) {
      std::cout << "[ERROR]: TCling has no typeinfo for " << vvt << std::endl;
      abort();
    }

    if (KnownClass(vvt)) { // If the contained type is a class (as opposed to a
                           // primitive), then we should check that we know how
                           // to proxy the vector value type
      WalkClass(TClass::GetClass(vvt.c_str()), Declarations);
    }

    // We don't need to emit a proxy class for the vector template itself
    return;
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
    WalkClass(bcls, Declarations);
  }

  // Loop through this classes public data members, checking if we need to emit
  // proxies for any of their types
  for (auto dm_to : *cls->GetListOfAllPublicDataMembers()) {
    auto &dm = dynamic_cast<TDataMember &>(*dm_to);

    if (!dm.IsValid()) {
      std::cout << "[ERROR]: Failed to read type for data member "
                << dm.GetName() << " of class " << cls->GetName() << std::endl;
      abort();
    }

    // If this data member's type is not a primitive, then we need to check if
    // we need to emit a proxy class for it, either the type itself, or the
    // value type of an STL vector.
    if (IsSTLVector(dm)) {

      auto vvt = GetVectorValueTypeName(dm.GetTypeName());
      if (!KnownType(vvt)) {
        std::cout << "[ERROR]: TCling has no typeinfo for " << vvt << std::endl;
        abort();
      }

      if (KnownClass(vvt)) { // If the contained type is a class (as opposed to
                             // a primitive), then we should check that we know
                             // how to proxy the vector value type
        WalkClass(TClass::GetClass(vvt.c_str()), Declarations);
      }

    } else if (!IsBasic(dm)) {
      WalkClass(TClass::GetClass(dm.GetTypeName()), Declarations);
    }
  }

  // Add this type to the list of types if we don't already know about it
  if (std::find(Declarations.begin(), Declarations.end(), cls->GetName()) ==
      Declarations.end()) {
    Declarations.push_back(cls->GetName());
  }
}

std::string input_header, target_class;
std::string output_file;
std::vector<std::string> includes;
std::string output_path;
std::string prolog_file, epilog_file, epilog_fwd_file;
std::string prolog_contents, epilog_contents, epilog_fwd_contents;

bool gen_flat = false;

std::string qualified_disclaimer;

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

  for (auto dm_to : *cls->GetListOfAllPublicDataMembers()) {
    auto &dm = dynamic_cast<TDataMember &>(*dm_to);

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
      tmplt::fwd_body, GetNS(classname), GetClassName(classname),
      gen_flat ? GetShortFlatType(classname) : GetShortProxyType(classname),
      fmt::format(gen_flat ? "flat::Flat<{}>" : "caf::Proxy<{}>", classname));

  //{0} == Type
  //{1} == ProxyType
  //{2} == BaseClass
  //{3} == Members
  out_hdr.print(gen_flat ? tmplt::flat::hdr_body : tmplt::proxy::hdr_body, type,
                ptype, base_declaration, memberlist.str());

  //{0} == ProxyType
  //{1} == Inits
  //{2} == Type
  //{3} == AssignBody
  //{4} == CheckEqualsBody
  out_impl.print(gen_flat ? tmplt::flat::cxx_body : tmplt::proxy::cxx_body,
                 ptype,
                 // Trim off the last newline and comma from this list
                 inits.str().substr(0, inits.str().length() - 2), type,
                 (gen_flat ? fill_body : assign_body).str(),
                 (gen_flat ? clear_body : checkequals_body).str());
}

void Usage(char const *argv[]) {
  std::cout << "[USAGE]: " << argv[0] << std::endl;
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
    }

    if ((opt_it + 1) < argc) {
      if ((arg == "-i") || (arg == "--input")) {
        input_header = argv[++opt_it];
      } else if ((arg == "-t") || (arg == "--target")) {
        target_class = argv[++opt_it];
      } else if ((arg == "-o") || (arg == "--output")) {
        output_file = argv[++opt_it];
      } else if ((arg == "-op") || (arg == "--output-path")) {
        output_path = argv[++opt_it];
        if (output_path.size() && (output_path.back() != '/')) {
          output_path += "/";
        }
      } else if (arg == "--prolog") {
        prolog_file = argv[++opt_it];
      } else if (arg == "--epilog") {
        epilog_file = argv[++opt_it];
      } else if (arg == "--epilog-fwd") {
        epilog_fwd_file = argv[++opt_it];
      } else if (arg == "-I") {
        includes.push_back(argv[++opt_it]);
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

      } else {
        Usage(argv);
        exit(1);
      }
      continue;
    }

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
    gInterpreter->AddIncludePath(ip.c_str());
  }

  if (!gInterpreter->LoadText(fmt::format("#include \"{}\"", input_header)
                                  .c_str())) { // returns int(true) on failure
    std::cout << "[ERROR]: TCling failed read: " << input_header << std::endl;
    return 1;
  }

  auto tcls = TClass::GetClass(target_class.c_str());

  if (!tcls) {
    std::cout << "[ERROR]: TCling failed to find class: " << target_class
              << " declaration in: " << input_header << std::endl;
    return 2;
  }

  std::vector<std::string> Declarations;
  WalkClass(tcls, Declarations);

  if (prolog_file.size()) {
    std::ifstream prolog_file_stream(prolog_file.c_str());
    std::stringstream ss;
    ss << prolog_file_stream.rdbuf();
    prolog_contents = ss.str();
  }

  if (epilog_file.size()) {
    std::ifstream epilog_file_stream(epilog_file.c_str());
    std::stringstream ss;
    ss << epilog_file_stream.rdbuf();
    epilog_contents = ss.str();
  }

  if (epilog_fwd_file.size()) {
    std::ifstream epilog_fwd_file_stream(epilog_fwd_file.c_str());
    std::stringstream ss;
    ss << epilog_fwd_file_stream.rdbuf();
    epilog_fwd_contents = ss.str();
  }

  auto out_hdr = fmt::output_file(output_file + ".h");
  auto out_impl = fmt::output_file(output_file + ".cxx");
  auto out_fwd = fmt::output_file("FwdDeclare.h");

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

  qualified_disclaimer = fmt::format(
      tmplt::disclaimer, SRProxy_VERSION, fmt::gmtime(std::time(nullptr)),
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
                 fmt::format("{}{}.h", output_path, output_file),
                 input_header);

  for (auto classname : Declarations) {
    EmitClass(classname, out_hdr, out_impl, out_fwd);
  }
}