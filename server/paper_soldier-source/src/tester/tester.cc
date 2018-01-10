#include "tester.h"

#include <boost/thread/mutex.hpp>
#include <vector>

#define TLOG(T, X) LOG(T) << "[" << X << "] "


namespace paper_soldier {

namespace {

typedef Tester::Address Address;
typedef Tester::SessionOpenedHandler SessionOpenedHandler;
typedef Tester::SessionClosedHandler SessionClosedHandler;
typedef Tester::MessageHandler MessageHandler;
typedef Tester::MessageHandler2 MessageHandler2;
typedef Tester::SessionRedirectedHandler SessionRedirectedHandler;

typedef std::vector<Ptr<Tester::Session> > TesterSessionVector;
typedef std::map<const fun::Uuid /*session id*/,
                 Ptr<Tester::Session> /*session*/ >SessionMap;
typedef std::map<const string /*tester_type*/, SessionMap> TesterSessionMap;

typedef std::map<const string /*event_name*/,
                 const MessageHandler /*handler*/> HandlerMap;
typedef std::map<const string /*event_name*/,
                 const MessageHandler2 /*handler*/> HandlerMap2;
typedef std::map<const string /*tester_name*/, HandlerMap> TesterHandlerMap;
typedef std::map<const string /*tester_name*/, HandlerMap2> TesterHandlerMap2;


struct SessionStateHandler {
  SessionStateHandler()
      : opened_handler(NULL), closed_handler(NULL) {
  }

