#include <bthread/bthread.h>
#include <bthread/timer_thread.h>
#include <brpc/channel.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <gflags/gflags.h>
#include <google/protobuf/util/time_util.h>

#include <random>
#include <string>
#include <thread>

#include "echo.pb.h"

DEFINE_string(protocol, "baidu_std",
              "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "",
              "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "0.0.0.0:8000", "IP Address of server");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(user_count, 100, "test user count");
DEFINE_int32(begin_id, 1, "begin user id");
DEFINE_int32(end_id, 100, "end user id");
DEFINE_int32(get_message_timeout, 10, "Get New Message timeout in seconds");
DEFINE_bool(if_send, true, "if send message");

bthread::TimerThread g_timer_thread;

std::string generateMessage(std::string receiver) {
  static std::string msg_array[] = {"hello", "how are you", "good bye", "LOL"};
  static int length = sizeof(msg_array) / sizeof(std::string);
  return receiver + "_" + msg_array[random() % length] + "_" + std::to_string(time(nullptr));
}

std::string generateReceiver(int32_t my_id) {
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> distrib(0, FLAGS_user_count);
  int receiver_id = 0;
  // 生成一个和自己不相等的id
  while ((receiver_id = distrib(gen)) == my_id)
    ;
  return std::to_string(receiver_id);
}

bool CheckMessage(int32_t my_id, const std::string &msg) {
  std::string my_username(std::to_string(my_id));
  std::size_t index = msg.find_first_of("_");
  if (index != std::string::npos) {
    std::string receive_username = msg.substr(0, index);
    if (receive_username == my_username) {
      return true;
    }
  }
  return false;
}

brpc::Channel channel;

static bool InitChannel() {
  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms /*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return false;
  }
  return true;
}

// void SendMessageTest(int32_t my_id);
void* SendMessageTest(void *p_my_id);

// void GetNewMessage(int32_t my_id);
void* GetNewMessage(void *p_my_id);

void SendMessage(int32_t my_id);

void OnSendMessageDone(example::NewMessageResponse *response, brpc::Controller* cntl, int32_t my_id);

void GetNewMessage(int32_t my_id);

void OnGetNewMessageDone(example::GetNewMessageResponse *response, brpc::Controller* cntl, int32_t my_id);

int main(int argc, char *argv[]) {
  // Parse gflags. We recommend you to use gflags as well.
  GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
  if (FLAGS_begin_id > FLAGS_user_count || FLAGS_end_id > FLAGS_user_count) {
    LOG(ERROR) << "user id range check failed, begin_id: " << FLAGS_begin_id << ", end_id:" << FLAGS_end_id
               << ", user count:" << FLAGS_user_count;
    exit(1);
  }
  g_timer_thread.start(NULL);
  if (!InitChannel()) {
    exit(1);
  }
  for (int32_t i = FLAGS_begin_id; i <= FLAGS_end_id; ++i) {
    SendMessage(i);
    GetNewMessage(i);
  }
  while (!brpc::IsAskedToQuit()) {
    bthread_usleep(1000 * 1000);
  }
  // std::vector<bthread_t> threads;
  // for (int32_t i = FLAGS_begin_id; i <= FLAGS_end_id; ++i) {
  //   bthread_t send_thread;
  //   if (bthread_start_background(&send_thread, NULL, SendMessageTest, static_cast<void *>(&i))) {
  //     LOG(ERROR) << "Fail to create send bthread for user:" << i;
  //     break;
  //   }
  //   threads.push_back(send_thread);
  //   bthread_t receive_thread;
  //   if (bthread_start_background(&receive_thread, NULL, GetNewMessage, static_cast<void *>(&i))) {
  //     LOG(ERROR) << "Fail to create receive bthread for user:" << i;
  //     break;
  //   }
  //   threads.push_back(receive_thread);
  // }
  // for (auto &thread : threads) {
  //   bthread_join(thread, nullptr);
  // }
}

void* SendMessageTest(void *p_my_id) {
  int32_t my_id = *(static_cast<int32_t*>(p_my_id));
  std::string username(std::to_string(my_id));
  brpc::Channel channel;
  // Initialize the channel, NULL means using default options.
  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = FLAGS_timeout_ms /*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    exit(-1);
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
    google::protobuf::Timestamp send_time =
        google::protobuf::util::TimeUtil::SecondsToTimestamp(
            static_cast<uint64_t>(time(nullptr)));
    *(message->mutable_send_time()) = send_time;
    message->set_sender(username);
    // 随机发给一个人,不发给自己
    std::string receiver = generateReceiver(my_id);
    message->set_receiver(receiver);
    message->set_content(generateMessage(receiver));
    stub.MessageHandle(&cntl, &request, &response, nullptr);
    if (!cntl.Failed()) {
      LOG(INFO) << "send new message, ret:" << response.rc().retcode();
    } else {
      LOG(WARNING) << cntl.ErrorText();
    }
    bthread_usleep(1000 * 1000);
  }
  return nullptr;
}

void SendMessage(int32_t my_id) {
  std::string username(std::to_string(my_id));
  example::IMService_Stub stub(&channel);
  example::NewMessageRequest request;
  example::NewMessageResponse *response = new example::NewMessageResponse();
  brpc::Controller *cntl = new brpc::Controller();

  example::Message *message = request.mutable_new_message();
  message->set_message_type(example::SingleMessage);
  google::protobuf::Timestamp send_time =
        google::protobuf::util::TimeUtil::SecondsToTimestamp(
            static_cast<uint64_t>(time(nullptr)));
  *(message->mutable_send_time()) = send_time;
  message->set_sender(username);
  // 随机发给一个人,不发给自己
  std::string receiver = generateReceiver(my_id);
  message->set_receiver(receiver);
  message->set_content(generateMessage(receiver));
  stub.MessageHandle(cntl, &request, response, brpc::NewCallback(OnSendMessageDone, response, cntl, my_id));
}

void DelaySendMessage(void *p_my_id) {
  int32_t *id = static_cast<int32_t *>(p_my_id);
  std::unique_ptr<int32_t> id_gurad(id);
  SendMessage(*id);
}

void OnSendMessageDone(example::NewMessageResponse *response, brpc::Controller* cntl, int32_t my_id) {
  // delete
  std::unique_ptr<example::NewMessageResponse> response_guard(response);
  std::unique_ptr<brpc::Controller> cntl_guard(cntl);
  if (!cntl->Failed()) {
      LOG(INFO) << "send new message, ret:" << response->rc().retcode();
  } else {
    LOG(WARNING) << cntl->ErrorText();
  }
  // bthread_usleep(1000 * 1000);
  int32_t *p_id = new int32_t(my_id);
  g_timer_thread.schedule(DelaySendMessage, p_id, butil::microseconds_from_now(1000 * 1000));
  // SendMessage(my_id);
}

void* GetNewMessage(void *p_my_id) {
  int32_t my_id = *(static_cast<int32_t*>(p_my_id));
  std::string username(std::to_string(my_id));
  brpc::Channel channel;
  // Initialize the channel, NULL means using default options.
  brpc::ChannelOptions options;
  options.protocol = FLAGS_protocol;
  options.connection_type = FLAGS_connection_type;
  options.timeout_ms = (FLAGS_get_message_timeout + 1) * 1000 /*milliseconds*/;
  if (channel.Init(FLAGS_server.c_str(), &options) != 0) {
    LOG(ERROR) << "Fail to initialize channel";
    return nullptr;
  }
  example::IMService_Stub stub(&channel);
  while (!brpc::IsAskedToQuit()) {
    example::GetNewMessageRequest request;
    example::GetNewMessageResponse response;
    brpc::Controller cntl;
    request.set_username(username);
    request.set_timeout(FLAGS_get_message_timeout);
    stub.GetNewMessageHandle(&cntl, &request, &response, NULL);
    if (!cntl.Failed()) {
      LOG(INFO) << "Received response from " << cntl.remote_side();
      if (response.rc().retcode() == example::RET_EMPTY) {
        LOG(INFO) << "Get New Message emtpy";
      } else if (response.rc().retcode() == example::RET_SUCC) {
        for (int i = 0; i < response.messages_size(); ++i) {
          LOG(INFO) << response.messages(i).DebugString();
          // 校验消息
        }
      } else {
        LOG(WARNING) << "Get New Message failed";
      }
    } else {
      LOG(WARNING) << cntl.ErrorText();
      bthread_usleep(1000 * 1000);
    }
  }
  return nullptr;
}

void GetNewMessage(int32_t my_id) {
  std::string username(std::to_string(my_id));
  example::IMService_Stub stub(&channel);
  example::GetNewMessageRequest request;
  example::GetNewMessageResponse *response = new example::GetNewMessageResponse();
  brpc::Controller *cntl = new brpc::Controller();
  cntl->set_timeout_ms(FLAGS_get_message_timeout * 1000 + 100);
  request.set_username(username);
  request.set_timeout(FLAGS_get_message_timeout);
  stub.GetNewMessageHandle(cntl, &request, response, brpc::NewCallback(OnGetNewMessageDone, response, cntl, my_id));
}

void DelayGetNewMessage(void *p_my_id) {
  int32_t *id = static_cast<int32_t *>(p_my_id);
  std::unique_ptr<int32_t> id_gurad(id);
  GetNewMessage(*id);
}

void OnGetNewMessageDone(example::GetNewMessageResponse *response, brpc::Controller *cntl, int32_t my_id) {
  std::unique_ptr<example::GetNewMessageResponse> response_guard(response);
  std::unique_ptr<brpc::Controller> cntl_guard(cntl);
  if (!cntl->Failed()) {
    LOG(INFO) << "Received response from " << cntl->remote_side();
    if (response->rc().retcode() == example::RET_EMPTY) {
      LOG(INFO) << "Get New Message emtpy";
    } else if (response->rc().retcode() == example::RET_SUCC) {
      for (int i = 0; i < response->messages_size(); ++i) {
        LOG(INFO) << response->messages(i).DebugString();
        // 校验消息
      }
    } else {
      LOG(WARNING) << "Get New Message failed";
    }
  } else {
    LOG(WARNING) << cntl->ErrorText();
    // bthread_usleep(1000 * 1000);
    int32_t *p_id = new int32_t(my_id);
    g_timer_thread.schedule(DelayGetNewMessage, p_id, butil::microseconds_from_now(1000 * 1000));
    return;
  }
  GetNewMessage(my_id);
}
