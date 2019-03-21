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

TransportSM::TransportSM (Session& s)
	: SessionHandleRef (s)
{}

/* transition actions */

void
TransportSM::start_playback (TransportStateMachine::play const& p)
{
	std::cout << "player::start_playback" << p._foo << "\n";
	process_event (TransportStateMachine::stop ()); // test if recusion is allowed
}

void
TransportSM::stop_playback (TransportStateMachine::stop const& s)
{
	std::cout << "player::stop_playback\n";
	//_session.realtime_stop (s.abort, s.clear_state);
}
