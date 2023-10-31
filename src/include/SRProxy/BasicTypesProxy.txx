#pragma once

#include "SRProxy/BasicTypesProxy.h"

#include "TError.h"
#include "TFile.h"
#include "TFormLeafInfo.h"
#include "TTreeFormula.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>

using namespace std::string_literals;
using std::isinf;
using std::isnan;

namespace {
/// Helper for CheckEquals
template <class T>
std::enable_if_t<std::is_floating_point<T>::value, bool> AreEqual(const T &x,
                                                                  const T &y) {
  return x == y || (std::isnan(x) && std::isnan(y));
}

template <class T>
std::enable_if_t<!std::is_floating_point<T>::value, bool> AreEqual(const T &x,
                                                                   const T &y) {
  return x == y;
}
} // namespace

namespace {
/// Helper class to track inf/nans encountered.
class InfNanTable {
private:
  struct Encounters {
    std::size_t count = 0; /// total number of times this var saw a NaN/inf
    std::string firstFile; /// first file a NaN/inf was seen in
    std::size_t firstEntry =
        std::numeric_limits<std::size_t>::max(); /// entry number within
                                                 /// firstFile where the first
                                                 /// NaN/inf was seen
  };

public:
  ~InfNanTable() noexcept { EmitTable(); }

  void LogInf(const std::string &varPath, const char *file, std::size_t entry) {
    Log(fInfEncounters, varPath, file, entry);
    CheckAbort();
  };
  void LogNaN(const std::string &varPath, const char *file, std::size_t entry) {
    Log(fNaNEncounters, varPath, file, entry);
    CheckAbort();
  };

  void EmitTable(std::ostream &stream = std::cerr) const noexcept {
    bool showedHeader = false;
    bool anyWarns = false;
    for (const auto encounters : {&fNaNEncounters, &fInfEncounters}) {
      if (encounters->empty())
        continue;

      anyWarns = true;
      if (!showedHeader) {
        stream << "\n\n\x1B[33mWARNING:\033[0m\n";
        showedHeader = true;
      }
      stream << "\n\x1B[33mSRProxy encountered "
             << (encounters == &fNaNEncounters ? "NaN" : "inf")
             << " in the following variables:\033[0m\n";
      for (const auto &encounterPair : *encounters) {
        stream << "  '\x1B[95m" << encounterPair.first << "\033[0m' ("
               << encounterPair.second.count << " encounters)\n";
        if (!encounterPair.second.firstFile.empty())
          stream << "     first encountered in entry "
                 << encounterPair.second.firstEntry
                 << " of file: " << encounterPair.second.firstFile << "\n";
      } // for (encounterPair)
    }   // for (encounters)

    if (anyWarns)
      stream << "\nSet environment variable SRPROXY_ABORT_ON_INFNAN=1 to "
                "instead abort immediately when inf/NaN is encountered.\n";
  } // InfNaNTable::EmitTable()

private:
  /// Set environment variable SRPROXY_ABORT_ON_INFNAN=1 to abort immediately
  /// when an inf or NaN is encountered
  void CheckAbort() const {
    // user can tell us to abort immediately if any NaNs/infs are found.
    // (useful for debugging)
    static bool checkedVar = false;
    if (!checkedVar) {
      if (auto val = getenv("SRPROXY_ABORT_ON_INFNAN")) {
        if (strcmp(val, "0") != 0) {
          EmitTable(std::cerr);
          std::cerr << "Aborting on first inf/NaN per configuration.  Unset "
                       "$SRPROXY_ABORT_ON_INFNAN to disable this behavior.\n";
          abort();
        }
      } // if ( val )
      checkedVar = true;
    } // if (!checkedVar)
  }   // CheckAbort()

  static void Log(std::map<std::string, Encounters> &encountersMap,
                  const std::string &varPath, const char *file,
                  std::size_t entry) {
    Encounters &encounters =
        encountersMap[varPath]; // will default-construct if not found
    if (encounters.count++ == 0) {
      encounters.firstFile = file;
      encounters.firstEntry = entry;
    }
  }

  std::map<std::string, Encounters> fNaNEncounters;
  std::map<std::string, Encounters> fInfEncounters;
};

InfNanTable infNanTable;
} // namespace

