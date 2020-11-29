#include "im_server.h"

namespace example {
void IMServiceImpl::MessageHandle(google::protobuf::RpcController *cntl_base,
                                  const NewMessageRequest *request,
                                  NewMessageResponse *response,
                                  google::protobuf::Closure *done) {
  // This object helps you to call done->Run() in RAII style. If you need
  // to process the request asynchronously, pass done_guard.release().
  brpc::ClosureGuard done_guard(done);

  brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);

  // The purpose of following logs is to help you to understand
  // how clients interact with servers more intuitively. You should
  // remove these logs in performance-sensitive servers.
  const Message &new_message = request->new_message();
  const std::string &sender = new_message.sender();
  LOG(INFO) << "Received MessageRequest[log_id=" << cntl->log_id() << "] from "
            << cntl->remote_side() << " to " << cntl->local_side() << " sender "
            << sender << " receiver " << new_message.receiver() << " content "
            << new_message.content();
  std::shared_ptr<User> user;
  if (user_map_.count(sender) == 0) {
    user = std::make_shared<User>(sender);
    user_map_[sender] = user;
  } else {
    user = user_map_[sender];
  }
  ResponseCode rc = user->SendMessage(new_message);
  *(response->mutable_rc()) = rc;

  // You can compress the response by setting Controller, but be aware
  // that compression may be costly, evaluate before turning on.
  // cntl->set_response_compress_type(brpc::COMPRESS_TYPE_GZIP);
}

void IMServiceImpl::GetNewMessageHandle(
    google::protobuf::RpcController *cntl_base,
    const GetNewMessageRequest *request, GetNewMessageResponse *response,
    google::protobuf::Closure *done) {
  brpc::ClosureGuard done_guard(done);
  brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);

  // The purpose of following logs is to help you to understand
  // how clients interact with servers more intuitively. You should
  // remove these logs in performance-sensitive servers.
  std::string username = request->username();
  int32_t timeout = request->timeout(); // 超时s
  LOG(INFO) << "Received GetNewMessageRequest[log_id=" << cntl->log_id() << "] from "
            << cntl->remote_side() << " to " << cntl->local_side()
            << " username " << username << " timeout(s) " << timeout;
  std::shared_ptr<User> user;
  if (user_map_.count(username) == 0) {
    user = std::make_shared<User>(username);
    user_map_[username] = user;
  } else {
    user = user_map_[username];
  }
  std::deque<Message> new_messages = user->GetNewMessage(1000000l * timeout);
  if (new_messages.empty()) {
    response->mutable_rc()->set_retcode(RET_EMPTY);
  } else {
    response->mutable_rc()->set_retcode(RET_SUCC);
    for (auto message : new_messages) {
      *(response->add_messages()) = message;
    }
  }
}

void IMServiceImpl::InnerMessageHandle(
    google::protobuf::RpcController *cntl_base,
    const InnerNewMessageRequest *request, InnerNewMessageResponse *response,
    google::protobuf::Closure *done) {
  brpc::ClosureGuard done_guard(done);

  brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);

  // The purpose of following logs is to help you to understand
  // how clients interact with servers more intuitively. You should
  // remove these logs in performance-sensitive servers.
  const Message &new_message = request->new_message();
  const std::string &sender = new_message.sender();
  const std::string &receiver = new_message.receiver();
  LOG(INFO) << "Received InnerMessageRequest[log_id=" << cntl->log_id() << "] from "
            << cntl->remote_side() << " to " << cntl->local_side() << " sender "
            << sender << " receiver " << new_message.receiver() << " content "
            << new_message.content();
  std::shared_ptr<User> user;
  if (user_map_.count(receiver) == 0) {
    user = std::make_shared<User>(receiver);
    user_map_[receiver] = user;
  } else {
    user = user_map_[receiver];
  }
  ResponseCode rc = user->HandleNewMessage(new_message);
  *(response->mutable_rc()) = rc;
}

} // namespace example
