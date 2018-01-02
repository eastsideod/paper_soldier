// PLEASE ADD YOUR EVENT HANDLER DECLARATIONS HERE.

#include <funapi.h>
#include <gflags/gflags.h>

#include "event_handlers.h"
#include "servers/server.h"
#include "paper_soldier_object.h"

// You can differentiate game server flavors.
DECLARE_string(app_flavor);


namespace {

typedef paper_soldier::Server Server;


class PaperSoldierServer : public Component {
 public:
  static bool Install(const ArgumentMap &arguments) {
    LOG(INFO) << "Built using Engine version: " << FUNAPI_BUILD_IDENTIFIER;

    // Kickstarts the Engine's ORM.
    // Do not touch this, unless you fully understand what you are doing.
    paper_soldier::ObjectModelInit();
    return true;
  }

  static bool Start() {
    Server::Start(FLAGS_app_flavor);
    return true;
  }

  static bool Uninstall() {
    Server::Stop();
    return true;
  }
};

}  // unnamed namespace


REGISTER_STARTABLE_COMPONENT(PaperSoldierServer, PaperSoldierServer)
