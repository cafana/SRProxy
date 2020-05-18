#include "SRProxy/BasicTypesProxy.h"
#include "StandardRecord/SREnums.h" // TODO

#include "TFormLeafInfo.h"
#include "TTreeFormula.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>

namespace
{
  /// Helper for CheckEquals
  template<class T> bool AreEqual(const T& x, const T& y)
  {
    if constexpr(std::is_floating_point_v<T>){
      return x == y || (std::isnan(x) && std::isnan(y));
    }
    else{
      return x == y;
    }
  }
}

namespace caf
{
  long SRProxySystController::fgSeqNo = 0;
  bool SRProxySystController::fgAnyShifted = false;

  std::set<std::string> SRBranchRegistry::fgBranches;

  //----------------------------------------------------------------------
  void SRBranchRegistry::Print(bool abbrev)
  {
    std::string prev;
    for(std::string b: fgBranches){
      if(abbrev){
        unsigned int cutto = 0;
        for(unsigned int i = 0; i < std::min(b.size(), prev.size()); ++i){
          if(b[i] != prev[i]) break;
          if(b[i] == '.') cutto = i;
        }
        prev = b;
        for(unsigned int i = 0; i < cutto; ++i) b[i] = ' ';
      }
      std::cout << b << std::endl;
    }
  }

