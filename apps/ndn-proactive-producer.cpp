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


#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "NFD/daemon/fw/forwarder.hpp"

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
      .AddAttribute("PCDtime",
                    "Defines when PCD will be triggered",
                    TimeValue(Seconds(5)), MakeTimeAccessor(&ProactiveProducer::m_pcdTime),
                    MakeTimeChecker());
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
  Simulator::Schedule(Seconds(5), &ns3::ndn::ProactiveProducer::ProactivelyDistributeData, this);
}

void
ProactiveProducer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
ProactiveProducer::ProactivelyDistributeData() {
  NS_LOG_UNCOND("ProactiveProducer::ProactivelyDistributeData()");

  if (!m_active)
    return;

  shared_ptr<Interest> interest = std::make_shared<ndn::Interest>(m_prefix);
  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
  interest->setNonce(rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setInterestLifetime(ndn::time::seconds(300));

  auto data = make_shared<Data>();
  data->setName(m_prefix);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));
  data->setContent(make_shared< ::ndn::Buffer>(100));
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

  GetNode()->GetObject<L3Protocol>()->getForwarder()->getCs().insert(*data, false);
  bool criticalDataInCs = true;
  if (criticalDataInCs) {
    // Turns out, 258 is a local face! 257 is non local and therefore actually
    // emits the data.
    // TODO: Make a new out-bound, non-local face so we dont have to rely on face 257
    // being a present
    shared_ptr<Face> face = GetNode()->GetObject<L3Protocol>()->getFaceById(257);
    face->getScope();
    std::cout << face->getScope() << '\n';
    // Kick-starts the distribution process. Should only be called if the data is definitely within
    // this nodes content store
    GetNode()->GetObject<L3Protocol>()->getForwarder()->startProcessInterest(*face, *interest);
  }
}


} // namespace ndn
} // namespace ns3
