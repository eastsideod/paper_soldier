#ifndef SRC_SERVERS_SERVER_H_
#define SRC_SERVERS_SERVER_H_

#include "../paper_soldier_types.h"


namespace paper_soldier {

class Server {
 public:
  enum State {
    kNone,
    kInstalled,
    kStarted,
    kStopped,
    kUninstalled
  };

  // TODO(inkeun): cc 파일로 옮긴다.
  class Impl {
   public:
    explicit Impl(const string &name);

    virtual void Install() = 0;
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Uninstall() = 0;

    const string &name() const;
    const State &state() const;
  
   protected:
    const string name_;
    State state_;
  };

  static void Register(const string &name,
                       const Ptr<Impl> &server);

  static void Start(const string &name);
  static void Stop();
};

}  // namespace paper_soldier

#endif  // SRC_SERVERS_SERVER_H_
