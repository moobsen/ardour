#ifndef _ardour_transport_fsm_h_
#define _ardour_transport_fsm_h_

#include <boost/msm/back/state_machine.hpp>
#include <boost/msm/front/state_machine_def.hpp>

#include "ardour/session_handle.h"

/* state machine */
namespace msm = boost::msm;
namespace mpl = boost::mpl;

namespace ARDOUR
{

namespace TransportStateMachine
{
	/* events */
	struct play { play(int foo = 1) : _foo (foo) {}; int _foo; };
	struct stop { stop () : abort (false), clear_state (false) {} ; bool abort; bool clear_state; };
	struct locate { locate (samplepos_t target) : _target (target) {} ; samplepos_t _target; };

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

		struct Playing : public msm::front::state<>
		{
			template <class Event,class FSM> void
				on_entry (Event const&, FSM&)
				{
					std::cout << "entering: Playing" << std::endl;
				}

			template <class Event,class FSM> void
				on_exit (Event const&, FSM&)
				{
					std::cout << "leaving: Playing" << std::endl;
				}
		};

		/* transition actions */
		virtual void start_playback (play const& p) {}
		virtual void stop_playback (stop const& s) {}

		/* the initial state */
		typedef Stopped initial_state;

		/* transition table */
		typedef TransportFSM _t; // makes transition table cleaner

		struct transition_table : mpl::vector<
			//      Start     Event         Next      Action               Guard
			//    +---------+-------------+---------+---------------------+----------------------+
			a_row < Stopped , play        , Playing , &_t::start_playback                        >,
			 _row < Stopped , stop        , Stopped                                              >,
			//    +---------+-------------+---------+---------------------+----------------------+
			a_row < Playing , stop        , Stopped , &_t::stop_playback                         >,
			 _row < Playing , play        , Playing                                              >
			//    +---------+-------------+---------+---------------------+----------------------+
			> {};
	};


	typedef msm::back::state_machine<TransportFSM> transport_fsm_t;

} /* end namespace TransportStateMachine */


  /* ideeally we'd use use a typedef here, and avoid wrapping
	 * msm::back::state_machine<>. but that won't work as
	 * forward declaration in session.h
	 *
	 * Any ideas how to use
	  typedef msm::back::state_machine<TransportStateMachine::TransportFSM> TransportSM
	 * instead of the following?
	 */

  /* allow forward declaration */
	class LIBARDOUR_API TransportSM : public TransportStateMachine::transport_fsm_t, public SessionHandleRef
	{
		public:
		TransportSM (ARDOUR::Session& s);

		private:
		/* overloaded TransportFSM actions
		 * TransportSM is a friend of ARDOUR::Session,
		 * transport_fsm_t cannot directly call into _session methods */
		void start_playback (TransportStateMachine::play const& p);
		void stop_playback (TransportStateMachine::stop const& s);
	};

} /* end namespace ARDOUR */

#endif
