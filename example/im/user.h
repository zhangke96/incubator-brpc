#include "echo.pb.h"
#include <bthread/condition_variable.h>
#include <bthread/mutex.h>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace example {
class User : public std::enable_shared_from_this<User> {
public:
  User(std::string username) : username_(std::move(username)) {}
  ResponseCode SendMessage(Message new_message);
  ResponseCode HandleNewMessage(Message new_message);
  std::deque<Message> GetNewMessage(long timeout_us);

private:
  static IMService_Stub *GetStub(std::string url);
  std::string username_;
  static std::mutex mutex_;
  static std::unordered_map<std::string, IMService_Stub *> stubs_;
  std::deque<Message> received_messages_;
  bthread::Mutex message_mutex_;
  bthread::ConditionVariable message_cv_;
};

} // namespace example
