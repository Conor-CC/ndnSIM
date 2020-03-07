/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "ndn-proactive-producer.hpp"
#include "ns3/object.h"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"



#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "NFD/daemon/fw/forwarder.hpp"
#include "NFD/daemon/table/cs.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.ProactiveProducer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ProactiveProducer);

TypeId
ProactiveProducer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ProactiveProducer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<ProactiveProducer>()
      .AddAttribute("Prefix", "Prefix, for which practive producer has the data", StringValue("/"),
                    MakeNameAccessor(&ProactiveProducer::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&ProactiveProducer::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&ProactiveProducer::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&ProactiveProducer::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&ProactiveProducer::m_keyLocator), MakeNameChecker())
      .AddAttribute("SimEnd",
                    "Node is aware of the simulation end time",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_simEnd),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxStart",
                    "Where PCD is triggered",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_contentTrigger_x_start),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxEnd",
                    "Where PCD is no longer triggered",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_contentTrigger_x_end),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTlStart",
                    "When PCD can be triggered",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_contentTrigger_l_start),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTlEnd",
                    "When PCD can no longer be triggered",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_contentTrigger_l_end),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxSpeed",
                    "If the content is mobile, update its position from the perspective of the node",
                    UintegerValue(0), MakeUintegerAccessor(&ProactiveProducer::m_contentTrigger_x_speed),
                    MakeUintegerChecker<uint32_t>());
  return tid;
}

ProactiveProducer::ProactiveProducer()
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
ProactiveProducer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();
  //TODO: emmit data based on factors originally described in proposal
  //This scheduling of transmission is purely for testing purposes
  ProactiveProducer::posChecker();
}

void
ProactiveProducer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void ProactiveProducer::posChecker() {
  for (uint32_t i = 1; i < m_simEnd; i++) {
    Simulator::Schedule(Seconds(i), &ProactiveProducer::posCheckerHelper, this);
  }
}

void ProactiveProducer::posCheckerHelper() {
  // Keeps track of content trigger position, speed and lifetime
  double current_x = GetNode()->GetObject<MobilityModel>()->GetPosition().x;
  double error = 2.0;
  if (current_x >= (m_contentTrigger_x_start - error) && current_x < (m_contentTrigger_x_end + error)) {
    //Check if contentTrigger is alive
    std::cout << "TEST" << '\n';
    double currentTime = Simulator::Now().GetSeconds();
    if (m_contentTrigger_l_start <= currentTime && m_contentTrigger_l_end > currentTime) {
      canDist = true;
    }
  }
  if (canDist && !hasDist) {
    ProactivelyDistributeData();
    hasDist = true;
  }
  m_contentTrigger_x_start = m_contentTrigger_x_start + m_contentTrigger_x_speed;
}

void
ProactiveProducer::ProactivelyDistributeData() {
  std::cout << "AAAAAAAAAAAAAA: " << GetNode()->GetObject<MobilityModel>()->GetPosition().x << std::endl;

  NS_LOG_UNCOND("ProactiveProducer::ProactivelyDistributeData()");

  if (!m_active)
    return;

  shared_ptr<Interest> interest = std::make_shared<ndn::Interest>(m_prefix);
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
  interest->setNonce(rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setInterestLifetime(ndn::time::seconds(300));

  auto data = make_shared<Data>();

  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_prefix);
  nameWithSequence->appendSequenceNumber(0);
  data->setName(m_prefix);

  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));
  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));
  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }
  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));
  data->setSignature(signature);
  // to create real wire encoding
  data->wireEncode();

  App::ProactivelyDistributeData(data); // tracing inside
  NS_LOG_FUNCTION(this << data);

  nfd::cs::Cs& contentStore = GetNode()->GetObject<L3Protocol>()->getForwarder()->getCs();//.insert(*data, false);
  contentStore.insert(*data, false);
  // Making sure proactive data to distribute is in CS to avoid unexpected behaviour
  // Turns out, 258 is a local face! 257 is non local and therefore actually
  // emits the data. Can detect wheter or not a face is local or non-local with
  // the non-static getScope() function in the Face class
  // TODO: Make a new out-bound, non-local face so we dont have to rely on face 257
  // being a present
  shared_ptr<Face> face = GetNode()->GetObject<L3Protocol>()->getFaceById(257);
  face->getScope();
  std::cout << face->getScope() << '\n';
  // Kick-starts the distribution process. Should only be called if the data is definitely within
  // this nodes content store
  GetNode()->GetObject<L3Protocol>()->getForwarder()->startProcessInterest(*face, *interest);

  NS_LOG_UNCOND("Emmiting PCD " << *data);
}

} // namespace ndn
} // namespace ns3
