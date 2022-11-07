#pragma once

#include "SRProxy/IBranchPolicy.h"

#include "TTree.h"

#include <string>
#include <vector>

namespace flat
{
  //----------------------------------------------------------------------
  template<class T> struct rootcode;
  template<> struct rootcode<         char>{const static char code = 'B';};
  template<> struct rootcode<unsigned char>{const static char code = 'b';};
  template<> struct rootcode<        short>{const static char code = 'S';};
  template<> struct rootcode<short unsigned int>{const static char code = 's';};
  template<> struct rootcode<          int>{const static char code = 'I';};
  template<> struct rootcode< unsigned int>{const static char code = 'i';};
  template<> struct rootcode<        float>{const static char code = 'F';};
  template<> struct rootcode<       double>{const static char code = 'D';};
  template<> struct rootcode<         long>{const static char code = 'L';};
  template<> struct rootcode<unsigned long>{const static char code = 'l';};
  template<> struct rootcode<         bool>{const static char code = 'O';};

  template<class T> struct is_vec                {static const bool value = false;};
  template<class T> struct is_vec<std::vector<T>>{static const bool value = true; };

  // Serialize most types as-is, but represent enums with short
  template<class T> struct FlatType{typedef std::conditional_t<std::is_enum_v<T>, short, T> type;};
  /// Prevent bit-packed vector<bool>
  template<> struct FlatType<bool>{typedef char type;};
  // TTree can't handle long long at all? This will lose information though...
  template<> struct FlatType<long long int>{typedef int type;};

  template<class T> class Flat
  {
    static_assert(std::is_arithmetic_v<typename FlatType<T>::type>, "Invalid type for basic type Flat");

  public:
    Flat(TTree* tr, const std::string& name, const std::string& totsize, const IBranchPolicy* policy)
      : fBranch(0)
    {
      if(policy && !policy->Include(name)) return;

      fData.emplace_back(); // needed to get an address
      T* target = (T*)&fData.front();
      fData.clear();

      const char code = rootcode<typename FlatType<T>::type>::code;

      if(totsize.empty()){ // this branch is not an array
        fBranch = tr->Branch(name.c_str(), target, (name+"/"+code).c_str());
      }
      else{ // needs to be an array - the size is given by 'totsize'
        fBranch = tr->Branch(name.c_str(), target, (name+"["+totsize+"]/"+code).c_str());
      }
    }

    void Clear()
    {
      fData.clear();
    }

    void Fill(const T& x)
    {
      const size_t oldcap = fData.capacity();

      fData.push_back(x);

      if(fBranch && fData.capacity() != oldcap){
        // The vector re-allocated, so we need to point the branch to the new
        // location.
        T* target = (T*)&fData.front();
        fBranch->SetAddress(target);
      }
    }

  protected:
    TBranch* fBranch;
    std::vector<typename FlatType<T>::type> fData;
  };

  template<class T> class Flat<std::vector<T>>
  {
  public:
    Flat(TTree* tr, const std::string& name, const std::string& totsize, const IBranchPolicy* policy) :
      fLength(tr, name+"..length", totsize, policy),
      fIdx(0),
      fTotArraySize(0),
      fData(tr, SubName(name), SubLengthName(tr, name, totsize), policy)
    {
      // Would always be zero if this vector was not nested inside any others
      if(!totsize.empty()){
        fIdx = new Flat<int>(tr, name+"..idx", totsize, policy);
      }
    }

    ~Flat()
    {
      delete fIdx;
    }

    void Clear()
    {
      fLength.Clear();
      if(fIdx) fIdx->Clear();
      fTotArraySize = 0;
      fData.Clear();
    }

    void Fill(const std::vector<T>& xs)
    {
      for(const T& x: xs) fData.Fill(x);
      fLength.Fill(xs.size());
      if(fIdx) fIdx->Fill(fTotArraySize);
      fTotArraySize += xs.size();
    }

  protected:
    std::string SubName(const std::string& name) const
    {
      // Nested containers would have the same name for length and idx at each
      // level, which is bad, so uniquify them.
      if(is_vec<T>::value || std::is_array_v<T>) return name+".elems";
      return name;
    }

    std::string SubLengthName(TTree* tr, const std::string& name, const std::string& totsize)
    {
      if(totsize.empty()) return name+"..length";

      const std::string ret = name+"..totarraysize";
      tr->Branch(ret.c_str(), &fTotArraySize, (ret+"/I").c_str());

      return ret;
    }

    Flat<int> fLength;
    Flat<int>* fIdx;

    int fTotArraySize;

    Flat<T> fData;
  };

