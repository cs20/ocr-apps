==============================================================================
                    _____   _____ ____                  _
                   |  __ \ / ____|  _ \                | |
                   | |__) | (___ | |_) | ___ _ __   ___| |__
                   |  _  / \___ \|  _ < / _ \ '_ \ / __| '_ \
                   | | \ \ ____) | |_) |  __/ | | | (__| | | |
                   |_|  \_\_____/|____/ \___|_| |_|\___|_| |_|

                                   Version 9

==============================================================================
Contact Information
==============================================================================

Organization:     Center for Exascale Simulation of Advanced Reactors (CESAR)
                  Argonne National Laboratory

Development Lead: John Tramm   <jtramm@mcs.anl.gov>
                  Amanda Lund  <alund@anl.gov>
                  Ron Rahaman  <rahaman@anl.gov>

==============================================================================
What is RSBench?
==============================================================================

A mini-app to represent the multipole resonance representation lookup
cross section algorithm.

==============================================================================
Quick Start Guide
==============================================================================

Download----------------------------------------------------------------------

	For the most up-to-date version of RSBench, we recommend that you
	download from our git repository. This can be accomplished via
	cloning the repository from the command line, or by downloading a zip
	from our github page. Alternatively, you can download a tar file from
	the CESAR website directly.

	Git Repository Clone:

		Use the following command to clone RSBench to your machine:

		>$ git clone https://github.com/jtramm/RSBench.git

		Once cloned, you can update the code to the newest version
		using the following command (when in the RSBench directory):

		>$ git pull

	Git Zip Download:

		Simply use the "zip download" option on our webpage at:

		https://github.com/jtramm/RSBench

Compilation-------------------------------------------------------------------

	To compile RSBench with default settings, use the following
	command:

	>$ make

Running RSBench---------------------------------------------------------------

Option - 1:
	To run RSBench with default settings, use the following command:

	>$ ./RSBench

Option - 2:
    # Using the unified run framework
    $ source ../../../../apps_env.bash
    $ cd build_and_run
    $ cd serial #cd omp #for OpenMP
    $ run_app ss_small 1500000 #Runs the "small" problem size defined in ../../../parameters.proposed file with 1500000 lookups

	For non-default settings, RSBench supports the following command line
	options:

	Usage: ./RSBench <options>
	Options include:
	  -t <threads>     Number of OpenMP threads to run
	  -s <size>        Size of H-M Benchmark to run (small, large)
	  -l <lookups>     Number of Cross-section (XS) lookups
	  -p <poles>       Average Number of Poles per Nuclide
	  -w <windows>     Average Number of Windows per Nuclide
	  -d               Disables Temperature Dependence (Doppler Broadening)
	Default is equivalent to: -s large -l 10000000 -p 1000 -w 100

	-t <threads>

		Sets the number of OpenMP threads to run. By default, RSBench
		will run with 1 thread per hardware core. If the architecture
		supports hyperthreading, multiple threads will be run per
		core.

		If running in MPI mode, this will be the number of threads
		per MPI rank.

	-s <size>

		Sets the size of the Hoogenboom-Martin reactor model. There
		are two options: 'small' and 'large'. By default,
		the 'large' option is selected.

		The H-M size corresponds to the number of nuclides present
		in the fuel region.  The small version has 34 fuel nuclides,
		whereas the large version has 321 fuel nuclides. This
		significantly slows down the runtime of the program as the
		data structures are much larger, and more lookups are required
		whenever a lookup occurs in a fuel material.  Note that the
		program defaults to "Large" if no specification is made.

	-l <lookups>

		Sets the number of cross-section (XS) lookups to perform. By
		default, this value is set to 5,000,000. Users may want to
		increase this value if they wish to extend the runtime of
		RSBench, perhaps to produce more reliable performance counter
		data - as extending the run will decrease the percentage of
		runtime spent on initialization.

	-p <poles>

		This is the average number of poles per nuclide. Default is
		set to 1000.

	-w <windows>

		This is the number of windows per nuclide. Default is set to
		100.

	-d
		This flag disables Doppler broadening (temperature dependence) in the
	    calculation. Doppler broadening represents a calculation where the
		temperature of the materials in the reactor are considered and
		Doppler broadening is accomplished via the evaluation of the Faddeeva
		function for each pole within a window, which is accomplished by using
	    a fast approximation method for the Faddeeva function. Disabling
	    this feature represents a neutronics calculation done at 0K.

==============================================================================
Debugging, Optimization & Profiling
==============================================================================

There are also a number of switches that can be set in the makefile.

Here is a sample of the control panel at the top of the makefile:

COMPILER  = gnu
OPTIMIZE  = yes
DEBUG     = no
PROFILE   = no
STATUS    = yes

-> Optimization enables the -O3 optimization flag.

-> Debugging enables the -g flag.

-> Profiling enables the -pg flag.

-> STATUS enables calculation completion % printout status text.
   You may want to disable this if doing batch or scripted runs.
   Does not affect performance.

==============================================================================
