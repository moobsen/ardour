#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>

/* state machine */
namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace ARDOUR
{

namespace TransportStateMachine
{

/* events to be delivered to the FSM */

struct start {
	start () {}
};

struct stop {
	stop (bool ab = false, bool cl = false)
		: abort (ab)
		, clear_state (cl) {}

	bool abort;
	bool clear_state;
};

struct locate {
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
				}

			template <class Event,class FSM> void
				on_exit (Event const&, FSM&)
				{
					std::cout << "leaving: DeclickOut" << std::endl;
				}
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

		/* the initial state */
		typedef Stopped initial_state;

		/* transition table */
		typedef TransportFSM T; // makes transition table cleaner

		struct transition_table : mpl::vector<
			//      Start     Event         Next      Action               Guard
			//    +----------+-------------+----------+---------------------+----------------------+
			a_row < Stopped  , start       , Rolling  , &T::start_playback                         >,
			 _row < Stopped  , stop        , Stopped                                              >,
			 _row < Stopped  , locate      , Stopped                                              >,
			//    +----------+-------------+----------+---------------------+----------------------+
			a_row < Rolling  , stop        , Stopped  , &T::stop_playback                          >,
			 _row < Rolling  , start       , Rolling                                              >,
			a_row < Rolling  , locate      , Locating , &T::start_locate                          >,
			//    +----------+-------------+----------+---------------------+----------------------+
			a_row < Locating , stop        , Stopped  , &T::stop_playback                         >,
			 _row < Locating , start       , Rolling                                              >,
			 _row < Locating , locate      , Rolling                                              >
			//    +----------+-------------+----------+---------------------+----------------------+
			> {};
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
	void start_playback (TransportStateMachine::start const& p);
	void stop_playback (TransportStateMachine::stop const& s);
	void start_locate (TransportStateMachine::locate const& s);
};

} /* end namespace ARDOUR */

#endif
