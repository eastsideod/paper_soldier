#ifndef SRC_TESTER_TESTER_H_
#define SRC_TESTER_TESTER_H_

#include <funapi/test/network.h>

#include "../paper_soldier_types.h"


namespace paper_soldier {

class Tester {
 public:
  typedef fun::funtest::Session Session;
  typedef fun::funtest::Network Network;

  typedef fun::funtest::SessionOpenedHandler SessionOpenedHandler;
  typedef fun::funtest::SessionClosedHandler SessionClosedHandler;
  typedef fun::funtest::MessageHandler MessageHandler;
  typedef fun::funtest::MessageHandler2 MessageHandler2;
  typedef fun::funtest::SessionRedirectedHandler SessionRedirectedHandler;

  struct Address {
    struct Detail {
      string ip;
      uint16_t port;
      EncodingScheme encoding_scheme;
    };

    Detail tcp;
    Detail udp;
    Detail http;
  };

  static void Install(const size_t io_theads_size);
  static void Start(const string &tester_type, const size_t tester_count,
                    const Address &address);
  static void Stop(const string &tester_type);

  static void RegisterSessionStateHandler(
      const string &tester_type,
      const SessionOpenedHandler &opened_handler,
      const SessionClosedHandler &closed_handler);

  static void RegisterHandler(const string &tester_type,
                              const string &event_name,
                              const MessageHandler &handler);

  static void RegisterHandler2(const string &tester_type,
                               const string &event_name,
                               const MessageHandler2 &handler);

};

}  // namespace paper_soldier

#endif  // SRC_TESTER_TESTER_H_
