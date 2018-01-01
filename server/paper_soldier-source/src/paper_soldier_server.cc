// PLEASE ADD YOUR EVENT HANDLER DECLARATIONS HERE.

#include <boost/bind.hpp>
#include <funapi.h>
#include <gflags/gflags.h>

#include "paper_soldier_object.h"

// You can differentiate game server flavors.
DECLARE_string(app_flavor);

// Adding gflags. In your code, you can refer to them as FLAGS_example_arg3, ...
DEFINE_string(example_arg3, "default_val", "example flag");
DEFINE_int32(example_arg4, 100, "example flag");
DEFINE_bool(example_arg5, false, "example flag");

namespace {


class PaperSoldierServer : public Component {
 public:
  static bool Install(const ArgumentMap &arguments) {
    LOG(INFO) << "Built using Engine version: " << FUNAPI_BUILD_IDENTIFIER;

    // Kickstarts the Engine's ORM.
    // Do not touch this, unless you fully understand what you are doing.
    paper_soldier::ObjectModelInit();

    if (not InstallMonoServer("PaperSoldier.Server", arguments)) {
      return false;
    }

    return true;
  }

  static bool Start() {
    if (not StartMonoServer()) {
      return false;
    }
    return true;
  }

  static bool Uninstall() {
    if (not UninstallMonoServer()) {
      return false;
    }
    return true;
  }
};

}  // unnamed namespace


REGISTER_STARTABLE_COMPONENT(PaperSoldierServer, PaperSoldierServer)
