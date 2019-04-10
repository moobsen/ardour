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

	typedef msm::active_state_switch_before_transition active_state_switch_policy;

#define define_state(State) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: " # State << std::endl; } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) {std::cout << "leaving: " # State << std::endl;} \
	}

#define define_state_flag(State,Flag) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: " # State << std::endl; } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) {std::cout << "leaving: " # State << std::endl;} \
		typedef mpl::vector1<Flag> flag_list; \
	}

	/* FSM states */

	define_state (Stopped);
	define_state (Rolling);

	struct ButlerWait : public msm::front::state<>
	{
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: ButlerWait" << std::endl; } ;
		template <class Event,class FSM> void on_exit (Event const&, FSM&) { std::cout << "leaving: ButlerWait" << std::endl; }


		typedef mpl::vector1<ButlerWaiting> flag_list;
		typedef mpl::vector3<start_transport,stop_transport,butler_required> deferred_events;
	};

	define_state (MasterWait);
	define_state_flag (DeclickOut,DeclickOutInProgress);

	/* Locating is a submachine */

	struct Locating_ : public msm::front::state_machine_def<Locating_> {
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: Locating" << std::endl; } ;
		template <class Event,class FSM> void on_exit (Event const&, FSM&) { std::cout << "leaving: Locating" << std::endl; }

#define define_l_state(State) \
	struct State : public msm::front::state<> \
	{ \
		template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: Locating::" # State << std::endl; } \
		template <class Event,class FSM> void on_exit (Event const&, FSM&) {std::cout << "leaving: Locating::" # State << std::endl;} \
	}
		define_l_state (Stopping);
		define_l_state (DeclickOut);
		define_l_state (ButlerWait);
		define_l_state (LocateInProgress);

		/* actions */

		/* guards */

		typedef Stopping initial_state;
		typedef Locating_ L; // makes transition table easier

		struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+
			_row < Stopping  , butler_required ,  ButlerWait, >,
			_row < Stopping  , butler_done  , DeclickOut >,
			_row < DeclickOut, butler_required, ButlerWait >,
			_row < ButlerWait, butler_done, LocateInProgress >
		> {};
	};
	typedef msm::back::state_machine<Locating_> Locating;

	void flush_event_queue () {}
	void fflush_event_queue () {
		std::cerr << "FEQ @ depth " << pdepth << std::endl;
		if (pdepth == 0) {
			std::cerr << "FEQ " << backend()->get_message_queue_size() << std::endl;
			backend()->execute_queued_events ();
			std::cerr << "after FEQ " << backend()->get_message_queue_size() << std::endl;
		}
	}

	template<typename Event> void enqueue (Event const & e) {
		if (pdepth > 0) {
			backend()->enqueue_event (e);
		} else {
			backend()->process_event (e);
		}
	}

	/* transition actions */

	void start_playback (start_transport const& p);
	void stop_playback (stop_transport const& s);
	void start_locate (locate const& s);
	void butler_completed_transport_work (butler_done const& s);
	void schedule_butler_for_transport_work (butler_required const&);
	void roll_after_locate (locate_done const &);

	/* guards */

	void stop_for_locate (locate const& l)  { _last_locate = l; stop_playback (stop_transport()); }
	bool should_roll_after_locate (locate_done const &);
	bool should_not_roll_after_locate (locate_done const & l)  { return !should_roll_after_locate (l); }

	/* the initial state */
	typedef Stopped initial_state;

	/* transition table */
	typedef TransportFSM T; // makes transition table cleaner

	struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Stopped  , start_transport       , Rolling    , &T::start_playback                     >,
		_row  < Stopped  , stop_transport        , Stopped                                              >,
		// stopped, locate can skip declick out and butlerwait and move directly to Locate2
		a_row < Stopped  , locate                , Locating  , &T::start_locate                       >,
		a_row < Stopped  , butler_required       , ButlerWait , &T::schedule_butler_for_transport_work >,
		a_row < Stopped  , butler_done           , Stopped    , &T::butler_completed_transport_work    >,
		//    +----------+-------------+----------+---------------------+----------------------+
		_row  < Rolling  , start_transport       , Rolling                                    >,
		a_row < Rolling  , stop_transport        , DeclickOut , &T::stop_playback             >,
		a_row < Rolling  , locate                , Locating , &T::stop_for_locate           >,
		_row  < Rolling  , butler_done           , Rolling                                    >,
		//    +----------+-------------+----------+---------------------+----------------------+
		// this is likely: butler is requested while still in Declick out
		a_row < DeclickOut , butler_required , ButlerWait, &T::schedule_butler_for_transport_work >,
		//  this is unlikely: declick too a long time and we got into  ButlerWait first
		_row < ButlerWait , declick_done , ButlerWait >,
		//    +----------+-------------+----------+---------------------+----------------------+
		g_row < Locating , locate_done , Stopped, &T::should_not_roll_after_locate >,
		row   < Locating , locate_done , Rolling, &T::roll_after_locate, &T::should_roll_after_locate >,
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row   < ButlerWait , butler_done , Stopped , &T::butler_completed_transport_work >,
		a_row < ButlerWait , butler_required , ButlerWait , &T::schedule_butler_for_transport_work >,
		//a_row < ButlerWait , locate , ButlerWait , &T::schedule_butler_for_transport_work >,

		// Deferrals

#define defer(start_state,ev) boost::msm::front::Row<start_state, ev, boost::msm::front::none , boost::msm::front::Defer, boost::msm::front::none >

		//defer (Locating1, start_transport),
		//defer (Locating1, stop_transport),
		defer (ButlerWait, start_transport),
		defer (ButlerWait, stop_transport)
		//    +----------+-------------+----------+---------------------+----------------------+
		> {};

	typedef int activate_deferred_events;

	locate _last_locate;
	int pdepth;

	TransportAPI* api;
};

} /* end namespace ARDOUR */

#endif