  //----------------------------------------------------------------------
  void SRBranchRegistry::ToFile(const std::string& fname)
  {
    std::ofstream fout(fname);
    for(const std::string& b: fgBranches) fout << b << std::endl;
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>::Proxy(TDirectory* d, TTree* tr, const std::string& name, const long& base, int offset)
    : fName(name), fLeaf(0), fTree(tr),
      fDir(d), fBase(base), fOffset(offset),
      fLeafInfo(0), fBranch(0), fTTF(0), fEntry(-1), fSubIdx(0),
      fSystOverrideEntry(-1), fSystOverrideSeqNo(-1)
  {
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>::Proxy(const Proxy<T>& p)
    : fName("copy of "+p.fName), fLeaf(0), fTree(p.fDir ? 0 : p.fTree),
      fDir(p.fDir), fBase(p.fBase), fOffset(p.fOffset),
      fLeafInfo(0), fBranch(0), fTTF(0), fSubIdx(-1),
      fSystOverrideValue(p.fSystOverrideValue),
      fSystOverrideEntry(p.fSystOverrideEntry),
      fSystOverrideSeqNo(p.fSystOverrideSeqNo)
  {
    // Ensure that the value is evaluated and baked in in the parent object, so
    // that fTTF et al aren't re-evaluated in every single copy.
    fVal = p.GetValue();
    fEntry = p.fEntry;
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>::Proxy(const Proxy&& p)
    : fName("move of "+p.fName), fLeaf(0), fTree(p.fDir ? 0 : p.fTree),
      fDir(p.fDir), fBase(p.fBase), fOffset(p.fOffset),
      fLeafInfo(0), fBranch(0), fTTF(0), fSubIdx(-1),
      fSystOverrideValue(p.fSystOverrideValue),
      fSystOverrideEntry(p.fSystOverrideEntry),
      fSystOverrideSeqNo(p.fSystOverrideSeqNo)
  {
    // Ensure that the value is evaluated and baked in in the parent object, so
    // that fTTF et al aren't re-evaluated in every single copy.
    fVal = p.GetValue();
    fEntry = p.fEntry;
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>::~Proxy()
  {
    // The other pointers aren't ours
    delete fTTF;
  }

  //----------------------------------------------------------------------
  template<class T> T Proxy<T>::GetValue() const
  {
    if(fDir) return GetValueFlat(); else return GetValueNested();
  }

  template<class T> void GetTypedValueWrapper(TLeaf* leaf, T& x, int subidx)
  {
    x = leaf->GetTypedValue<T>(subidx);
  }

  void GetTypedValueWrapper(TLeaf* leaf, std::string& x, int subidx)
  {
    assert(subidx == 0); // Unused for flat trees at least
    x = (char*)leaf->GetValuePointer();
  }

  //----------------------------------------------------------------------
  template<class T> T Proxy<T>::GetValueFlat() const
  {
    // If there's a valid systematic override value in place, give that
    if((!fTree || fSystOverrideEntry == fBase+fOffset) &&
       fSystOverrideSeqNo == SRProxySystController::CurrentSeqNo()){
      return fSystOverrideValue;
    }

    if(!fTree) return fVal; // copied or moved?

    if(!fLeaf){
      fLeaf = fTree->GetLeaf(fName.c_str());
      if(!fLeaf){
        std::cout << std::endl << "BasicTypeProxy: Branch '" << fName
                  << "' not found in tree '" << fTree->GetName() << "'."
                  << std::endl;
        abort();
      }

      fEntry = -1;
      fSystOverrideSeqNo = -1;

      if(fName.find("_idx") == std::string::npos &&
         fName.find("_length") == std::string::npos &&
         fName.find(".size()") == std::string::npos){ // specific to "nested"
        SRBranchRegistry::AddBranch(fName);
      }
    }

    if(fEntry == fBase+fOffset) return fVal; // cache up-to-date

    fLeaf->GetBranch()->GetEntry(fBase+fOffset);

    //    fVal = (T)fLeaf->GetTypedValue<U>(0);
    U tmp;
    GetTypedValueWrapper(fLeaf, tmp, 0);
    fVal = (T)tmp;

    fEntry = fBase+fOffset;
    fSystOverrideSeqNo = -1;

    return fVal;
  }

  template<class T> void EvalInstanceWrapper(TTreeFormula* ttf, T& x)
  {
    // TODO is this the safest way to cast?
    x = (T)ttf->EvalInstance(0);
  }

  void EvalInstanceWrapper(TTreeFormula* ttf, std::string& x)
  {
    x = ttf->EvalStringInstance(0);
  }

  //----------------------------------------------------------------------
  template<class T> T Proxy<T>::GetValueNested() const
  {
    // If there's a valid systematic override value in place, give that
    if((!fTree || fSystOverrideEntry == fTree->GetReadEntry()) &&
       fSystOverrideSeqNo == SRProxySystController::CurrentSeqNo()){
      return fSystOverrideValue;
    }

    // If there's no tree set but we've reached this point, either we were
    // constructed wrong, or no one has initialized our SystOverride
    // properly.
    assert(fTree);

    // If we have the value cached already, give that
    if(fEntry == fTree->GetReadEntry()) return fVal;

    // We're about to update to the current entry, and we won't be appropriate
    // for any systematic shift.
    fEntry = fTree->GetReadEntry();
    fSystOverrideSeqNo = -1;

    // First time calling, set up the branches etc
    if(!fTTF){
      SRBranchRegistry::AddBranch(fName);

      // Leaves are attached to the TTF, must keep it
      fTTF = new TTreeFormula(("TTFProxy-"+fName).c_str(), fName.c_str(), fTree);
      fLeafInfo = fTTF->GetLeafInfo(0); // Can fail (for a regular branch?)
      fLeaf = fTTF->GetLeaf(0);
      fBranch = fLeaf->GetBranch();

      if(!fLeaf || !fBranch){
        std::cout << "Couldn't find " << fName << " in tree. Abort."
                  << std::endl;

        abort();
      }

      // TODO - parsing the array indices out sucks - pass in as an int somehow
      const size_t open_idx = fName.find_first_of('[');
      // Do we have exactly one set of [] in the name?
      if(open_idx != std::string::npos && open_idx == fName.find_last_of('[')){
	const size_t close_idx = fName.find_first_of(']');

	std::string numPart = fName.substr(open_idx+1, close_idx-open_idx-1);
	fSubIdx = atoi(numPart.c_str());
      }
    }

    if(fLeafInfo){
      // Using TTreeFormula always works, and is sometimes necessary

      fTTF->GetNdata(); // for some reason this is necessary for fTTF to work
                        // in all cases.

      EvalInstanceWrapper(fTTF, fVal);
    }
    else{
      // But when this is possible the hope is it might be faster

      if(fBranch->GetReadEntry() != fEntry){
        fBranch->GetEntry(fEntry);
      }

      // This check is much quicker than what CheckIndex() does, which winds up
      // calling a TTF, but I can't figure out a safe way to automatically
      // elide that check.
      if(fSubIdx > fLeaf->GetLen()){
        std::cout << std::endl << fName << " out of range (size() == " << fLeaf->GetLen() << "). Aborting." << std::endl;
        abort();
      }

      //      fVal = (T)fLeaf->GetTypedValue<U>(fSubIdx);
      U tmp;
      GetTypedValueWrapper(fLeaf, tmp, fSubIdx);
      fVal = (T)tmp;
    }

    return fVal;
  }

  //----------------------------------------------------------------------
  template<class T> void Proxy<T>::SetShifted()
  {
    if(fDir){
      // Flat
      fSystOverrideEntry = fBase+fOffset;
    }
    else{
      // Nested
      fSystOverrideEntry = fTree ? fTree->GetReadEntry() : 0;
    }
    fSystOverrideSeqNo = SRProxySystController::CurrentSeqNo();
    SRProxySystController::SetShifted();
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>& Proxy<T>::operator=(T x)
  {
    fSystOverrideValue = x;
    SetShifted();
    return *this;
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>& Proxy<T>::operator+=(T x)
  {
    if constexpr(!std::is_same_v<T, bool>){
      fSystOverrideValue = T(GetValue() + x);
      SetShifted();
    }
    else{
      std::cout << "Proxy<bool>::operator+=() is meaningless" << std::endl;
      (void)x;
      abort();
    }

    return *this;
  }

  //----------------------------------------------------------------------
  template<class T> Proxy<T>& Proxy<T>::operator*=(T x)
  {
    if constexpr(!std::is_same_v<T, bool>){
        fSystOverrideValue = T(GetValue() * x);
      SetShifted();
    }
    else{
      std::cout << "Proxy<bool>::operator*=() is meaningless" << std::endl;
      (void)x;
      abort();
    }

    return *this;
  }

  //----------------------------------------------------------------------
  template<> Proxy<std::string>& Proxy<std::string>::operator*=(std::string x)
  {
    std::cout << "BasicTypesProxy.cxx: Multiplying strings makes no sense..." << std::endl;
    abort();
  }

  //----------------------------------------------------------------------
  template<class T> void Proxy<T>::CheckEquals(const T& x) const
  {
    if(!AreEqual(GetValue(), x)){
      std::cout << fName << " differs: "
                << GetValue() << " vs " << x << std::endl;
    }
  }

  //----------------------------------------------------------------------
  VectorProxyBase::VectorProxyBase(TDirectory* d, TTree* tr,
                                   const std::string& name,
                                   const long& base, int offset)
    : fDir(d), fTree(tr), fName(name), fBase(base), fOffset(offset), fSize(d, tr, fDir ? fName+"_length" : AtSize(), base, offset), fIdxP(d, tr, name+"_idx", base, offset),
      fSystOverrideSize(-1),
      fSystOverrideEntry(-1),
      fSystOverrideSeqNo(-1),
      fWarn(false)
  {
  }

  //----------------------------------------------------------------------
  void VectorProxyBase::CheckIndex(size_t i) const
  {
    // This is the only way to get good error messages. But it also seems like
    // the call to size() here is necessary in Nested mode to trigger some
    // side-effect within ROOT, otherwise we get some bogus index out-of-range
    // crashes.
    if(i >= size()){
      std::cout << std::endl << fName << "[" << (signed)i << "] out of range (size() == " << size() << "). Aborting." << std::endl;
      abort();
    }
  }

  //----------------------------------------------------------------------
  std::string StripIndices(const std::string& s)
  {
    const size_t idx1 = s.find_first_of('[');

    if(idx1 == std::string::npos) return s;

    const size_t idx2 = s.find_first_of(']');

    // Huh?
    if(idx2 == std::string::npos) return s;

    // Recurse in case there are more
    return StripIndices(s.substr(0, idx1) + s.substr(idx2+1));
  }

  //----------------------------------------------------------------------
  std::string VectorProxyBase::NName() const
  {
    const int idx = fName.find_last_of('.');
    // foo.bar.baz -> foo.bar.nbaz
    return fName.substr(0, idx)+".n"+fName.substr(idx+1);
  }

  //----------------------------------------------------------------------
  // Used by nested variant
  std::string VectorProxyBase::AtSize() const
  {
    // Counts exist, but with non-systematic names
    if(fName == "rec.me.trkkalman"  ) return "rec.me.nkalman";
    if(fName == "rec.me.trkdiscrete") return "rec.me.ndiscrete";
    if(fName == "rec.me.trkcosmic"  ) return "rec.me.ncosmic";
    if(fName == "rec.me.trkbpf"     ) return "rec.me.nbpf";

    const int idx = fName.find_last_of('.');
    // foo.bar.baz -> foo.bar.nbaz
    const std::string nname = NName();

    int olderr = gErrorIgnoreLevel;
    gErrorIgnoreLevel = 99999999;
    TTreeFormula ttf(("TTFProxySize-"+fName).c_str(), nname.c_str(), fTree);
    TString junks = nname.c_str();
    int junki;
    const int def = ttf.DefinedVariable(junks, junki);
    gErrorIgnoreLevel = olderr;

    if(def >= 0) return nname;

    // Otherwise fallback and make a note to warn the first time we're accessed
    fWarn = true;

    // foo.bar.baz -> foo.bar.@baz.size()
    return fName.substr(0, idx+1)+"@"+fName.substr(idx+1)+".size()";
  }

  //----------------------------------------------------------------------
  // Used by nested variant
  std::string VectorProxyBase::Subscript(int i) const
  {
    // Only have to do the at() business for subscripts from the 3rd one on
    const int nSubs = std::count(fName.begin(), fName.end(), '[');
    if(nSubs < 2) return TString::Format("%s[%d]", fName.c_str(), i).Data();

    const int idx = fName.find_last_of('.');
    return TString::Format("%s.@%s.at(%d)",
                           fName.substr(0, idx).c_str(),
                           fName.substr(idx+1).c_str(),
                           i).Data();
  }

  //----------------------------------------------------------------------
  TTree* VectorProxyBase::GetTreeForName() const
  {
    TTree* tr = (TTree*)fDir->Get(fName.c_str());
    if(!tr){
      std::cout << "Couldn't find TTree " << fName
                << " in " << fDir->GetName() << std::endl;
      abort();
    }
    return tr;
  }

  //----------------------------------------------------------------------
  size_t VectorProxyBase::size() const
  {
    if(fWarn){
      fWarn = false;

      // Don't emit the same warning more than once
      static std::set<std::string> already;

      const std::string key = StripIndices(NName());
      if(already.count(key) == 0){
        already.insert(key);
        std::cout << std::endl;
        std::cout << "Warning: field '" << key << "' does not exist in file. "
                  << "Falling back to '" << StripIndices(fSize.Name()) << "' which is less efficient. "
                  << "Consider updating StandardRecord to include '" << key << "'." << std::endl;
        std::cout << std::endl;
      }
    }


    // If there's a valid systematic override value in place, give that
    if(fDir){
      // Flat
      if((!fTree || fSystOverrideEntry == fBase+fOffset) &&
         fSystOverrideSeqNo == SRProxySystController::CurrentSeqNo()){
        return fSystOverrideSize;
      }
    }
    else{
      // Nested
      if((!fTree || fSystOverrideEntry == fTree->GetReadEntry()) &&
         fSystOverrideSeqNo == SRProxySystController::CurrentSeqNo()){
        return fSystOverrideSize;
      }
    }

    return fSize;
  }

  //----------------------------------------------------------------------
  bool VectorProxyBase::empty() const
  {
    return size() == 0;
  }

  //----------------------------------------------------------------------
  void VectorProxyBase::resize(size_t i)
  {
    fSystOverrideSize = i;
    if(fDir){
      // Flat
      fSystOverrideEntry = fBase+fOffset;
    }
    else{
      // Nested
      fSystOverrideEntry = fTree ? fTree->GetReadEntry() : 0;
    }
    fSystOverrideSeqNo = SRProxySystController::CurrentSeqNo();
    SRProxySystController::SetShifted();
  }

  // Enumerate all the variants we expect
  template class Proxy<int>;
  template class Proxy<long>;
  template class Proxy<unsigned int>;
  template class Proxy<unsigned short>;
  template class Proxy<unsigned long>;
  template class Proxy<long long>;
  template class Proxy<unsigned long long>;
  template class Proxy<short>;
  template class Proxy<float>;
  template class Proxy<double>;
  template class Proxy<bool>;
  template class Proxy<unsigned char>;
  template class Proxy<std::string>;

  template class Proxy<Det_t>;
  template class Proxy<View_t>;
  template class Proxy<gen_process_t>;
  template class Proxy<generator_>;
  template class Proxy<mode_type_>;
  template class Proxy<int_type_>;

} // namespace
