// rmi_none.cpp — an empty RMI registry, for builds without generated RMIs.
// With generated RMIs, data/rmi/registry.cpp is linked instead.
#include "rmi_index.hpp"

namespace ft {
const RMIEntry* rmi_registry(std::size_t& count) { count = 0; return nullptr; }
const char* rmi_data_path() { return ""; }
} // namespace ft
