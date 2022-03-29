**n_stress** - Network stress tester
====================================

Simple program designed to saturate a network connection and validate that the
data sent across the wire is the same as what was sent.

Currently we have a CRC32 as our error detection mechanism, and since Ethernet
frames use [exactly the same check](https://en.wikipedia.org/wiki/Ethernet_frame),
we're really only confirming that the CRC was working. It would be better to
have a different error detecting code so we were cross-checking the CRC and
had different noise-immunities from CRCs.

As a blanket warning - this is a multi-threaded C application - While I tried
to be careful in construction of the program, I definitely built it without
a lot of prior multi-threaded experience. There's no state-sharing across the
threads - we configure everything in a single thread, then launch two, ideally
"stateless" threads which just shovel data. That being said - I'm sure there
are demons somewhere in the code - just be aware that I know that multi-
threaded C is a "Bad Idea™"

Please note that this project could really use a few rounds of cleanup -
* Variable names like "stuff" and "junk" are unhelpful!
* I should probably be going through and validating the behavior when the
  target is localhost (I got some odd crashes - maybe I need to switch to
  `send` and `recv`?)
* Since hitting control+C is the only way to exit, there should probably
  be a `SIGINT` handler which alerts the threads they should shutdown
* There should absolutely be unit tests somewhere in here

It's released under GNU LGPLv3 (you can read more in the license file).

Have fun!

--Kit
