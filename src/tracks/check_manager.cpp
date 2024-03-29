//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009  Joerg Henrichs
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "tracks/check_manager.hpp"

#include <string>
#include <algorithm>

#include "io/xml_node.hpp"
#include "tracks/ambient_light_sphere.hpp"
#include "tracks/check_cannon.hpp"
#include "tracks/check_lap.hpp"
#include "tracks/check_line.hpp"
#include "tracks/check_structure.hpp"
#include "tracks/track.hpp"

CheckManager *CheckManager::m_check_manager = NULL;

/** Loads all check structure informaiton from the specified xml file.
 */
void CheckManager::load(const XMLNode &node)
{
    for(unsigned int i=0; i<node.getNumNodes(); i++)
    {
        const XMLNode *check_node = node.getNode(i);
        const std::string &type = check_node->getName();
        if(type=="check-line")
        {
            CheckLine *cl = new CheckLine(*check_node, i);
            m_all_checks.push_back(cl);
        }   // checkline
        else if(type=="check-lap")
        {
            m_all_checks.push_back(new CheckLap(*check_node, i));
        }
		else if(type=="cannon")
		{
			m_all_checks.push_back(new CheckCannon(*check_node, i));
		}
        else if(type=="check-sphere")
        {
            AmbientLightSphere *cs = new AmbientLightSphere(*check_node,
                                                            i);
            m_all_checks.push_back(cs);
        }   // checksphere
        else
            printf("Unknown check structure '%s' - ignored.\n", type.c_str());
    }   // for i<node.getNumNodes

    // Now set all 'successors', i.e. check structures that need to get a
    // state change when a check structure is triggered. This can't be
    // done in the CheckStructures easily, since reversing a track changes
    // the direction of the dependencies.
    for(unsigned int i=0; i<node.getNumNodes(); i++)
    {
        const XMLNode *check_node = node.getNode(i);
        std::vector<int> check_structures_to_change_state;

        check_node->get("other-ids", &check_structures_to_change_state);
        // Backwards compatibility to tracks exported with older versions of
        // the track exporter
        if(check_structures_to_change_state.size()==0)
            check_node->get("other-id", &check_structures_to_change_state);
        std::vector<int>::iterator it;
        for(it=check_structures_to_change_state.begin();
            it != check_structures_to_change_state.end(); it++)
        {
            if(QuadGraph::get()->isReverse())
                m_all_checks[*it]->addSuccessor(i);
            else
                m_all_checks[i]->addSuccessor(*it);
        }

    }
}   // load

// ----------------------------------------------------------------------------
/** Private destructor (to make sure it is only called using the static
 *  destroy function). Frees all check structures.
 */
CheckManager::~CheckManager()
{
    for(unsigned int i=0; i<m_all_checks.size(); i++)
    {
        delete m_all_checks[i];
    }
    m_check_manager = NULL;
}   // ~CheckManager

// ----------------------------------------------------------------------------

/** Resets all checks. */
void CheckManager::reset(const Track &track)
{
    std::vector<CheckStructure*>::iterator i;
    for(i=m_all_checks.begin(); i!=m_all_checks.end(); i++)
        (*i)->reset(track);
}   // reset

// ----------------------------------------------------------------------------
/** Updates all animations. Called one per time step.
 *  \param dt Time since last call.
 */
void CheckManager::update(float dt)
{
    std::vector<CheckStructure*>::iterator i;
    for(i=m_all_checks.begin(); i!=m_all_checks.end(); i++)
        (*i)->update(dt);
}   // update

// ----------------------------------------------------------------------------
/** Returns the index of the first check structures that triggers a new
 *  lap to be counted. It aborts if no lap structure is defined.
 */
unsigned int CheckManager::getLapLineIndex() const
{
    // If possible find a proper check-lap structure:
    for (unsigned int i=0; i<getCheckStructureCount(); i++)
    {
        CheckStructure* c = getCheckStructure(i);

        if (dynamic_cast<CheckLap*>(c) != NULL) return i;
    }
    fprintf(stderr, 
           "No check-lap structure found! This can cause incorrect kart\n");
    fprintf(stderr,
           "ranking when crossing the line, but can otherwise be ignored.\n");
    for (unsigned int i=0; i<getCheckStructureCount(); i++)
    {
        if(getCheckStructure(i)->getType()==CheckStructure::CT_NEW_LAP)
            return i;
    }

    fprintf(stderr, "Error, no kind of lap line for track found, aborting.\n");
    exit(-1);
}   // getLapLineIndex

// ----------------------------------------------------------------------------
/** Returns the check line index that is triggered when going from 'from' 
 *  to 'to'. If no check line is triggered, -1 will be returned.
 *  \param from Coordinates to start from.
 *  \param to Coordinates to go to.
 */
int CheckManager::getChecklineTriggering(const Vec3 &from, 
                                         const Vec3 &to) const
{
    for (unsigned int i=0; i<getCheckStructureCount(); i++)
    {
        CheckStructure* c = getCheckStructure(i);

        // FIXME: why is the lapline skipped?
        if (dynamic_cast<CheckLap*>(c) != NULL) continue;

        if (c->isTriggered(from, to, 0 /* kart id */))
            return i;
    }
    return -1;
}   // getChecklineTriggering
