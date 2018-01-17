#include "party.h"

#include <boost/enable_shared_from_this.hpp>
#include <boost/foreach.hpp>
#include <boost/thread/mutex.hpp>
#include <map>
#include <vector>

#include "paper_soldier_rpc_messages.pb.h"


namespace paper_soldier {

namespace {

typedef Party::Id Id;
typedef Party::CreatedCallback CreatedCallback;
typedef Party::ClosedCallback ClosedCallback;
typedef Party::RecommendedCallback RecommendedCallback;
typedef Party::ParticipatedCallback ParticipatedCallback;
typedef Party::LocatedCallback LocatedCallback;
typedef Party::EnteredCallback EnteredCallback;
typedef Party::LeftCallback LeftCallback;

typedef Rpc::PeerId PeerId;
typedef Rpc::Xid Xid;
typedef std::map<const Party::Id, Ptr<Party> > PartyMap;


// account id, peer id 를 캐싱해서 사용한다.
// 일정 시간 이상은 유저가 캐싱된 서버에 머무른다고
// 가정한다.
class RemoteUserCache {
 public:
  typedef AccountManager::LocateCallback UserLocatedCallback;

  const boost::posix_time::time_duration kCacheExpiredTime =
      boost::posix_time::seconds(300);

  void SetUser(const string &account_id, const PeerId &peer_id);
  void EraseUser(const string &account_id);
  void Find(const string &account_id,
            const UserLocatedCallback &cb);

  void OnLocatedUser(const string &account_id,
                     const PeerId &peer_id,
                     const UserLocatedCallback &cb);

  void OnCacheExpired(const Timer::Id &id, const WallClock::Value &at,
                      const string &account_id, const PeerId &peer_id);

  void CancelNotExpiredTimers();

 private:
  // 2개 이상의 쓰레드에서 접근이 가능하다.
  // 하지만 반드시 락으로 보호할 필요가 반드시 있진 않을 듯 하다.
  // 우선 volatile 로 처리하고 추후에 필요한지 다시 고민한다.
  struct PeerCache {
    PeerId peer_id;
    volatile bool needed_to_expire;
  };

  typedef std::map<const string /*account_id*/,
                   PeerCache /*server*/> RemoteUserMap;
  typedef std::list<Timer::Id> TimerIdList;

  boost::mutex mutex_;
  RemoteUserMap map_;

