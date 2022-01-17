
- Readme - MPEG Audio Info Tool V2.2 - 2007-04-09

Description:
This tool can display information about MPEG audio files. It supports
MPEG1, MPEG2, MPEG2.5 in all three layers. You can get all the fields
from the MPEG audio frame header in each frame of the file. Additionally you
can check the whole file for inconsistencies.

This tool was written as an example on how to use the classes:
CMPAFile, CMPAFrame, CMPAHeader, CVBRHeader, CMPAStream, CTag and 
CMPAException.

The article MPEG Audio Frame Header on Sourceproject
[https://www.codeproject.com/Articles/8295/MPEG-Audio-Frame-Header] 
provides additional information about these classes and the MPEG audio
frame header in general.

This tool was written with MS Visual C++ 8.0. The MFC library is
statically linked.


Changes from version 2.1:

- added Musicmatch-Tag detection
- better exception handling
- improved design of CMPAStream
- added Drag&Drop functionality to dialog
- fixed memory leaks
- updated solution to Visual Studio 2005
- better performance for finding frames