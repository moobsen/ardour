/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "ardour/session.h"
#include "ardour/transport_fsm.h"

using namespace ARDOUR;

/* transition actions */

void
TransportFSM::start_playback (TransportFSM::start_transport const& p)
{
	std::cout << "tfsm::start_playback" << std::endl;
	api->start_transport();
}

void
TransportFSM::stop_playback (TransportFSM::stop_transport const& s)
{
	std::cout << "tfsm::stop_playback" << std::endl;
	api->stop_transport (s.abort, s.clear_state);
}

void
TransportFSM::exit_declick (TransportFSM::declick_done const&)
{
	std::cout << "tfsm::exit_declick" << std::endl;
	if (_stopped_to_locate) {
		api->locate (_last_locate.target, _last_locate.with_roll, _last_locate.with_flush, _last_locate.with_loop, _last_locate.force);
	} else {
		api->stop_transport (false, false);
	}
}

void
TransportFSM::start_locate (TransportFSM::locate const& l)
{
	std::cout << "tfsm::start_locate\n";
	_stopped_to_locate = true;
	api->locate (l.target, l.with_roll, l.with_flush, l.with_loop, l.force);
}

void
TransportFSM::butler_completed_transport_work (TransportFSM::butler_done const&)
{
	std::cout << "tfsm::butler_completed_transport_work\n";
	api->butler_completed_transport_work ();
}

void
TransportFSM::schedule_butler_for_transport_work (TransportFSM::butler_required const&)
{
	api->schedule_butler_for_transport_work ();
}

bool
TransportFSM::should_roll_after_locate (TransportFSM::locate_done const &)
{
	return api->should_roll_after_locate ();
}

void
TransportFSM::roll_after_locate (TransportFSM::locate_done const &)
{
	api->start_transport ();
}

void
TransportFSM::locate_phase_two (TransportFSM::butler_done const &)
{
	//assert (_stopped_to_locate);
	std::cerr << "Locate Phase 2: stl = " << _stopped_to_locate << std::endl;
	api->butler_completed_transport_work ();
	api->locate (_last_locate.target, _last_locate.with_roll, _last_locate.with_flush, _last_locate.with_loop, _last_locate.force);
}
