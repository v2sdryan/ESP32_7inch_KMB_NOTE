#include "KmbCatalogModel.h"

#include <cctype>
#include <limits>

namespace KmbCatalogModel
{
int parseSequence(const char *value)
{
  if (!value || !value[0]) return 0;

  int result = 0;
  for (const unsigned char *cursor =
           reinterpret_cast<const unsigned char *>(value);
       *cursor; ++cursor)
  {
    if (std::isdigit(*cursor) == 0) return 0;
    const int digit = *cursor - '0';
    if (result > (std::numeric_limits<int>::max() - digit) / 10) return 0;
    result = result * 10 + digit;
  }
  return result > 0 ? result : 0;
}
}