  // timer는 시순으로 쌓이게 될 것이다.
  // linear search가 유리하다.
  TimerIdList not_expired_timer_ids_;
};


const string kPartyRpcMessageType("_party");
const string kPartyMessageType("_party");
const Ptr<Party> kPartyNullPtr;


bool installed = false;
boost::mutex the_party_mutex;
PartyMap the_party_map;

bool use_remote_user_cache = true;

// 최초 install 이후에 변경하지 않는다.
// 추후 변경이 필요하다면 mutex로 감싸야한다.
string the_self_peer_id_str;
RemoteUserCache the_remote_user_cache;


// callbacks
// [
boost::mutex the_callback_mutex;
ClosedCallback the_closed_callback;
RecommendedCallback the_recommended_callback;
ParticipatedCallback the_participated_callback;
LocatedCallback the_located_callback;
EnteredCallback the_entered_callback;
LeftCallback the_left_callback;
// ]


// redis
// [
Ptr<RedisClient> the_redis_client;
string the_redis_server_ip = "127.0.0.1";
size_t the_redis_server_port = 6379;
string the_redis_auth_pass = "";
size_t the_redis_connection_count = 4;
// TODO(inkeun): 추후에 변경가능한 상태로 만든다(mutex 처리 추가)
uint64_t the_party_expire_duration_in_seconds = 300;
// ]


// messages
// [
void SetRecommendationMessage(const Party::Id &party_id,
                              const string &recommender_id,
                              const string &account_id,
                              const string &owner_id,
                              PartyMessage *out);
void SetAskParticipationMessage(const Party::Id &party_id,
                                const string &account_id,
                                PartyMessage *out);
void SetEnteredMessage(const Party::Id &party_id, const string &account_id,
                       const string &receiver_id, PartyMessage *out);
void SetLeftMessage(const Party::Id &party_id, const string &account_id,
                    PartyMessage *out);
// ]


// internal functions
// [
const string CreateRedisKey(const Party::Id &id);
void OnLocatedParty(const RedisClient::Result &result, const string &value,
                    const Party::Id &id, const LocatedCallback &cb);
void OnPartySubmittedInRedis(const RedisClient::Result &result, int64_t value,
                             const string &redis_key, const Ptr<Party> &party,
                             const CreatedCallback &cb);
void OnPartySubmittedInRedis2(const RedisClient::Result &result, int64_t value,
                              const Ptr<Party> &party,
                              const CreatedCallback &cb);
// ]


// rpc message handlers
// [
bool ValidateRpcMessage(const PartyMessage &message);
void OnPartyRpcMessageReceived(const PeerId &sender, const Xid &xid,
                               const Ptr<const FunRpcMessage> &request);
void OnRecommended(const Party::Id &id, const string &recommender_id,
                   const string &owner_id, const string &account_id);
void OnRecommendedResponsed(const Party::Id &party_id,
                            const string &recommender_id,
                            const string &owner_id,
                            const string &account_id,
                            const bool result);
void OnAskedParticipation(const Party::Id &party_id, const string &account_id);
void OnEntered(const Party::Id &party_id, const string account_id,
               const string &receiver_id);
void OnLeft(const Party::Id &party_id, const string &account_id,
            const string &receiver_id);
// ]


void RemoteUserCache::SetUser(const string &account_id, const PeerId &peer_id) {
  typedef std::pair<RemoteUserMap::iterator, bool> InsertResult;

  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(not peer_id.is_nil());

  PeerCache cache;
  cache.peer_id = peer_id;
  cache.needed_to_expire = true;

  {
    boost::mutex::scoped_lock lock(mutex_);
    InsertResult result = map_.insert(std::make_pair(account_id, cache));

    // 데이터 입력에 실패하는 경우는
    // 해당 유저가 존재하는 서버가 변경된 경우 밖에 없다.
    if (not result.second) {
      result.first->second.peer_id = peer_id;
      result.first->second.needed_to_expire = false;
    }
  }

  // TODO(inkeun): 현재는 유저의 위치가 바뀌면 타이머를 한 번 만료시키고
  // 그 뒤에 도는 타이머로 처리하도록 하였다.
  // 타이머를 여기서 직접 취소하고 다시 타이머를 돌려야 할지 고민해본다
  // (현재는 시순으로 쌓이게 되어있어 이 부분의 변경이 필요하다)
  const Timer::Id timer_id = Timer::ExpireAfter(
      RemoteUserCache::kCacheExpiredTime,
      bind(&RemoteUserCache::OnCacheExpired, this,
           _1, _2, account_id, peer_id));

  LOG_ASSERT(timer_id != Timer::kInvalidTimerId);

  {
    boost::mutex::scoped_lock lock(mutex_);
    not_expired_timer_ids_.push_back(timer_id);
  }
}


// 가급적 타이머가 만료되어 캐시에서 내려가겠지만,
// 강제로 내려야하는 경우에 호출한다.
void RemoteUserCache::EraseUser(const string &account_id) {
  LOG_ASSERT(not account_id.empty());

  boost::mutex::scoped_lock lock(mutex_);
  // 성공하던, 실패하던 상관없다.
  map_.erase(account_id);
}


void RemoteUserCache::Find(const string &account_id,
                           const UserLocatedCallback &cb) {
  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(cb);

  RemoteUserMap::iterator itr;

  {
    boost::mutex::scoped_lock lock(mutex_);
    itr = map_.find(account_id);
  }

  if (itr == map_.end()) {
    AccountManager::LocateAsync(
        account_id, bind(&RemoteUserCache::OnLocatedUser, this, _1, _2, cb));
    return;
  }

  // NOTE(inkeun): cache에 들어있는 정보가 현재 유저가 접속한 서버와
  // 반드시 일치한다는 보장을 하지 않는다(확률적 성공)
  // 이 경우 해당 서버로 rpc 를 보낸 뒤에 해당 서버에서 처리하도록 유도한다.
  // 추가로, Find에 성공했다면 해당 캐시의 유효기간을 늘려준다.
  //
  // TODO(inkeun): 해당 캐시의 유효기간이 늘어난다고해서,
  // 데이터가 올바르다는 뜻은 아니다. 해당 캐시가 실제로 성공했는 지
  // 외부 서버에서 응답을 받는 것을 고려한다.
  itr->second.needed_to_expire = false;
  cb(account_id, itr->second.peer_id);
}


void RemoteUserCache::OnLocatedUser(const string &account_id,
                                    const PeerId &peer_id,
                                    const UserLocatedCallback &cb) {
  // 실패하면 캐시에 올리지 않는다.
  if (account_id.empty() || peer_id.is_nil()) {
    cb(account_id, peer_id);
    return;
  }

  RemoteUserCache::SetUser(account_id, peer_id);
  cb(account_id, peer_id);
}


void RemoteUserCache::OnCacheExpired(const Timer::Id &id,
                                     const WallClock::Value &at,
                                     const string &account_id,
                                     const PeerId &peer_id) {
  LOG_ASSERT(not account_id.empty());

  {
    boost::mutex::scoped_lock lock(mutex_);

    RemoteUserMap::iterator itr = map_.find(account_id);
    // 이미 캐시에서 내려간 유저
    // 다른 처리를 할 필요가 없다.
    if (itr == map_.end()) {
      return;
    }

    // 새로운 peer_id를 발급받았다.
    // 다른 처리하지 않고 다음 타이머가 돌도록 기다린다.
    if (itr->second.peer_id != peer_id) {
      return;
    }

    // 만료 기간이 연장되었다(자주 캐시를 호출하는 경우이다)
    // 이 경우 다시 타이머를 등록한다.
    if (not itr->second.needed_to_expire) {
      itr->second.needed_to_expire = true;
      Timer::Id timer_id = Timer::ExpireAfter(
          RemoteUserCache::kCacheExpiredTime,
          bind(&RemoteUserCache::OnCacheExpired, this, _1, _2,
               account_id, peer_id));

      LOG_ASSERT(timer_id != Timer::kInvalidTimerId);
      not_expired_timer_ids_.push_back(timer_id);
      return;
    }

    map_.erase(itr);

    // 시순으로 호출될 것이다.
    // linear search를 할 것이므로 앞 순서대로 제거할 것을 기대한다.
    not_expired_timer_ids_.remove(id);
  }
}


// 만료되지 않은 타이머를 모두 종료한다.
void RemoteUserCache::CancelNotExpiredTimers() {
  boost::mutex::scoped_lock lock(mutex_);
  BOOST_FOREACH(const Timer::Id &id, not_expired_timer_ids_) {
    LOG_ASSERT(id != Timer::kInvalidTimerId);
    Timer::Cancel(id);
  }
}


void SetRecommendationMessage(
    const Party::Id &party_id, const string &recommender_id,
    const string &account_id, const string &owner_id,
    PartyMessage *out) {
  LOG_ASSERT(out);

  out->set_type(PartyMessage_Type_kRecommendationRequest);
  out->set_party_id(boost::lexical_cast<string>(party_id));
  out->set_recommender_id(recommender_id);
  out->set_account_id(account_id);
  out->set_owner_id(owner_id);
}


void SetAskParticipationMessage(const Party::Id &party_id,
                                const string &account_id,
                                PartyMessage *out) {
  LOG_ASSERT(out);

  out->set_type(PartyMessage_Type_kAskParticipation);
  out->set_party_id(boost::lexical_cast<string>(party_id));
  out->set_account_id(account_id);
}


void SetEnteredMessage(const Party::Id &party_id, const string &account_id,
                       const string &receiver_id, PartyMessage *out) {
  LOG_ASSERT(out);

  out->set_type(PartyMessage_Type_kEntered);
  out->set_party_id(boost::lexical_cast<string>(party_id));
  out->set_account_id(account_id);
  out->set_receiver_id(receiver_id);
}


void SetLeftMessage(const Party::Id &party_id, const string &account_id,
                    const string &receiver_id, PartyMessage *out) {
  LOG_ASSERT(out);

  out->set_type(PartyMessage_Type_kLeft);
  out->set_party_id(boost::lexical_cast<string>(party_id));
  out->set_account_id(account_id);
  out->set_receiver_id(receiver_id);
}


const string CreateRedisKey(const Party::Id &id) {
  static const string kPartyPrefixKeyword("paper_soldier/party/");
  LOG_ASSERT(not id.is_nil());

  std::stringstream ss;
  ss << kPartyPrefixKeyword << boost::lexical_cast<string>(id);
  return ss.str();
}


void OnLocatedParty(const RedisClient::Result &result, const string &value,
                    const Party::Id &id, const LocatedCallback &cb) {
  LOG_ASSERT(cb);
  if (value.empty()) {
    cb(id, Uuid(), false);
  }

  PeerId peer_id = boost::lexical_cast<Uuid>(value);
  cb(id, peer_id, true);
}


void OnPartySubmittedInRedis2(
    const RedisClient::Result &result, int64_t value,
    const Ptr<Party> &party, const CreatedCallback &cb) {
  LOG_ASSERT(party);
  LOG_ASSERT(cb);

   LOG(INFO) << "redis expire result=" << value;

  // setnx->expire 설정까지 모두 완료되었다.
  if (result.type == RedisClient::Result::kSuccess) {
    cb(party, false);
    return;
  }

  // TODO(inkeun): remote redis_key if exists

  cb(nullptr, false);
}


void OnPartySubmittedInRedis(const RedisClient::Result &result, int64_t value,
                             const string &redis_key, const Ptr<Party> &party,
                             const CreatedCallback &cb) {
  LOG(INFO) << "redis set nx result=" << value;

  // 중복된 파티 아이디이다. 가능한지 확인해봐야 할 것이다.
  if (result.type != RedisClient::Result::kSuccess) {
    LOG(ERROR) << "Failed to submit party id in redis. id="
               << boost::lexical_cast<string> (party->id())
               << ", redis_key=" << redis_key;
    cb(nullptr, false);
    return;
  }

  the_redis_client->Expire(
      redis_key, the_party_expire_duration_in_seconds,
      bind(&OnPartySubmittedInRedis2, _1, _2, party, cb));
}


bool ValidateRpcMessage(const PartyMessage &message) {
  switch (message.type()) {
    case PartyMessage_Type_kRecommendationRequest:
    {
      // 별다른 처리를 하지 않는다.
      if (not message.has_recommender_id() || not message.has_owner_id() ||
          not message.has_account_id()) {
        return false;
      }
      return true;
    }

    case PartyMessage_Type_kRecommendationResponse:
    {
      // 별다른 처리를 하지 않는다.
      if (not message.has_party_id() || not message.has_recommender_id() ||
          not message.has_owner_id() || not message.has_account_id()) {
        return false;
      }
      return true;
    }

    default:
      return false;
  }

  return false;
}


void OnPartyRpcMessageReceived(const PeerId &sender, const Xid &xid,
                               const Ptr<const FunRpcMessage> &request) {
  if (not request->HasExtension(_rpc_party_message)) {
    LOG(ERROR) << __func__ << " Invalid rpc message. skipping.";
    return;
  }

  const PartyMessage &message = request->GetExtension(_rpc_party_message);

  if (not ValidateRpcMessage(message)) {
    LOG(ERROR) << __func__ << " Invalid rpc message."
                << " type=" << message.type();
    return;
  }

  Party::Id party_id;

  try {
    party_id = boost::lexical_cast<Uuid>(message.party_id());
  // TODO(inkeun): change bad cast
  } catch(...) {

  }

  switch (message.type()) {
    case PartyMessage_Type_kRecommendationRequest:
    {
      OnRecommended(party_id, message.recommender_id(),
                    message.owner_id(), message.account_id());
      return;
    }

    case PartyMessage_Type_kRecommendationResponse:
    {
      OnRecommendedResponsed(party_id, message.recommender_id(),
                             message.owner_id(), message.account_id(),
                             message.result());
      return;
    }
    case PartyMessage_Type_kAskParticipation:
    {
      OnAskedParticipation(party_id, message.account_id());
      return;
    }

    case PartyMessage_Type_kEntered:
    {
      OnEntered(party_id, message.account_id(), message.receiver_id());
      return;
    }

    case PartyMessage_Type_kLeft:
    {
      OnLeft(party_id, message.account_id(), message.receiver_id());
      return;
    }

    default:
      LOG_ASSERT(false);
  }
}


// 파티원으로 부터 파티 초대 요청
void OnRecommended(const Party::Id &id, const string &recommender_id,
                   const string &owner_id, const string &account_id) {
  const Ptr<Session> owner = AccountManager::FindLocalSession(owner_id);

  // 다른 서버에서 포워딩되어 날아온 메시지인데,
  // 세션이 존재하지 않는다. 이 경우는 다른 서버로 옮겨갔는지
  // 확인해보고 없다면 별다른 처리를 하지 않는다
  // (클라이언트 측 파티 초대 요청에 대한 타임아웃으로 처리되길 기대한다)
  if (not owner) {
    // the_remote_user_cache.Find(
    //     owner_id, bind(&ForwardRecommendedMessage, _1, _2, _3));
    return;
  }

  Ptr<FunMessage> message(new FunMessage());
  SetRecommendationMessage(
      id, recommender_id, account_id, owner_id,
      message->MutableExtension(_party_message));
  owner->SendMessage(kPartyMessageType, message);
}


// 파티장으로 부터 파티 초대 요청에 대한 응답
void OnRecommendedResponsed(
    const Party::Id &party_id, const string &recommender_id,
    const string &owner_id, const string &account_id,
    const bool result) {
  const Ptr<Session> session = AccountManager::FindLocalSession(recommender_id);

  // 로컬에 존재하지 않는 파티원
  if (not session) {
    // the_remote_user_cache.Find(
    //     recommender_id, bind(&SendRecommendResponsed, _1, _2, result));
  }

  // 파티 초대 요청을 거절 했다면 달리 처리할 것이 없다.
  // (추후 응답을 돌려준지 고민한다)
  if (not result) {
    return;
  }

  // OnAskedParticipation();
}

void OnAskedParticipation(const Party::Id &party_id, const string &account_id) {
  const Ptr<Session> account = AccountManager::FindLocalSession(account_id);

  // 파티 초대를 요청할 세션이 로컬에 없다.
  if (not account) {
    // TODO(inkeun): impl this
    return;
  }

  Ptr<FunMessage> msg(new FunMessage());
  SetAskParticipationMessage(
      party_id, account_id, msg->MutableExtension(_party_message));
  account->SendMessage(_party_message, msg, kDefaultEncryption, kTcp);
}


void OnEntered(const Party::Id &party_id, const string account_id,
               const string &receiver_id) {
  const Ptr<Session> receiver = AccountManager::FindLocalSession(receiver_id);

  // 파티 초대를 요청할 세션이 로컬에 없다.
  if (not receiver) {
    // TODO(inkeun): impl this
    return;
  }

  Ptr<FunMessage> msg(new FunMessage());
  SetEnteredMessage(
      party_id, account_id, receiver_id, msg->MutableExtension(_party_message));
  receiver->SendMessage(_party_message, msg, kDefaultEncryption, kTcp);
}


void OnLeft(const Party::Id &party_id, const string &account_id,
            const string &receiver_id) {
  const Ptr<Session> receiver = AccountManager::FindLocalSession(receiver_id);

  // 파티 초대를 요청할 세션이 로컬에 없다.
  if (not receiver) {
    // TODO(inkeun): impl this
    return;
  }

  Ptr<FunMessage> msg(new FunMessage());
  SetLeftMessage(party_id, account_id, receiver_id,
                 msg->MutableExtension(_party_message));
  receiver->SendMessage(_party_message, msg, kDefaultEncryption, kTcp);
}

}  // unnnamed namespace


class Party::PartyImpl {
 public:
  struct User {
    string account_id;
    bool is_self;
    // TODO(inkeun): boost::variant로 바꿀지 고민해본다.
    PeerId peer_id;
    Ptr<Session> session;
  };

