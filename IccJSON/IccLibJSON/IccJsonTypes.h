#ifndef _ICCJSONTYPES_H
#define _ICCJSONTYPES_H

#include <functional>
#include <nlohmann/json.hpp>

#ifndef ICC_JSON_ORDERED
#include <unordered_map>
#endif

namespace iccJson {

#ifdef ICC_JSON_ORDERED

using json = nlohmann::ordered_json;

#else

template <class Key, class T, class IgnoredLess, class Allocator>
struct unordered_map : public std::unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, Allocator>
{
  using base = std::unordered_map<Key, T, std::hash<Key>, std::equal_to<Key>, Allocator>;
  using key_compare = IgnoredLess;
  using base::base;
};

using json = nlohmann::basic_json<unordered_map>;

#endif

} // namespace iccJson

#endif
