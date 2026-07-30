#include "SRProxy/BasicTypesProxy.txx"

namespace caf {

// Enumerate all the variants we expect
template class Proxy<char>;
template class Proxy<short>;
template class Proxy<int>;
template class Proxy<long>;
template class Proxy<long long>;

template class Proxy<unsigned char>;
template class Proxy<unsigned short>;
template class Proxy<unsigned int>;
template class Proxy<unsigned long>;
template class Proxy<unsigned long long>;

template class Proxy<float>;
template class Proxy<double>;
template class Proxy<long double>;

template class Proxy<bool>;

template class Proxy<std::string>;

} // namespace caf