  PartyImpl(const Party::Id &id);

  void OnLocatedRecommendedUser(const string &account_id,
                                const PeerId &peer_id,
                                const string &recommender_id);
  void OnLocatedParticipationUser(const string &account_id,
                                  const PeerId &peer_id);

  // 파티원 -> 파티장(초대요청), 파티장 -> 유저(파티 참가 요청)
  void Recommend(const string &recommender_id,
                 const string &account_id);

  // 파티장 -> 유저(파티 참가 요청)
  void AskParticipation(const string &account_id);

  bool Enter(const string &account_id, const Ptr<Session> &session);
  bool Enter(const string &account_id, const PeerId &peer_id);
  bool Leave(const string &account_id);
  void Close();

  void SetOwner(const string &account_id);
  void ChangeOwner(const string &account_id);
  bool Has(const string &account_id);

  void SendMessage(const string &id, const string &message_type,
                   const Ptr<FunMessage> &message,
                   const Encryption encryption,
                   const TransportProtocol protocol);
  void Broadcast(const string &message_type,
                 const Ptr<FunMessage> &message,
                 const Encryption encryption,
                 const TransportProtocol protocol);

  const string &GetOwnerId() const;
  const Id &id() const;

 private:
  typedef std::map<const string, Ptr<User> > AccountMap;

