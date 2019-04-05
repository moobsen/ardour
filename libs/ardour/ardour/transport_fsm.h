#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>
#include <boost/msm/front/functor_row.hpp>

#include "pbd/stacktrace.h"

/* state machine */
namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace ARDOUR
{

namespace TransportStateMachine
{

/* events to be delivered to the FSM */

struct locate_done {};
struct butler_done {};
struct butler_required {};
struct declick_done {};
struct start {};

struct stop {
	stop (bool ab = false, bool cl = false)
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

/* front-end: define the FSM structure  */
struct TransportFSM : public msm::front::state_machine_def<TransportFSM>
{
	virtual ~TransportFSM () {}

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
		typedef mpl::vector3<start,stop,butler_required> deferred_events;
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

	/* transition actions */

	virtual void start_playback (start const& p) {}
	virtual void stop_playback (stop const& s) {}
	virtual void start_locate (locate const& s) {}
	virtual void butler_completed_transport_work (butler_done const& s) {}
	virtual void schedule_butler_for_transport_work (butler_required const&) {}
	virtual void exit_declick (declick_done const&) {}
	virtual void locate_phase_two (butler_done const&) {}
	virtual void roll_after_locate (locate_done const &) {}

	/* guards */

	template<typename Event> bool stopped_to_locate(Event const&) { std::cerr << "Stopped to locate: " << _stopped_to_locate << std::endl; return _stopped_to_locate; }
	template<typename Event> bool stopped_to_stop(Event const&)  { std::cerr << "Stopped to stop: " << !_stopped_to_locate << std::endl; return !_stopped_to_locate; }
	void mark_for_locate (locate const& l)  { std::cerr << "marking declick out for locate\n"; _stopped_to_locate = true; _last_locate = l; stop_playback (stop()); }
	void mark_for_stop (stop const& s)  { std::cerr << "marking declick out for stop\n"; _stopped_to_locate = false; stop_playback (s); }
	virtual bool should_roll_after_locate (locate_done const &)  { return false; }
	bool should_not_roll_after_locate (locate_done const & l)  { return !should_roll_after_locate (l); }

	/* the initial state */
	typedef Stopped initial_state;

	/* transition table */
	typedef TransportFSM T; // makes transition table cleaner

	struct transition_table : mpl::vector<
		//      Start     Event         Next      Action               Guard
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Stopped  , start       , Rolling  , &T::start_playback                         >,
		_row  < Stopped  , stop        , Stopped                                               >,
		a_row < Stopped  , locate      , Locating , &T::mark_for_locate                        >,
		a_row < Stopped  , butler_done , Stopped  , &T::butler_completed_transport_work        >,
		a_row < Stopped  , butler_required , ButlerWait  , &T::schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		a_row < Rolling  , stop        , DeclickOut, &T::mark_for_stop                        >,
		_row  < Rolling  , start       , Rolling                                              >,
		a_row < Rolling  , locate      , DeclickOut , &T::mark_for_locate                     >,
		_row  < Rolling  , butler_done , Rolling                                              >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row   < DeclickOut , declick_done , Locating, &T::exit_declick, &T::stopped_to_locate >,
		row   < DeclickOut , declick_done , Stopped,  &T::exit_declick, &T::stopped_to_stop   >,
		a_row < DeclickOut , butler_required , ButlerWait, &T::schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row   < Locating , locate_done , Rolling, &T::roll_after_locate, &T::should_roll_after_locate >,
		g_row < Locating , locate_done , Stopped, &T::should_not_roll_after_locate >,
		a_row < Locating , stop        , Stopped  , &T::stop_playback                         >,
		_row  < Locating , start       , Rolling                                              >,
		_row  < Locating , locate      , Rolling                                              >,
		_row  < Locating , butler_done , Locating                                             >,
		a_row < Locating , butler_required , ButlerWait  , &T::schedule_butler_for_transport_work >,
		//    +----------+-------------+----------+---------------------+----------------------+
		row < ButlerWait , butler_done , Stopped , &T::butler_completed_transport_work, &T::stopped_to_stop                     >,
		row < ButlerWait , butler_done , Locating , &T::locate_phase_two, &T::stopped_to_locate                     >,
		boost::msm::front::Row < ButlerWait , start , boost::msm::front::none , boost::msm::front::Defer, boost::msm::front::none >,
		boost::msm::front::Row < ButlerWait , stop , boost::msm::front::none , boost::msm::front::Defer, boost::msm::front::none >,
		a_row < ButlerWait , butler_required , ButlerWait , &T::schedule_butler_for_transport_work >
		//    +----------+-------------+----------+---------------------+----------------------+
		> {};

	typedef int activate_deferred_events;

	bool _stopped_to_locate;
	locate _last_locate;
};

typedef msm::back::state_machine<TransportFSM> transport_fsm_t;

} /* end namespace TransportStateMachine */

/* ideeally we'd use use a typedef here, and avoid wrapping
 * msm::back::state_machine<>. but that won't work as
 * forward declaration in session.h
 *
 * Any ideas how to use
 * typedef msm::back::state_machine<TransportStateMachine::TransportFSM> TransportSM
 * instead of the following?
 */

class TransportAPI;

class LIBARDOUR_API TransportSM : public TransportStateMachine::transport_fsm_t
{
  public:
	TransportSM (ARDOUR::TransportAPI& api);

  private:
	TransportAPI& api;

	/* overloaded TransportFSM actions
	 * TransportSM is a friend of ARDOUR::Session,
	 * transport_fsm_t cannot directly call into _session methods
	 */
	void start_playback (TransportStateMachine::start const&);
	void stop_playback (TransportStateMachine::stop const&);
	void start_locate (TransportStateMachine::locate const&);
	void butler_completed_transport_work (TransportStateMachine::butler_done const&);
	void schedule_butler_for_transport_work (TransportStateMachine::butler_required const&);
	void exit_declick (TransportStateMachine::declick_done const&);
	void roll_after_locate (TransportStateMachine::locate_done const &);
	void locate_phase_two (TransportStateMachine::butler_done const&);

	/* virtual guards (which require TransportAPI access) */
	bool should_roll_after_locate (TransportStateMachine::locate_done const &);
};

} /* end namespace ARDOUR */

#endif
