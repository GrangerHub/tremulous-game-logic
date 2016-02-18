DEPENDENCIES:
--------------------

 jpeg-8c
 libogg-1.3.1
 libspeex
 minizip
 opus-1.1
 opusfile-0.5
 sdl2
 speexdsp
 zlib

(see below for additional client dependencies)

If you don't have these in your system, also clone the `tremulous-dependencies`
repo and then make symbolic links into a directory called `dep` as in the
following:

    $ git clone <into tremulous-dependencies>
    $ git clone <into tremulous-source>
    $ cd tremulous-source
    $ mkdir dep
    $ cd dep
    $ ln -s ../../tremulous-dependencies/* .

*Note:* libspeex is also provided in the ioQ3 repo, under `code/libspeex`.
Download from git@github.com:ioquake/ioq3.git.

BUILDING THE SERVER:
--------------------

Instead of editing `GNUmakefile`, make a file named `GNUmakefile.local` that
contains macros you want to customize. A sample version has been provided in
`GNUmakefile.local-example`. Check GNUmakefile for possible macros to control
whether to build the client, server, or any other components. You can also
specify whether to use any of the above provided dependencies
(USE_INTERNAL_LIBS=1), or simply look for them in your system
(USE_INTERNAL_LIBS=0). Finer level of control is available with macros for each
dependency (see GNUmakefile).

Then, compile with:

    $ make 

(You can add -j option to compile in parallel)

To compile with debug symbols:

    $ make debug

BUILDING THE CLIENT:
--------------------

The client has additional dependencies that need to be installed system-wide such that they can be found using the `pkg-config` command. These requirement are:

- libcurl-dev(el)
- SDL2-dev(el)
- openal-dev(el)
- libogg-dev(el)

RUNNING
--------------------

To start a local server, run the client and issue:

    /sv_pure 0
    /map atcs

Clone the `tremulous-dev-scripts` repo for more scripts to run the server and
client separately.

