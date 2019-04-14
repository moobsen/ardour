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

class TransportAPI;

struct TransportFSM : public msm::front::state_machine_def<TransportFSM>
{
	/* events to be delivered to the FSM */

	struct locate_done {};
	struct locate_complete {};
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

	typedef msm::active_state_switch_before_transition active_state_switch_policy;

	/* transition actions */

	void start_playback (start_transport const& p);
	void stop_playback (stop_transport const& s);
	void start_locate (locate const& s);
	void butler_completed_transport_work (butler_done const& s);
	void schedule_butler_for_transport_work (butler_required const&);
	void roll_after_locate (locate_complete const &);

	/* guards */

	bool should_roll_after_locate (locate_complete const &);
	bool should_not_roll_after_locate (locate_complete const &)  { return !should_roll_after_locate (locate_complete()); }

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

		define_l_state (ButlerWaitOnStop);
		define_l_state (ButlerWaitOnLocate);

		struct LocateInProgress : public msm::front::state<>, msm::front::explicit_entry<0> {
			template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: Locating::LocateInProgress" << std::endl; }
			template <class Event,class FSM> void on_exit (Event const&, FSM&) {std::cout << "leaving: Locating::LocateInProgress" << std::endl;}
		};

		struct Stopping : public msm::front::state<>, msm::front::explicit_entry<0> {
			template <class Event,class FSM> void on_entry (Event const&, FSM&) { std::cout << "entering: Locating::Stopping" << std::endl; }
			template <class Event,class FSM> void on_exit (Event const&, FSM&) {std::cout << "leaving: Locating::Stopping" << std::endl;}
		};

		/* When we leave this submachine, we will deliver a
		 * "locate_complete" event to the outer state machine.
		 */

		struct Exit : public msm::front::exit_pseudo_state<locate_complete> {
			template <class Event,class FSM>
			void on_entry(Event const&,FSM& ) {std::cout << "entering: Locating::Exit" << std::endl;}
			template <class Event,class FSM>
			void on_exit(Event const&,FSM& ) {std::cout << "leaving: Locating::Exit" << std::endl;}
		};

		typedef Stopping initial_state;

		/* something to store a locate request in while we stop/declick etc. */

		locate _last_locate;

		/* constructor (with argument, because we need to refer to the outer FSM) */

		typedef msm::back::state_machine<TransportFSM> OuterFSM;
		boost::weak_ptr<OuterFSM> _outer;

		Locating_ (boost::shared_ptr<OuterFSM> o) : _outer (o) { std::cerr << "outer constructed Locating with " << _outer.lock() << std::endl; }
		Locating_ (const Locating_& other) : _outer (other._outer) { std::cerr << "Copy construction Locating_ from " << &other << " outer = " << _outer.lock() << std::endl; }

		/* this will be called, unfortunately, when the outerFSM
		   constructs its default states. Then we immediately replace the
		   Locating state with one that has a valid outerFSM reference.
		*/

		Locating_ () { std::cerr << "default constructed Locating (null _outer)\n"; }

		/* functors. These are used to call methods of the outer FSM
		 * (There seems to be no other way to do this)
		 */

