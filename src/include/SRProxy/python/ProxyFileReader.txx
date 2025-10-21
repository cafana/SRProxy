#include "SRProxy/BasicTypesProxy.h"

#include "TChain.h"
#include "TInterpreter.h"

#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

#include <memory>

namespace py = pybind11;

template <typename T> class ProxyFileReader {

  std::unique_ptr<TChain> sr_chain;
  std::unique_ptr<caf::Proxy<T>> srp;

  size_t nentries;
  size_t ientry;

public:
  ProxyFileReader(std::string const &chname,
                  std::vector<std::string> const &infiles) {

    gInterpreter->SetClassAutoloading(false);
    gInterpreter->SetClassAutoparsing(false);

    sr_chain = std::make_unique<TChain>(chname.c_str());

    for (auto const &f : infiles) {
      sr_chain->Add(f.c_str());
    }
    nentries = sr_chain->GetEntries();
    ientry = 0;

    srp = std::make_unique<caf::Proxy<T>>(sr_chain.get(), chname);
  }

  pybind11::object first() {
    ientry = 0;
    return next();
  }

  pybind11::object next() {
    if (ientry < nentries) {
      sr_chain->GetEntry(ientry++);
      return py::cast(srp.get());
    } else {
      return py::none();
    }
  }

  size_t entries() const { return nentries; }
  pybind11::object entry(size_t i) {
    ientry = i;
    return next();
  }
};

class ProxyFileReader_sentinel {};

template <typename T> class ProxyFileReader_iter {
  std::reference_wrapper<ProxyFileReader<T>> pfr;
  pybind11::object curr_event;

public:
  ProxyFileReader_iter(ProxyFileReader<T> &_pfr) : pfr(_pfr) {
    curr_event = pfr.get().first();
  }
  void operator++() { curr_event = pfr.get().next(); }
  pybind11::object const &operator*() { return curr_event; }
  bool operator!=(ProxyFileReader_sentinel const &) const {
    return !curr_event.is(py::none());
  }
  bool operator==(ProxyFileReader_sentinel const &) const {
    return curr_event.is(py::none());
  }
};

template <typename T> ProxyFileReader_iter<T> begin(ProxyFileReader<T> &pfr) {
  return ProxyFileReader_iter(pfr);
}

template <typename T> ProxyFileReader_sentinel end(ProxyFileReader<T> &) {
  return ProxyFileReader_sentinel();
}
