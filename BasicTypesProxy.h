#pragma once

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "TString.h"

class TDirectory;
class TFormLeafInfo;
class TBranch;
class TLeaf;
class TTreeFormula;
class TTree;

namespace caf
{
  class SRProxySystController
  {
  public:
    static void ResetSysts()
    {
      if(fgAnyShifted){
        ++fgSeqNo;
        fgAnyShifted = false;
      }
    }
    static bool AnyShifted() {return fgAnyShifted;}
  protected:
    template<class T> friend class Proxy;
    friend class VectorProxyBase;

    static long CurrentSeqNo() {return fgSeqNo;};
    static void SetShifted() {fgAnyShifted = true;}

    static long fgSeqNo;
    static bool fgAnyShifted;
  };

  class SRBranchRegistry
  {
  public:
    static void AddBranch(const std::string& b){fgBranches.insert(b);}
    static const std::set<std::string>& GetBranches(){return fgBranches;}
    static void clear() {fgBranches.clear();}

    static void Print(bool abbrev = true);
    static void ToFile(const std::string& fname);
  protected:
    static std::set<std::string> fgBranches;
  };

  template<class T> class Proxy;

  template<class T> class Proxy
  {
  public:
    static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, std::string>, "Invalid type for basic type Proxy");

    Proxy(TDirectory* d, TTree* tr, const std::string& name, const long& base, int offset);

    // Need to be copyable because Vars return us directly
    Proxy(const Proxy&);
    Proxy(const Proxy&&);
    // No need to be assignable though
    Proxy& operator=(const Proxy&) = delete;

    // Somehow including this helps us not get automatically converted to a
    // type we might not want to be in ternary expressions (we now get a type
    // error instead).
    Proxy(T v) = delete;

    ~Proxy();

    operator T() const {return GetValue();}

    T GetValue() const;

    // In practice these are the only operations that systematic shifts use
    Proxy<T>& operator=(T x);
    Proxy<T>& operator+=(T x);
    Proxy<T>& operator*=(T x);

    std::string Name() const {return fName;}

    void CheckEquals(const T& x) const;

  protected:
    T GetValueFlat() const;
    T GetValueNested() const;

    void SetShifted();

    // The type to fetch from the TLeaf - get template errors inside of ROOT
    // for enums.
    typedef typename std::conditional_t<std::is_enum_v<T>, int, T> U;

    // Shared
    std::string fName;
    mutable TLeaf* fLeaf;
    mutable T fVal;
    TTree* fTree;

    // Flat
    TDirectory* fDir;
    const long& fBase;
    int fOffset;

    // Nested
    mutable TFormLeafInfo* fLeafInfo;
    mutable TBranch* fBranch;
    mutable TTreeFormula* fTTF;
    mutable long fEntry;
    mutable int fSubIdx;

