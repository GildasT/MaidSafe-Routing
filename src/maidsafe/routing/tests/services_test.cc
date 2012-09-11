/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#include <memory>
#include <vector>

#include "maidsafe/common/log.h"
#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/network_utils.h"
#include "maidsafe/routing/non_routing_table.h"
#include "maidsafe/routing/parameters.h"
#include "maidsafe/routing/routing_pb.h"
#include "maidsafe/routing/rpcs.h"
#include "maidsafe/routing/service.h"
#include "maidsafe/routing/tests/test_utils.h"


namespace maidsafe {

namespace routing {

namespace test {

namespace {

typedef boost::asio::ip::udp::endpoint Endpoint;

}  // unnamed namespace

TEST(ServicesTest, BEH_Ping) {
  asymm::Keys keys;
  keys.identity = RandomString(64);
  RoutingTable RT(keys, false);
  NodeInfo node;
  rudp::ManagedConnections rudp;
  protobuf::PingRequest ping_request;
  // somebody pings us
  protobuf::Message message = rpcs::Ping(NodeId(keys.identity), "me");
  EXPECT_TRUE(message.destination_id() == keys.identity);
  EXPECT_TRUE(ping_request.ParseFromString(message.data(0)));  // us
  EXPECT_TRUE(ping_request.IsInitialized());
  // run message through Service
  service::Ping(RT, message);
  EXPECT_EQ(1, message.type());
  EXPECT_EQ(message.request(), false);
  EXPECT_NE(message.data_size(), 0);
  EXPECT_TRUE(message.source_id() == keys.identity);
  EXPECT_EQ(message.replication(), 1);
  EXPECT_EQ(message.type(), 1);
  EXPECT_EQ(message.request(), false);
  EXPECT_EQ(message.id(), 0);
  EXPECT_FALSE(message.client_node());
  // EXPECT_FALSE(message.has_relay());
}

TEST(ServicesTest, BEH_FindNodes) {
  NodeInfo us(MakeNode());
  NodeInfo them(MakeNode());
  asymm::Keys keys;
  keys.identity = us.node_id.String();
  keys.public_key = us.public_key;
  RoutingTable RT(keys, false);
  protobuf::Message message = rpcs::FindNodes(us.node_id, us.node_id, 8);
  service::FindNodes(RT, message);
  protobuf::FindNodesResponse find_nodes_respose;
  EXPECT_TRUE(find_nodes_respose.ParseFromString(message.data(0)));
//  EXPECT_TRUE(find_nodes_respose.nodes().size() > 0);  // will only have us
//  EXPECT_EQ(find_nodes_respose.nodes().Get(1), us.node_id.String());
  EXPECT_TRUE(find_nodes_respose.has_timestamp());
  EXPECT_TRUE(find_nodes_respose.timestamp() > static_cast<int32_t>(GetTimeStamp() - 2));
  EXPECT_TRUE(find_nodes_respose.timestamp() < static_cast<int32_t>(GetTimeStamp() + 1));
  EXPECT_EQ(message.destination_id(), us.node_id.String());
  EXPECT_EQ(message.source_id(), us.node_id.String());
  EXPECT_NE(message.data_size(), 0);
  EXPECT_TRUE(message.direct());
  EXPECT_EQ(message.replication(), 1);
  EXPECT_EQ(message.type(), 3);
  EXPECT_EQ(message.request(), false);
  EXPECT_EQ(message.id(), 0);
  EXPECT_FALSE(message.client_node());
  // EXPECT_FALSE(message.has_relay());
}

// TEST(ServicesTest, BEH_ProxyConnect) {
//   asymm::Keys my_keys;
//   my_keys.identity = RandomString(64);
//   asymm::Keys keys;
//   keys.identity = RandomString(64);
//   RoutingTable RT(keys, false);
//   NonRoutingTable NRT(keys);
//   AsioService asio_service(0);
//   Timer timer(asio_service);
//   NodeInfo node;
//   NetworkUtils network(RT, NRT, timer);
//   protobuf::ProxyConnectRequest proxy_connect_request;
//   // they send us an proxy connect rpc
//   rudp::EndpointPair endpoint_pair;
//   endpoint_pair.external =  Endpoint(boost::asio::ip::address_v4::loopback(), GetRandomPort());
//   endpoint_pair.local =  Endpoint(boost::asio::ip::address_v4::loopback(), GetRandomPort());
//   protobuf::Message message = rpcs::ProxyConnect(NodeId(keys.identity), NodeId(my_keys.identity),
//                                                  endpoint_pair);
//   EXPECT_TRUE(message.destination_id() == keys.identity);
//   EXPECT_TRUE(proxy_connect_request.ParseFromString(message.data(0)));  // us
//   EXPECT_TRUE(proxy_connect_request.IsInitialized());
//   // run message through Service
//   service::ProxyConnect(RT, network, message);
//   protobuf::ProxyConnectResponse proxy_connect_respose;
//   EXPECT_TRUE(proxy_connect_respose.ParseFromString(message.data(0)));
//   EXPECT_EQ(protobuf::kFailure, proxy_connect_respose.result());
//   EXPECT_NE(message.data_size(), 0);
//   EXPECT_TRUE(message.direct());
//   EXPECT_TRUE(message.source_id() == keys.identity);
//   EXPECT_EQ(1, message.replication());
//   EXPECT_EQ(4, message.type());
//   EXPECT_EQ(message.request(), false);
//   EXPECT_EQ(0, message.id());
//   EXPECT_FALSE(message.client_node());
//   // EXPECT_FALSE(message.has_relay());
//   // TODO(Prakash): Need to add peer to connect and test for kSuccess & kAlreadyConnected.
// }
// 
}  // namespace test

}  // namespace routing

}  // namespace maidsafe