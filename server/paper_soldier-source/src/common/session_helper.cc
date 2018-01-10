#include "session_helper.h"


namespace paper_soldier {

void SetId(const Ptr<Session> &session, const string id) {
  static const string kId = "id";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    Json &json = session->GetContext();
    json[kId] = id;
  }
}


void IsAuthorized(const Ptr<Session> &session, const bool is_signed_in) {
  static const string kSignIn = "sign_in";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    Json &json = session->GetContext();
    json[kSignIn] = is_signed_in;
  }
}


void SetCharacterIndice(const Ptr<Session> &session,
                        const IndexVector &indice) {
  static const string kIndice = "character_indice";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    Json &json = session->GetContext();
    json[kIndice].SetArray();

    for (const auto &i : indice) {
      json[kIndice].PushBack(i);
    }
  }
}


void SetState(const Ptr<Session> &session,
              const SessionState &state) {
  static const string kState = "state";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    Json &json = session->GetContext();
    json[kState] = static_cast<int64_t>(state);
  }
}


const string GetId(const Ptr<Session> &session) {
  static const string kId = "id";
  static const string kEmptyString("");

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    const Json &json = session->GetContext();
    if (not json.HasAttribute(kId, Json::kString)) {
      return kEmptyString;
    }

    return json[kId].GetString();
  }
}


const bool IsAuthorized(const Ptr<Session> &session) {
  static const string kSignIn = "sign_in";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    const Json &json = session->GetContext();
    if (not json.HasAttribute(kSignIn, Json::kBoolean)) {
      return false;
    }
    return json[kSignIn].GetBool();
  }
}


const bool HasCharacter(const Ptr<Session> &session, const size_t index) {
  static const string kIndice = "character_indice";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    const Json &json = session->GetContext();
    if (not json.HasAttribute(kIndice, Json::kArray)) {
      return false;
    }
    size_t size = json[kIndice].Size();
    int64_t index2 = static_cast<int64_t>(index);
    for (size_t i = 0; i < size; ++i) {
      if (json[kIndice][i].GetInteger() == index2) {
        return true;
      }
    }

    return false;
  }
}


const SessionState GetState(const Ptr<Session> &session) {
  static const string kState = "state";

  LOG_ASSERT(session);

  {
    boost::mutex::scoped_lock lock(session->GetContextMutex());
    const Json &json = session->GetContext();
    if (not json.HasAttribute(kState, Json::kString)) {
      return kNoneState;
    }

    return static_cast<SessionState>(json[kState].GetInteger());
  }
}

}  // namespace paper_soldier