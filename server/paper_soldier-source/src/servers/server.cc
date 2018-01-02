#include "server.h"

#include <boost/foreach.hpp>
#include <boost/thread/mutex.hpp>

#include <map>


namespace paper_soldier {

namespace {

typedef Server::Impl Impl;
typedef Server::State State;
typedef std::map<string /* Server name */, Ptr<Impl> /* impl */> ServerMap;


const string kDefaultServerName("default");


ServerMap the_server_map;
boost::mutex the_server_mutex;

}  // unnamed namespace


Server::Impl::Impl(const string &name) 
    : name_(name), state_(State::kNone) {
}


const string &Server::Impl::name() const {
  return name_;
}


const State &Server::Impl::state() const {
  return state_;
}


void Server::Register(const string &name,
                      const Ptr<Impl> &Server) {
  LOG_ASSERT(not name.empty());
  LOG_ASSERT(Server);

  {
    boost::mutex::scoped_lock lock(the_server_mutex);
    bool result = the_server_map.insert(
        std::make_pair(name, Server)).second;
    LOG_ASSERT(result);
  }
}


void Server::Start(const string &name) {
  LOG_ASSERT(not name.empty());

  // NOTE(inkeun):
  // 서버 역할은 개별 서버를 띄우는 것이기 때문에,
  // 서버들의 연관성은 없다고 가정한다.
  {
    boost::mutex::scoped_lock lock(the_server_mutex);
    
    if (name == kDefaultServerName) {
      BOOST_FOREACH(ServerMap::value_type pair, the_server_map) {
        LOG_ASSERT(pair.second);
        pair.second->Install();
        pair.second->Start();
      }
      return;
    } 

    ServerMap::const_iterator itr = the_server_map.find(name);
    LOG_ASSERT(itr != the_server_map.end());
    LOG_ASSERT(itr->second);

    itr->second->Install();
    itr->second->Start();
  }
}


void Server::Stop() {
  {
    boost::mutex::scoped_lock lock(the_server_mutex);
    BOOST_REVERSE_FOREACH(ServerMap::value_type pair, the_server_map) {
      LOG_ASSERT(pair.second);
      
      // 등록되었지만 installed 되지 않은 서버들은 무시한다
      if (pair.second->state() == Server::kNone) {
        continue;
      } 

      pair.second->Stop();
      pair.second->Uninstall();
    }
  }
}

}  // namespace paper_soldier