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
TransportFSM::save_locate_and_stop (TransportFSM::locate const & l)
{
	std::cout << "tfsm::save_locate_and_stop" << std::endl;
	_last_locate = l;
	stop_playback (stop_transport());
}

void
TransportFSM::start_locate (TransportFSM::locate const& l)
{
	std::cout << "tfsm::start_locate\n";
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
TransportFSM::should_roll_after_locate (TransportFSM::exit_from_locating const &)
{
	bool ret = api->should_roll_after_locate ();
	std::cerr << "tfsm::should_roll_after_locate() ? " << ret << std::endl;
	return ret;
}

void
TransportFSM::roll_after_locate (TransportFSM::exit_from_locating const &)
{
	api->start_transport ();
}

