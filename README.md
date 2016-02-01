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

BUILDING:
--------------------

Instead of editing GNUmakefile, make your own copy named GNUmakefile.local. A
sample version has been provided in GNUmakefile.local-example. Check
GNUmakefile for possible macros to control whether to build the client, server,
or any other components. You can also specify whether to use any of the above
provided dependencies, or simply look for them in your system.

Then, compile with:

    $ make 

(You can add -j option to compile in parallel)

To compile with debug symbols:

    $ make debug

RUNNING
--------------------

To start a local server, run the client and issue:

    /sv_pure 0
    /map atcs

Clone the `tremulous-dev-scripts` repo for more scripts to run the server and
client separately.