  /// Implementation for flat arrays mirroring layout of flat vectors
  template<class T, int N> class FlatOutOfLineArray
  {
  public:
    FlatOutOfLineArray(TTree* tr, const std::string& name, const std::string& totsize, const IBranchPolicy* policy) :
      fIdx(0),
      fTotArraySize(0),
      fData(tr, SubName(name), SubLengthName(tr, name, totsize), policy)
    {
      // Would always be zero if this vector was not nested inside any others
      if(!totsize.empty()){
        fIdx = new Flat<int>(tr, name+"..idx", totsize, policy);
      }
    }

    ~FlatOutOfLineArray()
    {
      delete fIdx;
    }

    void Clear()
    {
      fTotArraySize = 0;
      if(fIdx) fIdx->Clear();
      fData.Clear();
    }

    void Fill(const T* xs)
    {
      for(int i = 0; i < N; ++i) fData.Fill(xs[i]);
      if(fIdx) fIdx->Fill(fTotArraySize);
      fTotArraySize += N;
    }

  protected:
    std::string SubName(const std::string& name) const
    {
      // Nested contains would have the same name for length and idx at each
      // level, which is bad, so uniquify them.
      if(is_vec<T>::value || std::is_array_v<T>) return name+".elems";
      return name;
    }

    std::string SubLengthName(TTree* tr, const std::string& name, const std::string& totsize)
    {
      if(totsize.empty()) return std::to_string(N);

      const std::string ret = name+"..totarraysize";
      tr->Branch(ret.c_str(), &fTotArraySize, (ret+"/I").c_str());

      return ret;
    }

    Flat<int>* fIdx;

    int fTotArraySize;

    Flat<T> fData;
  };

  /// \brief Implementation for "inline" flat arrays
  ///
  /// This means that foo[0].bar corresponds to a branch literally called
  /// foo.0.bar
  template<class T, int N> class FlatInlineArray
  {
  public:
    FlatInlineArray(TTree* tr, const std::string& name, const std::string& totsize, const IBranchPolicy* policy)
    {
      for(int i = 0; i < N; ++i){
        fData[i] = new Flat<T>(tr, name+"."+std::to_string(i), totsize, policy);
      }
    }

    ~FlatInlineArray()
    {
      for(Flat<T>* d: fData) delete d;
    }

    void Clear()
    {
      for(Flat<T>* d: fData) d->Clear();
    }

    void Fill(const T* xs)
    {
      for(unsigned int i = 0; i < N; ++i) fData[i]->Fill(xs[i]);
    }

  protected:
    Flat<T>* fData[N];
  };

  /// Critical size at which we switch from "inline" to vector-style arrays
  ///
  /// Inline arrays are easier to access in some contexts, and don't require
  /// cross-referencing indices, but can become unweildy with particularly
  /// large arrays. Arrays up to an including this size will be represented
  /// inline, while larger arrays will mimic the layout of vectors.
  static const int kMaxInlineSize = 16;

  /// Figure out the class arrays of a particular size should be implemented by
  template<class T, int N> using FlatArray = std::conditional_t<(N <= kMaxInlineSize), FlatInlineArray<T, N>, FlatOutOfLineArray<T, N>>;

  /// Implementation of flat arrays forwarding to inline or out-of-line variant
  template<class T, int N> class Flat<T[N]> : public FlatArray<T, N>
  {
    using FlatArray<T, N>::FlatArray; // inherit constructor
  };


  template<> class Flat<std::string>: public Flat<std::vector<char>>
  {
  public:
    Flat(TTree* tr, const std::string& name, const std::string& totsize, const IBranchPolicy* policy) : Flat<std::vector<char>>(tr, name, totsize, policy)
    {
    }

    void Fill(const std::string& x)
    {
      Flat<std::vector<char>>::Fill(std::vector<char>(x.c_str(), x.c_str()+x.size()+1)); // deliberately include the trailing null
    }
  };

}
