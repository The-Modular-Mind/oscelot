# oscpack -- Open Sound Control packet manipulation library
A simple C++ library for packing and unpacking OSC packets.  
http://www.rossbencina.com/code/oscpack  
Copyright (c) 2004-2013 Ross Bencina <rossb@audiomulch.com>  

Oscpack is simply a set of C++ classes for packing and unpacking OSC packets. 
Oscpack includes a minimal set of UDP networking classes for Windows and POSIX.
The networking classes are sufficient for writing many OSC applications and servers, 
but you are encouraged to use another networking framework if it better suits your needs. 
Oscpack is not an OSC application framework. It doesn't include infrastructure for 
constructing or routing OSC namespaces, just classes for easily constructing, 
sending, receiving and parsing OSC packets. The library should also be easy to use 
for other transport methods (e.g. serial).

The key goals of the oscpack library are:

    - Be a simple and complete implementation of OSC
    - Be portable to a wide variety of platforms
    - Allow easy development of robust OSC applications 
      (for example it should be impossible to crash a server 
      by sending it malformed packets, and difficult to create 
      malformed packets.)

---
If you fix anything or write a set of TCP send/receive classes please consider sending me a patch.  
My email address is rossb@audiomulch.com. Thanks :)

For more information about Open Sound Control, see:  
http://opensoundcontrol.org/

Thanks to Till Bovermann for helping with POSIX networking code and
Mac compatibility, and to Martin Kaltenbrunner and the rest of the
reacTable team for giving me a reason to finish this library. Thanks
to Merlijn Blaauw for reviewing the interfaces. Thanks to Xavier Oliver
for additional help with Linux builds and POSIX implementation details.

Portions developed at the Music Technology Group, Audiovisual Institute, 
University Pompeu Fabra, Barcelona, during my stay as a visiting
researcher, November 2004 - September 2005.

Thanks to Syneme at the University of Calgary for providing financial 
support for the 1.1.0 update, December 2012 - March 2013.

See the file LICENSE for information about distributing and using this code.