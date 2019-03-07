#include "uri.hpp"

uri::uri(boost::filesystem::path path, std::string query, std::string fragment)
  : path(path) // path.remove_trailing_separator()
  , query(query)
  , fragment(fragment)
{
}