		struct _schedule_butler_for_transport_work {
			template <class Fsm,class Evt,class SourceState,class TargetState>
			void operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& ) {
				std::cerr << "locating::schedule_butler_for_transport_work" << std::endl;
				fsm._outer.lock()->schedule_butler_for_transport_work (butler_required());
			}
		};

		struct _start_locate {
			template <class Fsm,class Evt,class SourceState,class TargetState>
			void operator()(Evt const&, Fsm& fsm, SourceState&,TargetState& ) {
				std::cerr << "locating::_start_locate" << std::endl;
				fsm._outer.lock()->start_locate (fsm._last_locate);
			}
		};

		struct _save_locate_and_stop {
			template <class Fsm,class SourceState,class TargetState>
			void operator()(locate const& l, Fsm& fsm, SourceState&,TargetState& ) {
				std::cerr << "locating::_save_locate_and_stop" << std::endl;
				fsm._last_locate = l;
				fsm._outer.lock()->stop_playback (stop_transport());
			}
		};

		/* transition table */

		struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+

			/* When we enter this submachine from a stopped state,
			 * the locate event is forwarded to us, and we take
			 * action.
			 */

			boost::msm::front::Row < LocateInProgress, locate,  LocateInProgress, _start_locate, boost::msm::front::none >,

			/* we enter this sub-machine in Stopping, with the
			*/
			boost::msm::front::Row < Stopping , locate , Stopping , _save_locate_and_stop, boost::msm::front::none >,
			boost::msm::front::Row < Stopping , locate , ButlerWaitOnStop , _schedule_butler_for_transport_work, boost::msm::front::none >,

			/* Once the butler is done stopping the transport, we
			   are now in a DeclickOut state, waiting for everything
			   to finish declicking.
			*/
			_row < ButlerWaitOnStop  , butler_done  , DeclickOut >,

			/* Once Declick is finished we can actually start the
			   locate. This will need the butler again.
			*/
			boost::msm::front::Row < DeclickOut, declick_done, LocateInProgress , _start_locate, boost::msm::front::none >,

			/* call for the butler when asked
			 */

			boost::msm::front::Row < LocateInProgress  , butler_required ,  ButlerWaitOnLocate , _schedule_butler_for_transport_work, boost::msm::front::none >,

			/* once the butler is done with the non-realtime part
			   of the locate, we are done and can exit this submachine
			*/
			_row < ButlerWaitOnLocate, butler_done, Exit >,

			/* it is possible that no work is required to complete
			 * the locate, in which case we will get this
			 * transition.
			 */

			_row < LocateInProgress, locate_done, Exit >
		> {};

	};

	typedef msm::back::state_machine<Locating_> Locating;

	// Pick a back-end
	typedef msm::back::state_machine<TransportFSM> back;

	boost::weak_ptr<back> wp;

	static boost::shared_ptr<back> create(TransportAPI& api) {

		boost::shared_ptr<back> p (new back ());

		Locating l (p);

		p->set_states (msm::back::states_ << l);

		p->wp = p;
		p->api = &api;
		p->pdepth = 0;
		return p;
	}

	boost::shared_ptr<back> backend() { return wp.lock(); }

	template<typename Event> void enqueue (Event const & e) {
		if (pdepth > 0) {
			backend()->enqueue_event (e);
		} else {
			backend()->process_event (e);
		}
	}

	/* the initial state */
	typedef Stopped initial_state;

	/* transition table */
	typedef TransportFSM T; // makes transition table cleaner

	struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Stopped  , start_transport       , Rolling    , &T::start_playback                     >,
		_row  < Stopped  , stop_transport        , Stopped                                              >,
		a_row < Stopped  , butler_required       , ButlerWait , &T::schedule_butler_for_transport_work >,
		a_row < Stopped  , butler_done           , Stopped    , &T::butler_completed_transport_work    >,
		//    +----------+-------------+----------+---------------------+----------------------+
		_row  < Stopped  , locate                , Locating::direct<Locating_::LocateInProgress> >,
		_row  < Rolling  , locate                , Locating::direct<Locating_::Stopping>  >,
		//    +----------+-------------+----------+---------------------+----------------------+
		_row  < Rolling  , start_transport       , Rolling                                    >,
		a_row < Rolling  , stop_transport        , DeclickOut , &T::stop_playback             >,
		_row  < Rolling  , butler_done           , Rolling                                    >,
		//    +----------+-------------+----------+---------------------+----------------------+
		// this is likely: butler is requested while still in Declick out
		a_row < DeclickOut , butler_required , ButlerWait, &T::schedule_butler_for_transport_work >,
		//  this is unlikely: declick too a long time and we got into  ButlerWait first
		_row < ButlerWait , declick_done , ButlerWait >,
		//    +----------+-------------+----------+---------------------+----------------------+
		g_row < Locating::exit_pt<Locating::Exit> , locate_complete , Stopped, &T::should_not_roll_after_locate >,
		row   < Locating::exit_pt<Locating::Exit> , locate_complete , Rolling, &T::roll_after_locate, &T::should_roll_after_locate >,
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

	int pdepth;

	TransportAPI* api;
};

} /* end namespace ARDOUR */

#endif
