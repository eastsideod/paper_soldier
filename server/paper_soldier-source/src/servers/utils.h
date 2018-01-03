#ifndef SRC_SERVERS_UTILS_H_
#define SRC_SERVERS_UTILS_H_

#include "../paper_soldier_types.h"
#include "paper_soldier_messages.pb.h"


#define VALIDATE_MESSAGE(msg_type, session, msg, field_number) \
  if (not paper_soldier::HasExtension(msg, field_number)) {\
    SendErrorMessage(paper_soldier::kInvalidMessageType, session);\
    return;\
  }


#define VALIDATE_MESSAGE_FIELDS(session, message)\
  if (not paper_soldier::ValidateMessageFields(message)) {\
    SendErrorMessage(paper_soldier::kInvalidMessageType, session);\
    return;\
  }


namespace paper_soldier {

enum ErrorType {
  kNone = 0,
  kInvalidMessageType,
  kInvalidRequireField
};


template <typename T>
bool HasExtension(const Ptr<FunMessage> &message,
                  const T &field_number) {
  return message->HasExtension(field_number);
}


template <typename T>
bool ValidateMessageFields(const T& message) {
  LOG(FATAL) << "Should be implemented this.";
  return false;
}


void SendErrorMessage(const ErrorType &error_type,
                      const Ptr<Session> &session);

}  // namespace paper_soldier

#endif  // SRC_SERVERS_UTILS_H_