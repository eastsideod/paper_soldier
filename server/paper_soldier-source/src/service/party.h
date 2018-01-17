#ifndef SRC_SERVICE_PARTY_H_
#define SRC_SERVICE_PARTY_H_

#include "../paper_soldier_types.h"
#include "paper_soldier_rpc_messages.pb.h"
#include "paper_soldier_messages.pb.h"
#include "proto/party_messages.pb.h"


namespace paper_soldier {

class Party {
 public:
  typedef Uuid Id;
  typedef function<void(const Ptr<Party> &/*party*/,
                        const bool/*error*/)> CreatedCallback;
  typedef function<void(const Id &/*party_id*/,
                        const bool /*error*/)> ClosedCallback;
  typedef function<void(const Id &/*party_id*/,
                        const Rpc::Rpc::PeerId &/*peer_id*/,
                        const string &/*account_id*/)> EnteredCallback;
  typedef function<void(const Party::Id &/*party_id*/,
                        const string &/*account_id*/,
                        bool /*result*/)> RecommendedCallback;
  typedef function<void(const Party::Id &/*party_id*/,
                        const string &/*account_id*/,
                        bool /*result*/)> ParticipatedCallback;
  typedef function<void(const Id &/*party_id*/,
                        const Rpc::Rpc::PeerId &/*peer_id*/,
                        const string &/*account_id*/)> LeftCallback;
  typedef function<void(const Id &/*party_id*/,
                        const Rpc::Rpc::PeerId &/*peer_id*/,
                        const bool /*error*/)> LocatedCallback;

  static void Create(const CreatedCallback &cb);
  static const Ptr<Party> &Find(const Id &id);
  static void Locate(const Id &id, const LocatedCallback &cb);

  static void RegisterRecommendedCallback(const RecommendedCallback &cb);
  static void RegisterParticipatedCallback(const ParticipatedCallback &cb);
  static void RegisterEnteredCallback(const EnteredCallback &cb);
  static void RegisterLeftCallback(const LeftCallback &cb);
  static void RegisterClosedCallback(const ClosedCallback &cb);

  // 파티원 -> 파티장(초대요청), 파티장 -> 유저(파티 참가 요청)
  void Recommend(const string &recommender_id,
                 const string &account_id) ASSERT_NO_ROLLBACK;

  // 파티장 -> 유저(파티 참가 요청)
  void AskParticipation(const string &account_id) ASSERT_NO_ROLLBACK;

  bool Enter(const string &account_id,
             const Ptr<Session> &session) ASSERT_NO_ROLLBACK;
  bool Enter(const string &account_id,
             const Rpc::Rpc::PeerId &peer_id) ASSERT_NO_ROLLBACK;

  bool Leave(const string &account_id) ASSERT_NO_ROLLBACK;
  void Close() ASSERT_NO_ROLLBACK;

  void SetOwner(const string &account_id);
  void ChangeOwner(const string &account_id);
  bool Has(const string &account_id);

  void SendMessage(const string &account_id, const string &message_type,
                   const Ptr<FunMessage> &message, const Encryption encryption,
                   const TransportProtocol protocol) ASSERT_NO_ROLLBACK;
  void Broadcast(const string &message_type, const Ptr<FunMessage> &message,
                 const Encryption encryption,
                 const TransportProtocol protocol) ASSERT_NO_ROLLBACK;

  const string &GetOwnerId() const;
  const Id &id() const;

 private:
  Party(const Id &id);

  class PartyImpl;
  PartyImpl *impl_;
};

}  // namespace paper_soldier

#endif  // SRC_SERVICE_PARTY_H_