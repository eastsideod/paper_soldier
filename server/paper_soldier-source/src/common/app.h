#ifndef SRC_COMMON_APP_H_
#define SRC_COMMON_APP_H_

#include "../paper_soldier_types.h"


namespace paper_soldier {

class App {

 public:
  static bool Initialize();
  static bool Update();
  static bool IsAllowedCharacterIndex(const size_t index);
  static const Json& GetCharacterInfo(const size_t index);
};

}  // namespace paper_soldier

#endif  // SRC_COMMON_APP_H_