  SessionOpenedHandler opened_handler;
  SessionClosedHandler closed_handler;
};

typedef std::map<const string /*tester_name*/, SessionStateHandler>
    SessionStateHandlerMap;


bool installed = false;
TesterHandlerMap the_handler_map;
TesterHandlerMap2 the_handler_map2;
SessionStateHandlerMap the_session_state_handler_map;

boost::mutex the_session_map_mutex;
TesterSessionMap the_session_map;


void OnDefaultSessionOpened(const Ptr<Tester::Session> &session) {
  boost::mutex &mutex = session->GetContextMutex();
  string tester_type("");
  {
    boost::mutex::scoped_lock lock(mutex);
    Json &json = session->GetContext();
    LOG_ASSERT(json.HasAttribute("type", Json::kString));
    tester_type = json["type"].GetString();
  }

  TLOG(INFO, tester_type) << "session opened.";
  LOG_ASSERT(not tester_type.empty());

  SessionStateHandlerMap::const_iterator itr =
      the_session_state_handler_map.find(tester_type);

  LOG_ASSERT(itr != the_session_state_handler_map.end());

  itr->second.opened_handler(session);
}


void OnDefaultSessionClosed(const Ptr<Tester::Session> &session,
                            SessionCloseReason reason) {
  boost::mutex &mutex = session->GetContextMutex();
  string tester_type("");
  {
    boost::mutex::scoped_lock lock(mutex);
    Json &json = session->GetContext();
    LOG_ASSERT(json.HasAttribute("type", Json::kString));
    tester_type = json["type"].GetString();
  }

  LOG_ASSERT(not tester_type.empty());
  TLOG(INFO, tester_type) << "session closed.";

  const SessionStateHandlerMap::const_iterator itr =
      the_session_state_handler_map.find(tester_type);

  LOG_ASSERT (itr != the_session_state_handler_map.end());

  itr->second.closed_handler(
      session, SessionCloseReason::kClosedForServerDid);
}


void DefaultMessageHandler(const string &tester_type,
                           const string &event_name,
                           const Ptr<Tester::Session> &session,
                           const Json &message) {
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(not event_name.empty());

  TLOG(INFO, tester_type) << event_name << "json message received.";

  TesterHandlerMap::const_iterator itr = the_handler_map.find(tester_type);
  LOG_ASSERT(itr != the_handler_map.end());

  HandlerMap::const_iterator itr2 = itr->second.find(event_name);
  LOG_ASSERT(itr2 != itr->second.end());
  itr2->second(session, message);
}


void DefaultMessageHandler2(const string &tester_type,
                            const string &event_name,
                            const Ptr<Tester::Session> &session,
                            const Ptr<FunMessage> &message) {
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(not event_name.empty());

  TLOG(INFO, tester_type) << event_name << "pbuf message received.";

  TesterHandlerMap2::const_iterator itr = the_handler_map2.find(tester_type);
  LOG_ASSERT(itr != the_handler_map2.end());

  HandlerMap2::const_iterator itr2 = itr->second.find(event_name);
  LOG_ASSERT(itr2 != itr->second.end());
  itr2->second(session, message);
}


void CreateSession(const string &tester_type, const size_t count,
                   TesterSessionVector *vector) {
  static const string kType("type");
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(vector);

  {
    boost::mutex::scoped_lock lock(the_session_map_mutex);
    for (size_t i = 0; i < count; ++i) {
      Ptr<Tester::Session> session = Tester::Session::Create();
      LOG_ASSERT(session);
      vector->push_back(session);

      session->GetContext()[kType] = tester_type;
      std::pair<TesterSessionMap::iterator, bool> pair = the_session_map.insert(
          std::make_pair(tester_type, SessionMap()));
      pair.first->second.insert(std::make_pair(session->id(), session));
    }
  }
}

}  // unnamed namespace


void Tester::Install(const size_t io_threads_size) {
  LOG_ASSERT(io_threads_size > 0);
  installed = true;

  Tester::Network::Install(OnDefaultSessionOpened,
                           OnDefaultSessionClosed,
                           io_threads_size);
}


void Tester::Start(const string &tester_type, const size_t tester_count,
                   const Tester::Address &address) {
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(tester_count > 0);

  TesterSessionVector vector;
  CreateSession(tester_type, tester_count, &vector);
  LOG_ASSERT(not vector.empty());

  TLOG(INFO, tester_type) << "session created. session count=" << vector.size();
  LOG(INFO) << "====================================";
  TLOG(INFO, tester_type) << "address info";
  TLOG(INFO, tester_type) << "tcp ip=" << address.tcp.ip
			  << ", port=" << address.tcp.port
			  << ", encoding_scheme=" << address.tcp.encoding_scheme;

  TLOG(INFO, tester_type) << "udp ip=" << address.udp.ip
			  << ", port=" << address.udp.port
			  << ", encoding_scheme=" << address.udp.encoding_scheme;
  LOG(INFO) << "====================================";
  for (const auto &i : vector) {
    // connect tcp
    if (not address.tcp.ip.empty() && address.tcp.port > 0 &&
        address.tcp.encoding_scheme != EncodingScheme::kUnknownEncoding) {
      i->ConnectTcp(address.tcp.ip, address.tcp.port,
                    address.tcp.encoding_scheme);
    }

    // connect udp
    if (not address.udp.ip.empty() && address.udp.port > 0 &&
        address.tcp.encoding_scheme != EncodingScheme::kUnknownEncoding) {
      i->ConnectUdp(address.udp.ip, address.udp.port,
                    address.udp.encoding_scheme);
    }
  }
}


void Tester::Stop(const string &tester_type) {
  LOG_ASSERT(not tester_type.empty());

  {
    boost::mutex::scoped_lock lock(the_session_map_mutex);
    TesterSessionMap::const_iterator itr = the_session_map.find(tester_type);
    LOG_ASSERT(itr != the_session_map.end());

    for(const auto &pair : itr->second) {
      pair.second->Close();
    }
  }
}


void Tester::RegisterSessionStateHandler(
    const string &tester_type,
    const SessionOpenedHandler &opened_handler,
    const SessionClosedHandler &closed_handler) {
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(opened_handler);
  LOG_ASSERT(closed_handler);

  SessionStateHandler state_handler;
  state_handler.opened_handler = opened_handler;
  state_handler.closed_handler = closed_handler;

  the_session_state_handler_map.insert(
    std::make_pair(tester_type, state_handler));
}


void Tester::RegisterHandler(const string &tester_type,
                             const string &event_name,
                             const MessageHandler &handler) {
  LOG_ASSERT(not installed);
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(not event_name.empty());
  LOG_ASSERT(handler);

  std::pair<TesterHandlerMap::iterator, bool> pair = the_handler_map.insert(
      std::make_pair(tester_type, HandlerMap()));
  pair.first->second.insert(std::make_pair(event_name, handler));

  Tester::Network::Register(
      event_name, bind(
          &DefaultMessageHandler, tester_type, event_name, _1, _2));
}


void Tester::RegisterHandler2(const string &tester_type,
                              const string &event_name,
                              const MessageHandler2 &handler) {
  LOG_ASSERT(not installed);
  LOG_ASSERT(not tester_type.empty());
  LOG_ASSERT(not event_name.empty());
  LOG_ASSERT(handler);

  auto pair = the_handler_map2.insert(
      std::make_pair(tester_type, HandlerMap2()));
  pair.first->second.insert(std::make_pair(event_name, handler));

  Tester::Network::Register2(
      event_name, bind(
          &DefaultMessageHandler2, tester_type, event_name, _1, _2));
}

}  // namespace paper_soldier
