#include "app.h"

#include <boost/thread/mutex.hpp>


namespace paper_soldier {

namespace {

bool initialized = false;
boost::mutex the_mutex;
Ptr<const Json> the_character_data;


bool ReadAndSetConfigs() {
  // 락을 잡고 호출 한다.

  // TODO(inkeun): management tool 개발 및 외부 API 호출로 변경
  Ptr<const Json> character_data = ResourceManager::GetJsonData(
      "character.json");

  if (not character_data) {
    LOG(ERROR) << "Failed to load character information.";
    return false;
  }
  return true;
}

}  // unnamed namespace


bool App::Initialize() {
  LOG_ASSERT(not initialized);

  bool result = false;

  {
    boost::mutex::scoped_lock lock(the_mutex);
    result = ReadAndSetConfigs();
    initialized = result;
  }

  return result;
}


bool App::Update() {
  LOG_ASSERT(initialized);

  boost::mutex::scoped_lock lock(the_mutex);
  return ReadAndSetConfigs();
}


bool App::IsAllowedCharacterIndex(const size_t index) {
  boost::mutex::scoped_lock lock(the_mutex);
  return index < (*the_character_data).Size();
}


const Json &App::GetCharacterInfo(const size_t index) {
  const static Json kEmptyJson;

  LOG_ASSERT(the_character_data);

  boost::mutex::scoped_lock lock(the_mutex);
  const Json element = (*the_character_data)[index];

  if (not element.IsNull()) {
    return element;
  } else {
    return kEmptyJson;
  }
}

}  // namespace paper_soldier