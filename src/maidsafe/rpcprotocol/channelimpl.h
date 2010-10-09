/* Copyright (c) 2009 maidsafe.net limited
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.
    * Neither the name of the maidsafe.net limited nor the names of its
    contributors may be used to endorse or promote products derived from this
    software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef MAIDSAFE_RPCPROTOCOL_CHANNELIMPL_H_
#define MAIDSAFE_RPCPROTOCOL_CHANNELIMPL_H_

#include <boost/asio.hpp>
#include <google/protobuf/service.h>

#include <memory>
#include <string>

#include "maidsafe/maidsafe-dht_config.h"
#include "maidsafe/base/utils.h"

namespace transport {
class UdtConnection;
class UdtTransport;
}  // namespace transport

namespace rpcprotocol {

class Controller;
class ChannelManager;
class RpcMessage;

class ControllerImpl {
 public:
  ControllerImpl() : timeout_(-1), time_sent_(0), time_received_(0),
                     rtt_(0.0), failure_(), method_(), socket_id_(0),
                     udt_connection_() {}
  void SetFailed(const std::string &failure) { failure_ = failure; }
  void Reset();
  bool Failed() const { return !failure_.empty(); }
  std::string ErrorText() const { return failure_; }
  void StartCancel() {}
  bool IsCanceled() const { return false; }
  void NotifyOnCancel(google::protobuf::Closure*) {}
  // returns time between sending and receiving the RPC in milliseconds
  boost::uint32_t Duration() const {
    return time_sent_ < time_received_ ?
        static_cast<boost::uint32_t>(time_received_ - time_sent_) : 0;
  }
  // set sending time
  void StartRpcTimer() { time_sent_ = base::GetEpochMilliseconds(); }
  // set receiving time
  void StopRpcTimer() { time_received_ = base::GetEpochMilliseconds(); }
  // rtt in milliseconds
  void set_rtt(const float &rtt) { rtt_ = rtt; }
  float rtt() const { return rtt_; }
  void set_socket_id(const SocketId &socket_id) { socket_id_ = socket_id; }
  SocketId socket_id() const { return socket_id_; }
  void set_method(const std::string &method) { method_ = method; }
  std::string method() const { return method_; }
  void set_timeout(const boost::uint32_t &timeout) { timeout_ = timeout; }
  boost::uint32_t timeout() const { return timeout_; }
  void set_udt_connection(
      boost::shared_ptr<transport::UdtConnection> udt_connection) {
    udt_connection_ = udt_connection;
  }
  boost::shared_ptr<transport::UdtConnection> udt_connection() const {
    return udt_connection_;
  }

 private:
  boost::uint32_t timeout_;
  boost::uint64_t time_sent_, time_received_;
  float rtt_;
  std::string failure_, method_;
  SocketId socket_id_;
  boost::shared_ptr<transport::UdtConnection> udt_connection_;
};

class ChannelImpl {
 public:
  ChannelImpl(boost::shared_ptr<ChannelManager> channel_manager,
              boost::shared_ptr<transport::UdtTransport> udt_transport);
  ChannelImpl(boost::shared_ptr<ChannelManager> channel_manager,
              const IP &remote_ip, const Port &remote_port,
              const IP &local_ip, const Port &local_port,
              const IP &rendezvous_ip, const Port &rendezvous_port);
  ChannelImpl(boost::shared_ptr<ChannelManager> channel_manager,
              boost::shared_ptr<transport::UdtTransport> udt_transport,
              const IP &remote_ip, const Port &remote_port,
              const IP &local_ip, const Port &local_port,
              const IP &rendezvous_ip, const Port &rendezvous_port);
  ~ChannelImpl();
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *rpc_controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
  void SetService(google::protobuf::Service *service) { service_ = service; }
  void HandleRequest(const rpcprotocol::RpcMessage &rpc_message,
                     const SocketId &socket_id,
                     const float &rtt);
 private:
  ChannelImpl(const ChannelImpl&);
  ChannelImpl& operator=(const ChannelImpl&);
  void SendResponse(const google::protobuf::Message *response,
                    boost::shared_ptr<Controller> controller);
  std::string GetServiceName(const std::string &full_name);
  boost::shared_ptr<ChannelManager> channel_manager_;
  boost::shared_ptr<transport::UdtTransport> udt_transport_;
  boost::shared_ptr<transport::UdtConnection> udt_connection_;
  google::protobuf::Service *service_;
  IP remote_ip_, local_ip_, rendezvous_ip_;
  Port remote_port_, local_port_, rendezvous_port_;
  boost::uint32_t id_;
  bool local_transport_;
};

}  // namespace rpcprotocol

#endif  // MAIDSAFE_RPCPROTOCOL_CHANNELIMPL_H_
