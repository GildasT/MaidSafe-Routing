/*  Copyright 2014 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/routing/routing_node.h"

#include <utility>
#include "asio/use_future.hpp"
#include "boost/exception/diagnostic_information.hpp"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/serialisation.h"

#include "maidsafe/routing/messages/messages.h"
#include "maidsafe/routing/connection_manager.h"
#include "maidsafe/routing/message_header.h"
#include "maidsafe/routing/sentinel.h"
#include "maidsafe/routing/utils.h"

namespace maidsafe {

namespace routing {


RoutingNode::RoutingNode(asio::io_service& io_service, boost::filesystem::path db_location,
                         const passport::Pmid& pmid, std::shared_ptr<Listener> listener_ptr)
    : io_service_(io_service),
      our_fob_(pmid),
      rudp_(),
      bootstrap_handler_(std::move(db_location)),
      connection_manager_(io_service, rudp_, Address(pmid.name()->string())),
      listener_ptr_(listener_ptr),
      filter_(std::chrono::minutes(20)),
      sentinel_(io_service_),
      cache_(std::chrono::minutes(10)) {}

RoutingNode::~RoutingNode() {}

void RoutingNode::MessageReceived(NodeId /* peer_id */, rudp::ReceivedMessage serialised_message) {
  InputVectorStream binary_input_stream{serialised_message};
  MessageHeader header;
  MessageTypeTag tag;
  try {
    Parse(binary_input_stream, header, tag);
  } catch (const std::exception&) {
    LOG(kError) << "header failure." << boost::current_exception_diagnostic_information(true);
    return;
  }

  if (filter_.Check(header.FilterValue()))
    return;  // already seen
  // add to filter as soon as posible
  filter_.Add({header.FilterValue()});

  // We add these to cache
  if (tag == MessageTypeTag::GetDataResponse) {
    auto data = Parse<GetDataResponse>(binary_input_stream);
    cache_.Add(data.key, data.data);
  }
  // if we can satisfy request from cache we do
  if (tag == MessageTypeTag::GetData) {
    auto data = Parse<GetData>(binary_input_stream);
    auto test = cache_.Get(data.key);
    if (test) {
      GetDataResponse response(data.key, test.value());
      auto message(Serialise(CreateReplyHeader(std::move(header)), MessageTypeTag::GetDataResponse,
                             response));
      for (const auto& target : connection_manager_.GetTarget(header.GetSource().first))
        rudp_.Send(target.id, message, [](asio::error_code error) {
          if (error) {
            LOG(kWarning) << "rudp cannot send" << error.message();
          }
        });
      return;
    }
  }

  // send to next node(s) even our close group (swarm mode)
  // FIXME: The variable serialized_message has been moved out at the beginning
  // of this function.
  // FIXME: Uncomment and don't use future.
  for (const auto& target : connection_manager_.GetTarget(header.GetDestination().first))
    rudp_.Send(target.id, serialised_message, [](asio::error_code error) {
      if (error) {
        LOG(kWarning) << "rudp cannot send" << error.message();
      }
    });

  if (!connection_manager_.AddressInCloseGroupRange(header.GetDestination().first))
    return;  // not for us

  // FIXME(dirvine) Sentinel check here!!  :19/01/2015
  switch (tag) {
    case MessageTypeTag::Connect:
      HandleMessage(Parse<Connect>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::ConnectResponse:
      HandleMessage(Parse<ConnectResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::FindGroup:
      HandleMessage(Parse<FindGroup>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::FindGroupResponse:
      HandleMessage(Parse<FindGroupResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::GetData:
      HandleMessage(Parse<GetData>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::GetDataResponse:
      HandleMessage(Parse<GetDataResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PutData:
      HandleMessage(Parse<PutData>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PutDataResponse:
      HandleMessage(Parse<PutDataResponse>(binary_input_stream), std::move(header));
      break;
    case MessageTypeTag::PostMessage:
      HandleMessage(Parse<PostMessage>(binary_input_stream), std::move(header));
      break;
    default:
      LOG(kWarning) << "Received message of unknown type.";
      break;
  }
}


MessageHeader RoutingNode::CreateReplyHeader(MessageHeader orig_header) {
  return MessageHeader{
      DestinationAddress(orig_header.ReturnDestinationAddress()),
      SourceAddress{
          NodeAddress(OurId()),
          orig_header.FromGroup()
              ? boost::optional<
                    std::pair<boost::optional<GroupAddress>, boost::optional<ReplyToAddress>>>(
                    std::make_pair(*orig_header.FromGroup(), boost::none))
              : boost::none},
      orig_header.GetMessageId()};
}


void RoutingNode::ConnectionLost(NodeId peer) { connection_manager_.LostNetworkConnection(peer); }

// reply with our details;
void RoutingNode::HandleMessage(Connect connect, MessageHeader orig_header) {
  if (!connection_manager_.SuggestNodeToAdd(connect.get_requester_id()))
    return;
  auto targets(connection_manager_.GetTarget(connect.get_requester_id()));
  ConnectResponse respond;
  respond.requester_id = connect.get_requester_id();
  respond.requester_endpoints = connect.get_requester_endpoints();
  respond.receiver_id = connect.get_receiver_id();
  assert(connect.get_receiver_id() == OurId());
  respond.receiver_endpoints = NextEndpointPair();
  respond.receiver_fob = passport::PublicPmid(our_fob_);

  MessageHeader header(
      DestinationAddress(orig_header.ReturnDestinationAddress()),
      SourceAddress(std::make_pair(NodeAddress(connection_manager_.OurId()), boost::none)),
      orig_header.GetMessageId(), asymm::Sign(Serialise(respond), keys_.private_key));
  // FIXME(dirvine) Do we need to pass a shared_from_this type object or this may segfault on
  // shutdown
  // :24/01/2015
  for (auto& target : targets) {
    rudp_.Send(target.id, Serialise(header, MessageToTag<ConnectResponse>::value(), respond),
               [connect, this](asio::error_code error_code) {
      if (error_code)
        return;
    });
  }
  auto added =
      connection_manager_.AddNode(NodeInfo(connect.get_requester_id(), connect.get_requester_fob()),
                                  connect.get_requester_endpoints());

  rudp_.Add(rudp::Contact(connect.get_requester_id(), connect.get_requester_endpoints(),
                          connect.get_requester_fob().public_key()),
            [connect, added, this](asio::error_code error) mutable {
    if (error) {
      auto target(connect.get_requester_id());
      this->connection_manager_.DropNode(target);
      return;
    }
  });
  if (added)
    listener_ptr_->HandleCloseGroupDifference(*added);
}

void RoutingNode::HandleMessage(ConnectResponse connect_response, MessageHeader orig_header) {
  if (!connection_manager_.SuggestNodeToAdd(connect_response.requester_id))
    return;
  auto added = connection_manager_.AddNode(NodeInfo(target, connect_response.get_requester_fob()));
  if (added.first)
    rudp_.Add(target, [target, added, this](asio::error_code error) {
      if (error) {
        this->connection_manager_.DropNode(target);
        return;
      }
      if (added.second)
        rudp_.Remove(*added.second.id, asio::use_future).get();
    });
}
void RoutingNode::HandleMessage(FindGroup find_group, MessageHeader orig_header) {
  FindGroupResponse response(find_group);
  response.public_fobs = std::move(connection_manager_.OurCloseGroup());

  MessageHeader header(
      DestinationAddress(find_group.requester_id),
      SourceAddress(std::make_pair(NodeAddress(connection_manager_.OurId()), boost::none)),
      RandomUint32(), asymm::Sign(Serialise(response), keys_.private_key));

  for (const auto& node : connection_manager_.GetTarget(find_group.requester_id)) {
    rudp_.Send(node.id, Serialise(header, MessageToTag<FindGroupResponse>::value(), response),
               asio::use_future).get();
  }
}

void RoutingNode::HandleMessage(FindGroupResponse find_group_reponse, MessageHeader orig_header) {
  // this is called to get our group on bootstrap, we will try and connect to each of these nodes
  for (const auto node : find_group_reponse.public_fobs) {
    if (!connection_manager_.SuggestNodeToAdd(node.id))
      continue;
    // rudp - Add connection
    // if (!error)
    // connection_manager_.Add(node)
  }
}

void RoutingNode::HandleMessage(GetData /*get_data*/, MessageHeader orig_header) {}

void RoutingNode::HandleMessage(GetDataResponse /* get_data_response */,
                                MessageHeader orig_header) {}

void RoutingNode::HandleMessage(PutData /*put_data*/, MessageHeader orig_header) {}

void RoutingNode::HandleMessage(PutDataResponse /*put_data_response*/, MessageHeader orig_header) {}

// void RoutingNode::HandleMessage(Post /*post*/, MessageHeader orig_header) {}

SourceAddress RoutingNode::OurSourceAddress() const {
  return std::make_pair(NodeAddress(connection_manager_.OurId()), boost::optional<GroupAddress>());
}

template <class Message>
void RoutingNode::SendDirect(NodeId target, Message message, SendHandler handler) {
  MessageHeader header(DestinationAddress(target),
                       SourceAddress{NodeAddress(our_id_), boost::optional<GroupAddress>()},
                       message_id_++);

  rudp_.Send(target, Serialise(header, MessageToTag<Message>::value(), message), handler);
}

void RoutingNode::OnBootstrap(asio::error_code error, rudp::Contact contact,
                              std::function<void(asio::error_code, rudp::Contact)> handler) {
  if (error) {
    return handler(error, contact);
  }

  SendDirect(contact.id, FindGroup(our_id_, contact.id),
             [=](asio::error_code error) { handler(error, contact); });
}

}  // namespace routing

}  // namespace maidsafe
