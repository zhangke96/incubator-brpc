
#include "user.h"

#include "router.h"
#include <brpc/channel.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <gflags/gflags.h>

namespace example {

std::mutex User::mutex_;
std::unordered_map<std::string, IMService_Stub *> User::stubs_;

IMService_Stub *User::GetStub(std::string url) {
  std::lock_guard<std::mutex> lk(mutex_);
  if (stubs_.count(url)) {
    return stubs_[url];
  } else {
    brpc::Channel *channel = new brpc::Channel();
    brpc::ChannelOptions options;
    options.protocol = "baidu_std";
    // options.connection_type
    options.timeout_ms = 100;
    options.max_retry = 3;
    if (channel->Init(url.c_str(), &options) != 0) {
      LOG(ERROR) << "Fail to init channel";
      return nullptr;
    }
    IMService_Stub *stub = new IMService_Stub(
        channel, google::protobuf::Service::STUB_OWNS_CHANNEL);
    stubs_[url] = stub;
    return stub;
  }
}

ResponseCode User::SendMessage(Message new_message) {
  // 校验是否为自己的好友
  // 发送到receiver处理
  IMService_Stub *stub = GetStub(GetUserRouter(new_message.receiver()));
  InnerNewMessageRequest request;
  *(request.mutable_new_message()) = new_message;
  brpc::Controller cntl;
  InnerNewMessageResponse response;
  stub->InnerMessageHandle(&cntl, &request, &response, nullptr);
  return response.rc();
}

ResponseCode User::HandleNewMessage(Message new_message) {
  // 检验是否为自己的好友发来的消息
  {
    std::lock_guard<bthread::Mutex> lock(message_mutex_);
    received_messages_.push_back(new_message);
    message_cv_.notify_all();
  }
  ResponseCode rc;
  rc.set_retcode(0);
  return rc;
}

std::deque<Message> User::GetNewMessage(long timeout_us) {
  std::unique_lock<bthread::Mutex> lock(message_mutex_);
  // lock.lock();
  int ret = message_cv_.wait_for(lock, timeout_us);
  std::deque<Message> message_queue;
  if (ret == 0) {
    LOG(INFO) << "GetMessage succ, size:" << received_messages_.size();
    std::swap(received_messages_, message_queue);
  } else {
    LOG(INFO) << "GetMessage timeout:" << timeout_us;
  }
  // lock.unlock();
  return message_queue;
}

} // namespace example