    // Syst
    T fSystOverrideValue;
    mutable long fSystOverrideEntry;
    mutable long fSystOverrideSeqNo;
  };

  // Helper functions that don't need to be templated
  class VectorProxyBase
  {
  public:
    VectorProxyBase(TDirectory* d, TTree* tr, const std::string& name,
                    const long& base, int offset);

    VectorProxyBase& operator=(const VectorProxyBase&) = delete;
    VectorProxyBase(const VectorProxyBase& v) = delete;

    std::string Name() const {return fName;}

    size_t size() const;
    bool empty() const;
    void resize(size_t i);
  protected:
    void CheckIndex(size_t i) const;
    TTree* GetTreeForName() const;

    // Used by nested variant
    std::string AtSize() const;
    std::string Subscript(int i) const;

    /// Helper for AtSize()
    std::string NName() const;

    TDirectory* fDir;
    TTree* fTree;
    std::string fName;
    const long& fBase;
    int fOffset;
    Proxy<int> fSize;
    Proxy<long long> fIdxP;
    mutable long fIdx;

    size_t fSystOverrideSize;
    mutable long fSystOverrideEntry;
    mutable long fSystOverrideSeqNo;

    mutable bool fWarn;
  };


  template<class T> class Proxy<std::vector<T>>: public VectorProxyBase
  {
  public:
    Proxy(TDirectory* d, TTree* tr, const std::string& name, const long& base, int offset)
      : VectorProxyBase(d, tr, name, base, offset)
    {
    }

    ~Proxy(){for(Proxy<T>* e: fElems) delete e;}

    Proxy& operator=(const Proxy<std::vector<T>>&) = delete;
    Proxy(const Proxy<std::vector<T>>& v) = delete;

    Proxy<T>& at(size_t i) const {EnsureSize(i); return *fElems[i];}
    Proxy<T>& at(size_t i)       {EnsureSize(i); return *fElems[i];}

    Proxy<T>& operator[](size_t i) const {return at(i);}
    Proxy<T>& operator[](size_t i)       {return at(i);}

    template<class U> Proxy<std::vector<T>>& operator=(const std::vector<U>& x)
    {
      resize(x.size());
      for(unsigned int i = 0; i < x.size(); ++i) at(i) = x[i];
      return *this;
    }

    template<class U>
    void CheckEquals(const std::vector<U>& x) const
    {
      fSize.CheckEquals(x.size());
      for(unsigned int i = 0; i < std::min(size(), x.size()); ++i) at(i).CheckEquals(x[i]);
    }


    // U should be either T or const T
    template<class U> class iterator
    {
    public:
      Proxy<T>& operator*() {return (*fParent)[fIdx];}
      iterator<U>& operator++(){++fIdx; return *this;}
      bool operator!=(const iterator<U>& it) const {return fIdx != it.fIdx;}
      bool operator==(const iterator<U>& it) const {return fIdx == it.fIdx;}
    protected:
      friend class Proxy<std::vector<T>>;
      iterator(const Proxy<std::vector<T>>* p, int i) : fParent(p), fIdx(i) {}

      const Proxy<std::vector<T>>* fParent;
      size_t fIdx;
    };

    iterator<const T> begin() const {return iterator<const T>(this, 0);}
    iterator<T> begin() {return iterator<T>(this, 0);}
    iterator<const T> end() const {return iterator<const T>(this, size());}
    iterator<T> end() {return iterator<T>(this, size());}

  protected:
    /// Implies CheckIndex()
    void EnsureSize(size_t i) const
    {
      CheckIndex(i);
      if(i >= fElems.size()) fElems.resize(i+1);

      if(fDir){
        // Flat
        fIdx = fIdxP; // store into an actual value we can point to
        if(!fElems[i]){
          fElems[i] = new Proxy<T>(fDir, GetTreeForName(), fName, fIdx, i);
        }
      }
      else{
        // Nested
        if(!fElems[i]) fElems[i] = new Proxy<T>(0, fTree, Subscript(i), 0, 0);
      }
    }

    mutable std::vector<Proxy<T>*> fElems;
  };

  // Retain an alias to the old naming scheme for now
  template <class T> using VectorProxy = Proxy<std::vector<T>>;


  /// Used in comparison of GENIE version numbers
  template<class T> bool operator<(const Proxy<std::vector<T>>& a,
                                   const std::vector<T>& b)
  {
    const size_t N = a.size();
    if(N != b.size()) return N < b.size();
    for(size_t i = 0; i < N; ++i){
      if(a[i] != b[i]) return a[i] < b[i];
    }
    return false;
  }

  template<class T, unsigned int N> class Proxy<T[N]>
  {
  public:
    Proxy(TDirectory* d, TTree* tr, const std::string& name, const long& base, int offset)
      : fFlat(d != 0), fIdxP(Proxy<long long>(d, tr, name+"_idx", base, offset))
    {
      fElems.reserve(N);
      for(unsigned int i = 0; i < N; ++i){
        if(fFlat){
          fElems.emplace_back(d, tr, name, fIdx, i);
        }
        else{
          // Nested
          fElems.emplace_back(nullptr, tr, TString::Format("%s[%d]", name.c_str(), i).Data(), 0, 0);
        }
      }
    }

    Proxy& operator=(const Proxy<T[N]>&) = delete;
    Proxy(const Proxy<T[N]>& v) = delete;

    const Proxy<T>& operator[](size_t i) const {if(fFlat) fIdx = fIdxP; return fElems[i];}
          Proxy<T>& operator[](size_t i)       {if(fFlat) fIdx = fIdxP; return fElems[i];}

    Proxy<T[N]>& operator=(const T (&x)[N])
    {
      for(unsigned int i = 0; i < N; ++i) (*this)[i] = x[i];
      return *this;
    }

    void CheckEquals(const T (&x)[N]) const
    {
      for(unsigned int i = 0; i < N; ++i) (*this)[i].CheckEquals(x[i]);
    }

  protected:
    std::vector<Proxy<T>> fElems;

    // Flat
    bool fFlat;
    Proxy<long long> fIdxP;
    mutable long fIdx;
  };

  // Retain an alias to the old naming scheme for now
  template <class T, unsigned int N> using ArrayProxy = Proxy<T[N]>;

} // namespace

namespace std
{
  template<class T> T min(const caf::Proxy<T>& a, T b)
  {
    return std::min(a.GetValue(), b);
  }

  template<class T> T min(T a, const caf::Proxy<T>& b)
  {
    return std::min(a, b.GetValue());
  }

  template<class T> T max(const caf::Proxy<T>& a, T b)
  {
    return std::max(a.GetValue(), b);
  }

  template<class T> T max(T a, const caf::Proxy<T>& b)
  {
    return std::max(a, b.GetValue());
  }
}
