#ifndef SRC_COMMON_SESSION_HELPER_H_
#define SRC_COMMON_SESSION_HELPER_H_

#include "../paper_soldier_types.h"


namespace paper_soldier {

typedef std::vector<int64_t> IndexVector;

enum SessionState {
  kNoneState,
  kInLobby,
  kInPurchasing,
  kStartingMatchMaking,
  kInMatch
};

void SetId(const Ptr<Session> &session, const string &id);
void IsAuthorized(const Ptr<Session> &session, const bool is_signed_in);
void SetCharacterIndice(const Ptr<Session> &session,
                        const IndexVector &indice);
void SetState(const Ptr<Session> &session, const SessionState &state);

const string GetId(const Ptr<Session> &session);
const bool IsAuthorized(const Ptr<Session> &session);
const bool HasCharacter(const Ptr<Session> &session, const size_t index);
const SessionState GetState(const Ptr<Session> &session);

}  // namespace paper_soldier

#endif  // SRC_COMMON_SESSION_HELPER_H_