namespace caf {

static const long kDummyBaseUninit = -1;

//----------------------------------------------------------------------
template <class T>
Proxy<T>::Proxy(TTree *tr, const std::string &name, const long &base,
                int offset)
    : fName(name), fType(GetCAFType(tr)), fLeaf(0), fTree(tr), fBase(base),
      fOffset(offset), fLeafInfo(0), fBranch(0), fTTF(0), fEntry(-1),
      fSubIdx(0) {}

//----------------------------------------------------------------------
template <class T>
Proxy<T>::Proxy(const Proxy<T> &p)
    : fName("copy of " + p.fName), fType(kCopiedRecord), fLeaf(0), fTree(0),
      fBase(kDummyBaseUninit), fOffset(-1), fLeafInfo(0), fBranch(0), fTTF(0),
      fEntry(-1), fSubIdx(-1) {
  // Ensure that the value is evaluated and baked in in the parent object, so
  // that fTTF et al aren't re-evaluated in every single copy.
  fVal = p.GetValue();
}

//----------------------------------------------------------------------
template <class T>
Proxy<T>::Proxy(const Proxy &&p)
    : fName("move of " + p.fName), fType(kCopiedRecord), fLeaf(0), fTree(0),
      fBase(kDummyBaseUninit), fOffset(-1), fLeafInfo(0), fBranch(0), fTTF(0),
      fEntry(-1), fSubIdx(-1) {
  // Ensure that the value is evaluated and baked in in the parent object, so
  // that fTTF et al aren't re-evaluated in every single copy.
  fVal = p.GetValue();
}

//----------------------------------------------------------------------
template <class T> Proxy<T>::~Proxy() {
  // The other pointers aren't ours
  delete fTTF;
}

//----------------------------------------------------------------------
template <class T> T Proxy<T>::GetValue() const {
  switch (fType) {
  case kNested:
    return GetValueNested();
  case kFlat:
    return GetValueFlat();
  case kCopiedRecord:
    return (T)fVal;
  default:
    abort();
  }
}

//----------------------------------------------------------------------
template <class T>
std::enable_if_t<std::is_floating_point<T>::value, void>
CheckValue(T const &val, std::string fName, TTree *fTree, long fEntry) {
  const char *filename =
      (fTree && fTree->GetDirectory() && fTree->GetDirectory()->GetFile())
          ? fTree->GetDirectory()->GetFile()->GetName()
          : "";
  if (isnan(val))
    ::infNanTable.LogNaN(fName, filename, fEntry);
  else if (isinf(val))
    ::infNanTable.LogInf(fName, filename, fEntry);
}

template <class T>
std::enable_if_t<!std::is_floating_point<T>::value, void>
CheckValue(T const &, std::string, TTree *, long) {}

template <class T> T Proxy<T>::GetValueChecked() const {
  const T val = GetValue();

  CheckValue(val, fName, fTree, fEntry);

  return val;
}

//----------------------------------------------------------------------
template <class T> void GetTypedValueWrapper(TLeaf *leaf, T &x, int subidx) {
  x = leaf->GetTypedValue<T>(subidx);
}

//----------------------------------------------------------------------
template <> void GetTypedValueWrapper(TLeaf *leaf, std::string &x, int subidx) {
  assert(subidx == 0); // Unused for flat trees at least
  x = static_cast<char const *>(leaf->GetValuePointer());
}

//----------------------------------------------------------------------
template <class T> T Proxy<T>::GetValueFlat() const {
  assert(fTree);

  // Valid cached or systematically-shifted value
  if (fEntry == fTree->GetReadEntry())
    return (T)fVal;
  fEntry = fTree->GetReadEntry();

  if (!fLeaf) {
    const std::string sname = StripSubscripts(fName);
    // In a flat tree the branch and leaf have the same name, and this is
    // quicker than the naive TTree::GetLeaf()
    fBranch = fTree->GetBranch(sname.c_str());
    fLeaf = fBranch ? fBranch->GetLeaf(sname.c_str()) : 0;

    if (!fLeaf) {
      std::cout << std::endl
                << "BasicTypeProxy: Branch '" << sname
                << "' not found in tree '" << fTree->GetName() << "'."
                << std::endl;
      abort();
    }

    if (fName.find("..idx") == std::string::npos &&
        fName.find("..length") == std::string::npos) {
      SRBranchRegistry::AddBranch(sname);
    }
  }

  fBranch->GetEntry(fEntry);

  GetTypedValueWrapper(fLeaf, fVal, fBase + fOffset);

  return (T)fVal;
}

template <class T> void EvalInstanceWrapper(TTreeFormula *ttf, T &x) {
  // TODO is this the safest way to cast?
  x = static_cast<T>(ttf->EvalInstance(0));
}

template <> void EvalInstanceWrapper(TTreeFormula *ttf, std::string &x) {
  x = ttf->EvalStringInstance(0);
}

//----------------------------------------------------------------------
template <class T> T Proxy<T>::GetValueNested() const {
  assert(fTree);

  // Valid cached or systematically-shifted value
  if (fEntry == fTree->GetReadEntry())
    return (T)fVal;
  fEntry = fTree->GetReadEntry();

  // First time calling, set up the branches etc
  if (!fTTF) {
    SRBranchRegistry::AddBranch(fName);

    // Leaves are attached to the TTF, must keep it
    fTTF =
        new TTreeFormula(("TTFProxy-" + fName).c_str(), fName.c_str(), fTree);
    fLeafInfo = fTTF->GetLeafInfo(0); // Can fail (for a regular branch?)
    fLeaf = fTTF->GetLeaf(0);
    fBranch = fLeaf->GetBranch();

    if (!fLeaf || !fBranch) {
      std::cout << "Couldn't find " << fName << " in tree. Abort." << std::endl;

      abort();
    }

    // TODO - parsing the array indices out sucks - pass in as an int somehow
    const size_t open_idx = fName.find('[');
    // Do we have exactly one set of [] in the name?
    if (open_idx != std::string::npos && open_idx == fName.rfind('[')) {
      const size_t close_idx = fName.find(']');

      std::string numPart =
          fName.substr(open_idx + 1, close_idx - open_idx - 1);
      fSubIdx = atoi(numPart.c_str());
    }
  }

  if (fLeafInfo) {
    // Using TTreeFormula always works, and is sometimes necessary

    fTTF->GetNdata(); // for some reason this is necessary for fTTF to work
                      // in all cases.

    EvalInstanceWrapper(fTTF, fVal);
  } else {
    // But when this is possible the hope is it might be faster

    if (fBranch->GetReadEntry() != fEntry) {
      fBranch->GetEntry(fEntry);
    }

    // This check is much quicker than what CheckIndex() does, which winds up
    // calling a TTF, but I can't figure out a safe way to automatically
    // elide that check.
    if (fSubIdx > fLeaf->GetLen()) {
      std::cout << std::endl
                << fName << " out of range (" << fName
                << ".size() == " << fLeaf->GetLen() << "). Aborting."
                << std::endl;
      abort();
    }

    GetTypedValueWrapper(fLeaf, fVal, fSubIdx);
  }

  return (T)fVal;
}

//----------------------------------------------------------------------
template <class T> Proxy<T> &Proxy<T>::operator=(T x) {
  if (SRProxySystController::InTransaction())
    SRProxySystController::Backup(*this);
  fVal = x;

  switch (fType) {
  case kNested:
    fEntry = fTree->GetReadEntry();
    break;
  case kFlat:
    fEntry = fTree->GetReadEntry();
    break;
  case kCopiedRecord:
    break;
  default:
    abort();
  }

  return *this;
}

//----------------------------------------------------------------------

template <class T>
std::enable_if_t<std::is_same<T, bool>::value, T>
CheckSensiblePlusEqualsType(T const &, T const &) {
  std::cout << "Proxy<bool>::operator+=() is meaningless" << std::endl;
  abort();
}
template <class T>
std::enable_if_t<!std::is_same<T, bool>::value, T>
CheckSensiblePlusEqualsType(T const &a, T const &b) {
  return T(a + b);
}

template <class T> Proxy<T> &Proxy<T>::operator+=(T x) {

  // Do it this way to re-use the systematics logic in operator=
  *this = CheckSensiblePlusEqualsType(GetValue(), x);

  return *this;
}

//----------------------------------------------------------------------
template <class T>
std::enable_if_t<std::is_same<T, bool>::value, T>
CheckSensibleMinusEqualsType(T const &, T const &) {
  std::cout << "Proxy<bool>::operator-=() is meaningless" << std::endl;
  abort();
}
template <class T>
std::enable_if_t<std::is_same<T, std::string>::value, T>
CheckSensibleMinusEqualsType(T const &, T const &) {
  std::cout << "Proxy<std::string>::operator-=() is meaningless" << std::endl;
  abort();
}
template <class T>
std::enable_if_t<
    !(std::is_same<T, bool>::value || std::is_same<T, std::string>::value), T>
CheckSensibleMinusEqualsType(T const &a, T const &b) {
  return T(a - b);
}

template <class T> Proxy<T> &Proxy<T>::operator-=(T x) {

  // Do it this way to re-use the systematics logic in operator=
  *this = CheckSensibleMinusEqualsType(GetValue(), x);

  return *this;
}

//----------------------------------------------------------------------
template <class T>
std::enable_if_t<std::is_same<T, bool>::value, T>
CheckSensibleTimesEqualsType(T const &, T const &) {
  std::cout << "Proxy<bool>::operator*=() is meaningless" << std::endl;
  abort();
}
template <class T>
std::enable_if_t<std::is_same<T, std::string>::value, T>
CheckSensibleTimesEqualsType(T const &, T const &) {
  std::cout << "Proxy<std::string>::operator*=() is meaningless" << std::endl;
  abort();
}
template <class T>
std::enable_if_t<
    !(std::is_same<T, bool>::value || std::is_same<T, std::string>::value), T>
CheckSensibleTimesEqualsType(T const &a, T const &b) {
  return T(a * b);
}

template <class T> Proxy<T> &Proxy<T>::operator*=(T x) {

  // Do it this way to re-use the systematics logic in operator=
  *this = CheckSensibleTimesEqualsType(GetValue(), x);

  return *this;
}

//----------------------------------------------------------------------
template <class T> void Proxy<T>::CheckEquals(const T &x) const {
  if (!AreEqual(GetValue(), x)) {
    std::cout << fName << " differs: " << GetValue() << " vs " << x
              << std::endl;
  }
}

} // namespace caf
