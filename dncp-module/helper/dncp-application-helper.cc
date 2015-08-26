/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "dncp-application-helper.h"
#include "ns3/dncp-application.h"
#include "ns3/names.h"

namespace ns3 {

DncpApplicationHelper::DncpApplicationHelper ()
{
  m_factory.SetTypeId (DncpApplication::GetTypeId ());
}


ApplicationContainer
DncpApplicationHelper::Install (Ptr<Node> node) const
{
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
DncpApplicationHelper::Install (std::string nodeName) const
{
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return ApplicationContainer (InstallPriv (node));
}

ApplicationContainer
DncpApplicationHelper::Install (NodeContainer c) const
{
  ApplicationContainer apps;
  for (NodeContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      apps.Add (InstallPriv (*i));
    }
  return apps;
}

Ptr<Application>
DncpApplicationHelper::InstallPriv (Ptr<Node> node) const
{
  Ptr<Application> app = m_factory.Create<DncpApplication> ();
  node->AddApplication (app);
  return app;
}

/* ... */


}

