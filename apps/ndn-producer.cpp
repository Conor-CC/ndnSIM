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
#include "ndn-producer.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/global-value.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/mobility-module.h"


#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"
#include "NFD/daemon/fw/forwarder.hpp"
#include "NFD/daemon/table/cs.hpp"

#include <memory>
#include "globals.hpp"

NS_LOG_COMPONENT_DEFINE("ndn.Producer");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(Producer);

TypeId
Producer::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::Producer")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<Producer>()
      .AddAttribute("Prefix", "Prefix, for which producer has the data", StringValue("/"),
                    MakeNameAccessor(&Producer::m_prefix), MakeNameChecker())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&Producer::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&Producer::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&Producer::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&Producer::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&Producer::m_keyLocator), MakeNameChecker())
      .AddAttribute("SimEnd",
                    "Node is aware of the simulation end time",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_simEnd),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxStart",
                    "Where PCD is triggered",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_contentTrigger_x_start),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxEnd",
                    "Where PCD is no longer triggered",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_contentTrigger_x_end),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTlStart",
                    "When PCD can be triggered",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_contentTrigger_l_start),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTlEnd",
                    "When PCD can no longer be triggered",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_contentTrigger_l_end),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("CTxSpeed",
                    "If the content is mobile, update its position from the perspective of the node",
                    UintegerValue(0), MakeUintegerAccessor(&Producer::m_contentTrigger_x_speed),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("IsRSU",
                    "Id This Producer an RSU",
                    BooleanValue(0), MakeBooleanAccessor(&Producer::m_isRSU),
                    MakeBooleanChecker());
      return tid;
}

Producer::Producer()
{
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
Producer::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  FibHelper::AddRoute(GetNode(), m_prefix, m_face, 0);

  UtilityScheduler();
}

void
Producer::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();
}

void
Producer::UtilityScheduler() {
  for (uint32_t i = 1; i < m_simEnd; i++) {
    Simulator::Schedule(Seconds(i), &Producer::UtilityLoop, this);
  }
}

void Producer::UtilityLoop() {
  bool matching = false;
  nfd::cs::Cs& contentStore = GetNode()->GetObject<L3Protocol>()->getForwarder()->getCs();

  nfd::cs::Cs::const_iterator iter;
  for (iter = contentStore.begin(); iter != contentStore.end(); iter++) {
      if (iter->getName().equals("/criticalData/test")) {
          matching = true;
      }
  }

  if (m_isRSU) {
    if (g_rsusCanDist) {
      m_contentDiscovered = true;
      // std::cout << "RSU node_(" << GetNode()->GetId() << ") has sensed content on the RSU net" << '\n';
    }
    else if (matching) {
      // std::cout << "g_rsusCanDist = " << g_rsusCanDist << '\n';
      // std::cout << "RSU node_(" << GetNode()->GetId() << ") has sniffed content" << '\n';
      m_contentDiscovered = true;
      g_rsusCanDist = true;
    }
    else {
      m_contentDiscovered = false;
    }
  }

  if (!m_isRSU) {
    matching ? (m_contentDiscovered = true) : (m_contentDiscovered = false);
    double current_x = GetNode()->GetObject<MobilityModel>()->GetPosition().x;
    double error = 2.0;
    // Check node is within the Content Trigger
    bool isNodeInContentTrigger = (current_x >= (m_contentTrigger_x_start - error)) && (current_x < (m_contentTrigger_x_end + error));
    if (!m_contentDiscovered && isNodeInContentTrigger) {
      //Check if contentTrigger is alive
      double currentTime = Simulator::Now().GetSeconds();
      bool isContentTriggerActive = (m_contentTrigger_l_start <= currentTime && m_contentTrigger_l_end > currentTime);
      isContentTriggerActive ? (m_contentDiscovered = true) : (m_contentDiscovered = false);
    }
  }
  m_contentTrigger_x_start = m_contentTrigger_x_start + m_contentTrigger_x_speed;
  m_contentTrigger_x_end = m_contentTrigger_x_end + m_contentTrigger_x_speed;
}

void
Producer::OnInterest(shared_ptr<const Interest> interest)
{
  if (m_contentDiscovered) {
      // NS_LOG_UNCOND("Producer::OnInterest()");
      App::OnInterest(interest); // tracing inside

      NS_LOG_FUNCTION(this << interest);

      if (!m_active)
        return;

      Name dataName(interest->getName());
      // dataName.append(m_postfix);
      // dataName.appendVersion();

      auto data = make_shared<Data>();
      data->setName(dataName);
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

      NS_LOG_DEBUG("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

      // to create real wire encoding
      data->wireEncode();

      shared_ptr<Interest> interest = std::make_shared<ndn::Interest>(m_prefix);
      Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
      interest->setNonce(rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
      interest->setInterestLifetime(ndn::time::seconds(300));

      m_transmittedDatas(data, this, m_face);
      m_appLink->onReceiveData(*data);
  }
}

} // namespace ndn
} // namespace ns3
