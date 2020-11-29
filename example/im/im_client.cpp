#include <gflags/gflags.h>
#include <google/protobuf/util/time_util.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include <thread>
#include "echo.pb.h"

DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_string(username, "test", "username");
DEFINE_string(receiver, "test2", "receiver username");
DEFINE_int32(get_message_timeout, 10, "Get New Message timeout in seconds");
DEFINE_bool(if_send, true, "if send message");


std::string generateMessage() {
  static std::string msg_array[] = {"hello", "how are you", "good bye", "LOL"};
  static int length = sizeof(msg_array) / sizeof(std::string);
  return msg_array[random() % length] + "_" + std::to_string(time(nullptr));
}

void GetNewMessage();

int main(int argc, char *argv[]) {
  // Parse gflags. We recommend you to use gflags as well.
  GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
  std::thread get_msg_thread(GetNewMessage);

  brpc::Channel channel;
  // Initialize the channel, NULL means using default options.
  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel";
      return -1;
  }
  example::IMService_Stub stub(&channel);
  while (!brpc::IsAskedToQuit()) {
    if (!FLAGS_if_send) {
      LOG(INFO) << "not send message";
      break;
    }
    example::NewMessageRequest request;
    example::NewMessageResponse response;
    brpc::Controller cntl;

    example::Message *message = request.mutable_new_message();
    message->set_message_type(example::SingleMessage);
    google::protobuf::Timestamp send_time = google::protobuf::util::TimeUtil::SecondsToTimestamp(static_cast<uint64_t>(time(nullptr)));
    *(message->mutable_send_time()) = send_time;
    message->set_sender(FLAGS_username);
    message->set_receiver(FLAGS_receiver);
    message->set_content(generateMessage());
    stub.MessageHandle(&cntl, &request, &response, nullptr);
    if (!cntl.Failed()) {
      LOG(INFO) << "send new message, ret:" << response.rc().retcode();
    } else {
      LOG(WARNING) << cntl.ErrorText();
    }
    usleep(1000 * 1000);
  }
  get_msg_thread.join();
}

void GetNewMessage() {
  brpc::Channel channel;
  // Initialize the channel, NULL means using default options.
  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = (FLAGS_get_message_timeout + 1) * 1000 /*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
      LOG(ERROR) << "Fail to initialize channel";
      return;
  }
  example::IMService_Stub stub(&channel);
  while (!brpc::IsAskedToQuit()) {
    example::GetNewMessageRequest request;
    example::GetNewMessageResponse response;
    brpc::Controller cntl;
    request.set_username(FLAGS_username);
    request.set_timeout(FLAGS_get_message_timeout);
    stub.GetNewMessageHandle(&cntl, &request, &response, NULL);
    if (!cntl.Failed()) {
      LOG(INFO) << "Received response from " << cntl.remote_side();
      if (response.rc().retcode() == example::RET_EMPTY) {
        LOG(INFO) << "Get New Message emtpy";
      } else if (response.rc().retcode() == example::RET_SUCC) {
        for (int i = 0; i < response.messages_size(); ++i) {
          LOG(INFO) << response.messages(i).DebugString();
        }
      } else {
        LOG(WARNING) << "Get New Message failed";
      }
    } else {
        LOG(WARNING) << cntl.ErrorText();
        usleep(1000 * 1000);
    }
  }
}
