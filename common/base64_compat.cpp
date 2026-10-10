#include "base64.h"

#include <dynamips/macros.h>

#include <limits>
#include <span>
#include <string_view>

import base64;

cfn base64_encode(unsigned char *out, const unsigned char *in, int inlen)
    -> void {
  if (inlen < 0)
    return;

  const let size = static_cast<std::size_t>(inlen);
  const let capacity = base64::encoded_size(size);
  if (!capacity)
    return;

  const let encoded = base64::encode(
      std::span{reinterpret_cast<char *>(out), *capacity}, std::span{in, size});
  if (encoded)
    out[*encoded] = '\0';
}

cfn base64_decode(unsigned char *out, const unsigned char *in, int maxlen)
    -> int {
  if (maxlen < 0)
    return -1;

  let text = std::string_view{reinterpret_cast<const char *>(in)};
  if (text.starts_with("+ "))
    text.remove_prefix(2);
  text = text.substr(0, text.find('\r'));

  const let capacity =
      maxlen == 0 ? (text.size() / 4) * 3 : static_cast<std::size_t>(maxlen);
  const let decoded = base64::decode(std::span{out, capacity}, text);
  if (!decoded ||
      *decoded > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return -1;

  return static_cast<int>(*decoded);
}
