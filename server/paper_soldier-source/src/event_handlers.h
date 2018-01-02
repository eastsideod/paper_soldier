// PLEASE ADD YOUR EVENT HANDLER DECLARATIONS HERE.

#ifndef SRC_EVENT_HANDLERS_H_
#define SRC_EVENT_HANDLERS_H_

#include <funapi.h>

#include "paper_soldier_messages.pb.h"
#include "paper_soldier_object.h"
#include "paper_soldier_rpc_messages.pb.h"
#include "paper_soldier_dedicated_server_rpc_messages.pb.h"
#include "paper_soldier_types.h"


namespace paper_soldier {

void RegisterEventHandlers();

}  // namespace paper_soldier

#endif  // SRC_EVENT_HANDLERS_H_
