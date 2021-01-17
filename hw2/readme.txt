Execution:
	Use "make" to compile all my source codes.
	When executing "./bidding_system 1 20", sometimes it takes a little
	bit more time to finish running the whole code. Therefore, hope you
	will have enough patience to do so.

Description:
	I finish my program by following the instruction in the
	SP2017_HW2_spec.pdf step by step.
	I don't use any special algorithm in the bonus.

Self-Examination:
	1. (1pt)My bidding_system works fine.
		I use my bidding_system as well as TA’s host and player to
		testtify whether my bidding_system could successfully
		communicate with TA’s host and output correct result.
	2. (0.5pt)My bidding_system schedules host effectively.
		First of all, I give four player_ids to each host once. And
		then implement select to find which host is ready to return
		and make sure not to let available host idle.
	3. (0.5pt)My bidding_system executes host correctly.
		I only fork host host_num times, and assign new competition to
		any of them which is available.
	4. (2pt)My host works fine.
		I use my host as well as TA’s bidding_system and player to
		testify whether my host could successfully communicate with
		TA's bidding_system and player, and output correct result.
	5. (1pt)My player works fine.
		I use my player as well as TA’s bidding_system and host to
		testify whether my player could successfully communicate
		between TA’s and host, and output correct result.
	6. (1.5 pt)Completeness.
		I can successfully produce correct result with all my
		bidding_system, host and player.
	7. (0.5pt)Produce executable files successfully.
		My Makefile can generate bidding_system, host and player.