  Party::Id id_;
  mutable boost::mutex mutex_;
  AccountMap users_;
  string owner_id_;
};


Party::PartyImpl::PartyImpl(const Party::Id &id) : id_(id) {
}


void Party::PartyImpl::SetOwner(const string &account_id) {
  boost::mutex::scoped_lock lock(mutex_);
  owner_id_ = account_id;
}


void Party::PartyImpl::ChangeOwner(const string &account_id) {
  boost::mutex::scoped_lock lock(mutex_);
  owner_id_ = account_id;
}


void Party::PartyImpl::Close() {
  {
    boost::mutex::scoped_lock lock(mutex_);
    // TODO(inkeun): send message;
  }

  {
    boost::mutex::scoped_lock lock(the_party_mutex);
    bool result = the_party_map.erase(id_);
    LOG_ASSERT(result);
  }
}


bool Party::PartyImpl::Has(const string &account_id) {
  boost::mutex::scoped_lock lock(mutex_);
  AccountMap::const_iterator itr = users_.find(account_id);
  return itr != users_.end();
}


// 방장이 다른 서버에 있다.
void Party::PartyImpl::OnLocatedRecommendedUser(
    const string &account_id, const PeerId &peer_id,
    const string &recommender_id) {

  // 유저를 찾지 못했다.
  // 로그아웃 했다.
  if (account_id.empty() || peer_id.is_nil()) {
    the_recommended_callback(id_, account_id, false);
    return;
  }

  Ptr<FunRpcMessage> message (new FunRpcMessage());
  SetRecommendationMessage(
      id_, recommender_id, account_id, owner_id_,
      message->MutableExtension(_rpc_party_message));
  Rpc::Call(peer_id, message);
}


void Party::PartyImpl::OnLocatedParticipationUser(
    const string &account_id, const PeerId &peer_id) {
  // 찾지 못했다.
  if (account_id.empty() || peer_id.is_nil()) {
    the_participated_callback(id_, account_id, false);
    return;
  }

  Ptr<FunRpcMessage> message (new FunRpcMessage());
  SetAskParticipationMessage(
      id_, account_id,
      message->MutableExtension(_rpc_party_message));
  Rpc::Call(peer_id, message);
}


void Party::PartyImpl::Recommend(const string &recommender_id,
                                 const string &account_id) {
  AssertNoRollback();

  const Ptr<Session> owner = AccountManager::FindLocalSession(owner_id_);

  // 방장이 다른 서버에 있다.
  if (not owner) {
    the_remote_user_cache.Find(
        account_id, bind(&Party::PartyImpl::OnLocatedRecommendedUser, this,
                         _1, _2, recommender_id));
    return;
  }

  Ptr<FunMessage> msg (new FunMessage());
  SetRecommendationMessage(id_, recommender_id, account_id,
                           owner_id_, msg->MutableExtension(_party_message));
  owner->SendMessage(_party_message, msg, kDefaultEncryption, kTcp);
}


// 파티장 -> 유저(파티 참가 요청)
void Party::PartyImpl::AskParticipation(const string &account_id) {
  AssertNoRollback();

  const Ptr<Session> account = AccountManager::FindLocalSession(account_id);

  // 유저가 다른 서버에 있다.
  if (not account) {
    the_remote_user_cache.Find(
        account_id, bind(&Party::PartyImpl::OnLocatedParticipationUser, this,
                         _1, _2));
    return;
  }

  Ptr<FunMessage> msg (new FunMessage());
  PartyMessage *party_msg = msg->MutableExtension(_party_message);
  SetAskParticipationMessage(id_, account_id, party_msg);
  account->SendMessage(_party_message, msg, kDefaultEncryption, kTcp);
}


bool Party::PartyImpl::Enter(const string &account_id,
                             const Ptr<Session> &account) {
  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(account);

  AssertNoRollback();

  Ptr<Party::PartyImpl::User> user(new Party::PartyImpl::User());

  LOG_ASSERT(user);

  user->is_self = true;
  user->session = account;

  AccountMap local_sessions;
  AccountMap remote_sessions;

  {
    boost::mutex::scoped_lock lock(mutex_);
    if (not users_.insert(std::make_pair(account_id, user)).second) {
      return false;
    }

    BOOST_FOREACH(const AccountMap::value_type &pair, users_) {
      if (pair.second->is_self) {
        local_sessions.insert(pair);
      } else {
        remote_sessions.insert(pair);
      }
    }
  }

  // 자기 자신을 포함해서 메시지를 전송한다.
  BOOST_FOREACH(const AccountMap::value_type &pair, local_sessions) {
    Ptr<FunMessage> message (new FunMessage());
    PartyMessage *party_message = message->MutableExtension(_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    pair.second->session->SendMessage(
        _party_message, message, kDefaultEncryption, kTcp);
  }

  BOOST_FOREACH(const AccountMap::value_type &pair, remote_sessions) {
    Ptr<FunRpcMessage> message (new FunRpcMessage());
    PartyMessage *party_message = message->MutableExtension(_rpc_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    Rpc::Call(pair.second->peer_id, message);
  }

  return true;
}


bool Party::PartyImpl::Enter(const string &account_id,
                             const PeerId &peer_location) {
  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(not peer_location.is_nil());

  AssertNoRollback();

  Ptr<Party::PartyImpl::User> user(new Party::PartyImpl::User());
  user->is_self = false;
  user->peer_id = peer_location;

  AccountMap local_sessions;
  AccountMap remote_sessions;

  {
    boost::mutex::scoped_lock lock(mutex_);
    if (not users_.insert(std::make_pair(account_id, user)).second) {
      return false;
    }

    BOOST_FOREACH(const AccountMap::value_type &pair, users_) {
      if (pair.second->is_self) {
        local_sessions.insert(pair);
      } else {
        remote_sessions.insert(pair);
      }
    }
  }

  // 자기 자신을 포함해서 메시지를 전송한다.
  BOOST_FOREACH(const AccountMap::value_type &pair, local_sessions) {
    Ptr<FunMessage> message (new FunMessage());
    PartyMessage *party_message = message->MutableExtension(_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    pair.second->session->SendMessage(
        _party_message, message, kDefaultEncryption, kTcp);
  }

  BOOST_FOREACH(const AccountMap::value_type &pair, remote_sessions) {
    Ptr<FunRpcMessage> message (new FunRpcMessage());
    PartyMessage *party_message = message->MutableExtension(_rpc_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    Rpc::Call(pair.second->peer_id, message);
  }

  return true;
}


bool Party::PartyImpl::Leave(const string &account_id) {
  LOG_ASSERT(not account_id.empty());

  AccountMap local_sessions;
  AccountMap remote_sessions;

  {
    boost::mutex::scoped_lock lock(mutex_);
    if (not (users_.erase(account_id) > 0)) {
      return false;
    }

    BOOST_FOREACH(const AccountMap::value_type &pair, users_) {
      if (pair.second->is_self) {
        local_sessions.insert(pair);
      } else {
        remote_sessions.insert(pair);
      }
    }
  }

  // 자기 자신을 포함해서 메시지를 전송한다.
  BOOST_FOREACH(const AccountMap::value_type &pair, local_sessions) {
    Ptr<FunMessage> message (new FunMessage());
    PartyMessage *party_message = message->MutableExtension(_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    pair.second->session->SendMessage(
        _party_message, message, kDefaultEncryption, kTcp);
  }

  BOOST_FOREACH(const AccountMap::value_type &pair, remote_sessions) {
    Ptr<FunRpcMessage> message (new FunRpcMessage());
    PartyMessage *party_message = message->MutableExtension(_rpc_party_message);
    SetEnteredMessage(id_, account_id, pair.first, party_message);
    Rpc::Call(pair.second->peer_id, message);
  }

  return true;
}


void Party::PartyImpl::SendMessage(
    const string &account_id, const string &message_type,
    const Ptr<FunMessage> &message, const Encryption encryption,
    const TransportProtocol protocol) ASSERT_NO_ROLLBACK {
  LOG_ASSERT(not account_id.empty());

  AssertNoRollback();

  AccountMap::const_iterator itr = users_.find(account_id);

  if (itr == users_.end()) {
    LOG(ERROR) << "Failed to send message in party."
               << " invalid account id. party id=" << id_
               << ", account id=" << account_id;
    return;
  }

  if (itr->second->is_self) {
    itr->second->session->SendMessage(message_type, message,
                                      encryption, protocol);
  } else {
    // 다른 서버에 존재하는 세션이다.
    // TODO(inkeun): add rpc send message;
  }

}


void Party::PartyImpl::Broadcast(
    const string &message_type, const Ptr<FunMessage> &message,
    const Encryption encryption,
    const TransportProtocol protocol) ASSERT_NO_ROLLBACK {
  BOOST_FOREACH(const AccountMap::value_type &value, users_) {
    if (value.second->is_self) {
      value.second->session->SendMessage(message_type, message,
                                         encryption, protocol);
    } else {
      // 다른 서버에 존재하는 세션이다.
      // TODO(inkeun): add rpc send message;
    }
  }
}


const string &Party::PartyImpl::GetOwnerId() const {
  boost::mutex::scoped_lock lock(mutex_);
  return owner_id_;
}


const Id &Party::PartyImpl::id() const {
  boost::mutex::scoped_lock lock(mutex_);
  return id_;
}


Party::Party(const Id &id) : impl_(new PartyImpl(id)) {
}


void Party::Create(const CreatedCallback &cb) {
  LOG_ASSERT(the_redis_client);
  LOG_ASSERT(cb);

  Uuid id = RandomGenerator::GenerateUuid();
  const Ptr<Party> party(new Party(id));
  bool inserted = false;
  {
    boost::mutex::scoped_lock lock(the_party_mutex);
    inserted = the_party_map.insert(std::make_pair(id, party)).second;
  }

  if (not inserted) {
    cb(nullptr, true);
  }

  // NOTE(inkeun): 생성 후 레디스에 키를 추가한다.
  // zookeeper api가 열려있지 않기 때문에 우선 레디스를 활용한다.
  // SET에 EX,NX를 한꺼번에 줄 수 있는지 확인 후 추후 변경한다.

  const string redis_key = CreateRedisKey(id);
  the_redis_client->SetNx(redis_key, the_self_peer_id_str,
                          bind(&OnPartySubmittedInRedis, _1, _2,
                               redis_key,party, cb));
}


const Ptr<Party> &Party::Find(const Id &id) {
  LOG_ASSERT(not id.is_nil());

  {
    boost::mutex::scoped_lock lock(the_party_mutex);
    PartyMap::const_iterator itr = the_party_map.find(id);
    if (itr == the_party_map.end()) {
      return kPartyNullPtr;
    }

    return itr->second;
  }
}


void Party::Locate(const Id &id, const LocatedCallback &cb) {
  LOG_ASSERT(not id.is_nil());
  LOG_ASSERT(cb);

  the_redis_client->Get(
      CreateRedisKey(id), bind(&OnLocatedParty, _1, _2, id, cb));
}


void Party::RegisterRecommendedCallback(const RecommendedCallback &cb) {
  LOG_ASSERT(cb);

  boost::mutex::scoped_lock lock(the_callback_mutex);
  the_recommended_callback = cb;
}


void Party::RegisterParticipatedCallback(const ParticipatedCallback &cb) {
  LOG_ASSERT(cb);

  boost::mutex::scoped_lock lock(the_callback_mutex);
  the_participated_callback = cb;
}


void Party::RegisterEnteredCallback(const EnteredCallback &cb) {
  LOG_ASSERT(cb);

  boost::mutex::scoped_lock lock(the_callback_mutex);
  the_entered_callback = cb;
}


void Party::RegisterLeftCallback(const LeftCallback &cb) {
  LOG_ASSERT(cb);

  boost::mutex::scoped_lock lock(the_callback_mutex);
  the_left_callback = cb;
}


void Party::RegisterClosedCallback(const ClosedCallback &cb) {
  LOG_ASSERT(cb);

  boost::mutex::scoped_lock lock(the_callback_mutex);
  the_closed_callback = cb;
}


void Party::SetOwner(const string &id) {
  LOG_ASSERT(not id.empty());

  impl_->SetOwner(id);
}


bool Party::Enter(const string &account_id, const Ptr<Session> &session) {
  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(session);

  return impl_->Enter(account_id, session);
}


bool Party::Enter(const string &account_id, const PeerId &peer_id) {
  LOG_ASSERT(not account_id.empty());
  LOG_ASSERT(not peer_id.is_nil());

  return impl_->Enter(account_id, peer_id);
}


bool Party::Leave(const string &account_id) {
  LOG_ASSERT(not account_id.empty());

  return impl_->Leave(account_id);
}


void Party::Close() {
  impl_->Close();
}


void Party::SendMessage(const string &account_id,
                        const string &message_type,
                        const Ptr<FunMessage> &message,
                        const Encryption encryption,
                        const TransportProtocol protocol) ASSERT_NO_ROLLBACK {
  impl_->SendMessage(account_id, message_type, message, encryption, protocol);
}


void Party::Broadcast(const string &message_type,
                      const Ptr<FunMessage> &message,
                      const Encryption encryption,
                      const TransportProtocol protocol) ASSERT_NO_ROLLBACK {
  impl_->Broadcast(message_type, message, encryption, protocol);
}


const string &Party::GetOwnerId() const {
  return impl_->GetOwnerId();
}


const Id &Party::id() const {
  return impl_->id();
}


namespace {

class PartyInstaller : public Component {
 public:
  static bool Install(const ArgumentMap &arguments) {
    the_redis_client = RedisClient::Create(the_redis_server_ip,
                                           the_redis_server_port,
                                           the_redis_auth_pass,
                                           the_redis_connection_count,
                                           false);
    LOG_ASSERT(the_redis_client);
    the_redis_client->Initialize();

    Rpc::RegisterVoidReplyHandler(
        kPartyRpcMessageType, OnPartyRpcMessageReceived);
    installed = true;
    return true;
  }

  static bool Start() {
    return true;
  }

  static bool Uninstall() {
    return true;
  }
};

}  // unnamed namespace

}  // namespace paper_soldier


REGISTER_STARTABLE_COMPONENT(Party, paper_soldier::PartyInstaller)