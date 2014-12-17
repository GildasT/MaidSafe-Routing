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

#include "maidsafe/routing/utils.h"

#include <string>
#include <sstream>

#include "maidsafe/common/test.h"
#include "maidsafe/common/utils.h"
#include "maidsafe/common/serialisation/binary_archive.h"
#include "maidsafe/common/serialisation/compile_time_mapper.h"
#include "maidsafe/common/serialisation/serialisation.h"
#include "maidsafe/rudp/contact.h"

namespace maidsafe {

namespace routing {

namespace test {

using address_v6 = boost::asio::ip::address_v6;
using address_v4 = boost::asio::ip::address_v4;
using address = boost::asio::ip::address;

TEST(UtilsTest, BEH_Serialisation) {
  // Load/save endpoints in IPv4 format
  std::string v4 = std::to_string(RandomUint32() % 256);
  for (int i = 0; i != 3; ++i)
    v4 += '.' + std::to_string(RandomUint32() % 256);
  auto test_address_v4 = address_v4::from_string(v4.c_str());
  auto test_address = address{test_address_v4};

  auto endpoint_v4 = rudp::Endpoint{test_address, (RandomUint32() % 65536)};
  auto serialised_endpoint = Serialise(endpoint_v4);

  auto binary_input_stream = InputVectorStream{serialised_endpoint};
  auto parsed_endpoint = Parse<rudp::Endpoint>(binary_input_stream);
  EXPECT_EQ(endpoint_v4, parsed_endpoint);

  // Load/save endpoints in IPv6 format
  std::stringstream v6_stream;
  v6_stream << std::hex << (RandomUint32() % 65536);
  for (int i = 0; i != 7; ++i)
    v6_stream << ':' << RandomUint32() % 65536;
  auto v6 = v6_stream.str();
  auto test_address_v6 = address_v6::from_string(v6.c_str());
  test_address = address{test_address_v6};

  auto endpoint_v6 = rudp::Endpoint{test_address, (RandomUint32() % 65536)};
  serialised_endpoint = Serialise(endpoint_v6);

  binary_input_stream.swap_vector(serialised_endpoint);
  parsed_endpoint = Parse<rudp::Endpoint>(binary_input_stream);
  EXPECT_EQ(endpoint_v6, parsed_endpoint);

  // Load/save EndpointPair
  rudp::EndpointPair endpoint_pair{endpoint_v4, endpoint_v6};
  auto serialised_endpoint_pair = Serialise(endpoint_pair);

  binary_input_stream.swap_vector(serialised_endpoint_pair);
  auto parsed_endpoint_pair = Parse<rudp::EndpointPair>(binary_input_stream);
  EXPECT_EQ(endpoint_v4, parsed_endpoint_pair.local);
  EXPECT_EQ(endpoint_v6, parsed_endpoint_pair.external);
}

}  // namespace test

}  // namespace routing

}  // namespace maidsafe
