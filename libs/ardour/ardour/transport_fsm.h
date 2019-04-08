#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <boost/weak_ptr.hpp>
#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include "pbd/stacktrace.h"

/* state machine */
namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace ARDOUR
{

struct TransportFSM : public msm::front::state_machine_def<TransportFSM>
{
	/* events to be delivered to the FSM */

	struct locate_done {};
	struct butler_done {};
	struct butler_required {};
	struct declick_done {};
	struct start_transport {};

	struct stop_transport {
		stop_transport (bool ab = false, bool cl = false)
			: abort (ab)
			, clear_state (cl) {}

		bool abort;
		bool clear_state;
	};

	struct locate {
		locate ()
			: target (0)
			, with_roll (false)
			, with_flush (false)
			, with_loop (false)
			, force (false) {}

		locate (samplepos_t target, bool roll, bool flush, bool loop, bool f4c)
			: target (target)
			, with_roll (roll)
			, with_flush (flush)
			, with_loop (loop)
			, force (f4c) {}

		samplepos_t target;
		bool with_roll;
		bool with_flush;
		bool with_loop;
		bool force;
	};

	/* Flags */

	struct ButlerWaiting {};
	struct DeclickOutInProgress {};

	// Pick a back-end
	typedef msm::back::state_machine<TransportFSM> back;

	boost::weak_ptr<back> wp;

	static boost::shared_ptr<back> create(TransportAPI& api) {
		boost::shared_ptr<back> p (new back);
		p->wp = p;
		p->api = &api;
		p->pdepth = 0;
		return p;
	}

	boost::shared_ptr<back> backend() { return wp.lock(); }

	typedef msm::active_state_switch_after_exit active_state_switch_policy;

	/* FSM states */
	struct Stopped : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: Stopped" << std::endl;
		}
		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: Stopped" << std::endl;
		}
	};

	struct Rolling : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: Rolling" << std::endl;
		}

		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: Rolling" << std::endl;
		}
	};

	struct ButlerWait : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: ButlerWait" << std::endl;
		}

		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: ButlerWait" << std::endl;
		}


		typedef mpl::vector1<ButlerWaiting> flag_list;
		typedef mpl::vector3<start_transport,stop_transport,butler_required> deferred_events;
	};

	struct MasterWait : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: MasterWait" << std::endl;
		}

		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: MasterWait" << std::endl;
		}
	};


	struct DeclickOut : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: DeclickOut" << std::endl;
			PBD::stacktrace (std::cerr, 30);
		}

		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: DeclickOut" << std::endl;
			PBD::stacktrace (std::cerr, 30);
		}

		typedef mpl::vector1<DeclickOutInProgress> flag_list;
	};

	struct Locating : public msm::front::state<>
	{
		template <class Event,class FSM> void
		on_entry (Event const&, FSM&)
		{
			std::cout << "entering: Locating" << std::endl;
		}

		template <class Event,class FSM> void
		on_exit (Event const&, FSM&)
		{
			std::cout << "leaving: Locating" << std::endl;
		}
	};

	void flush_event_queue () {
		std::cerr << "FEQ " << backend()->get_message_queue_size() << std::endl;
		wp.lock()->execute_queued_events ();
	}

	template<typename Event> void enqueue (Event const & e) {
		if (pdepth > 0) {
			backend()->enqueue_event (e);
		} else {
			backend()->process_event (e);
		}
	}

	/* transition actions */

	void _start_playback (start_transport const& e) { pdepth++; start_playback (e); pdepth--; flush_event_queue(); }
	void _stop_playback (stop_transport const& e) { pdepth++; stop_playback (e); pdepth--; flush_event_queue(); }
	void _start_locate (locate const& e) { pdepth++; start_locate (e); pdepth--; flush_event_queue(); }
	void _butler_completed_transport_work (butler_done const& e) { pdepth++; butler_completed_transport_work (e); pdepth--; flush_event_queue(); }
	void _schedule_butler_for_transport_work (butler_required const& e) { pdepth++; schedule_butler_for_transport_work (e); pdepth--; flush_event_queue(); }
	void _exit_declick (declick_done const& e) { pdepth++; exit_declick (e); pdepth--; flush_event_queue(); }
	void _locate_phase_two (butler_done const& e) { pdepth++; locate_phase_two (e); pdepth--; flush_event_queue(); }
	void _roll_after_locate (locate_done const& e) { pdepth++; roll_after_locate (e); pdepth--; flush_event_queue(); }

	void start_playback (start_transport const& p);
	void stop_playback (stop_transport const& s);
	void start_locate (locate const& s);
	void butler_completed_transport_work (butler_done const& s);
	void schedule_butler_for_transport_work (butler_required const&);
	void exit_declick (declick_done const&);
	void locate_phase_two (butler_done const&);
	void roll_after_locate (locate_done const &);

	/* guards */

	template<typename Event> bool stopped_to_locate(Event const&) { std::cerr << "Stopped to locate: " << _stopped_to_locate << std::endl; return _stopped_to_locate; }
	template<typename Event> bool stopped_to_stop(Event const&)  { std::cerr << "Stopped to stop: " << !_stopped_to_locate << std::endl; return !_stopped_to_locate; }
	void mark_for_locate (locate const& l)  { std::cerr << "marking declick out for locate\n"; _stopped_to_locate = true; _last_locate = l; _stop_playback (stop_transport()); }
	void mark_for_stop (stop_transport const& s)  { std::cerr << "marking declick out for stop\n"; _stopped_to_locate = false; _stop_playback (s); }
	bool should_roll_after_locate (locate_done const &);
	bool should_not_roll_after_locate (locate_done const & l)  { return !should_roll_after_locate (l); }

	/* the initial state */
	typedef Stopped initial_state;

	/* transition table */
	typedef TransportFSM T; // makes transition table cleaner

	struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Stopped  , start_transport       , Rolling  , &T::_start_playback                         >,
		_row  < Stopped  , stop_transport        , Stopped                                               >,
		a_row < Stopped  , locate      , Locating , &T::mark_for_locate                        >,
		a_row < Stopped  , butler_done , Stopped  , &T::_butler_completed_transport_work        >,
		a_row < Stopped  , butler_required , ButlerWait  , &T::_schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Rolling  , stop_transport        , DeclickOut, &T::mark_for_stop                        >,
		_row  < Rolling  , start_transport       , Rolling                                              >,
		a_row < Rolling  , locate      , DeclickOut , &T::mark_for_locate                     >,
		_row  < Rolling  , butler_done , Rolling                                              >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row   < DeclickOut , declick_done , Locating, &T::_exit_declick, &T::stopped_to_locate >,
		row   < DeclickOut , declick_done , Stopped,  &T::_exit_declick, &T::stopped_to_stop   >,
		a_row < DeclickOut , butler_required , ButlerWait, &T::_schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row   < Locating , locate_done , Rolling, &T::_roll_after_locate, &T::should_roll_after_locate >,
		g_row < Locating , locate_done , Stopped, &T::should_not_roll_after_locate >,
		a_row < Locating , stop_transport        , Stopped  , &T::_stop_playback                         >,
		_row  < Locating , start_transport       , Rolling                                              >,
		_row  < Locating , locate      , Rolling                                              >,
		_row  < Locating , butler_done , Locating                                             >,
		a_row < Locating , butler_required , ButlerWait  , &T::_schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row   < ButlerWait , butler_done , Stopped , &T::_butler_completed_transport_work, &T::stopped_to_stop                     >,
		row   < ButlerWait , butler_done , Locating , &T::_locate_phase_two, &T::stopped_to_locate                     >,
		//a_row < ButlerWait , locate , ButlerWait , &T::_schedule_butler_for_transport_work >,
		a_row < ButlerWait , butler_required , ButlerWait , &T::_schedule_butler_for_transport_work >,
		boost::msm::front::Row < ButlerWait , start_transport , boost::msm::front::none , boost::msm::front::Defer, boost::msm::front::none >,
		boost::msm::front::Row < ButlerWait , stop_transport , boost::msm::front::none , boost::msm::front::Defer, boost::msm::front::none >
		//    +----------+-------------+----------+---------------------+----------------------+
		> {};

	typedef int activate_deferred_events;

	bool _stopped_to_locate;
	locate _last_locate;
	int pdepth;

	TransportAPI* api;
};

} /* end namespace ARDOUR */

#endif
