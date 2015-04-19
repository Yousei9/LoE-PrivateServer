All changes were made with Red Gate Reflector 7.6.0.808
Feel free to scan those or send them to VirusTotal.

Assembly-CSharp.dll :
Modified to connect to the PrivateServer for login (127.0.0.1:1031)
Modified to connect to the PrivateServer for cutiemarks (127.0.0.1:1031)
Changed livestream in SCC to PonyvilleFM (http://37.59.138.56:7090/pvfm1.ogg)
Changed the version string to v 0.0.20140416-MPS, and made it actually visible
Changed "Connecting to the official Legends of Equestria server" by "Connecting to the Legends of Equestria private server"
Changed the "Register" button to load "https://github.com/tux3/LoE-PrivateServer/"
Change the body and horn size checks when importing ponycodes to only restrict between 0.01 and 100.0
Disable the race-locking checks when importing a ponycode.

LegendsOfEquestria.Data.dll :
Modified to connect to the PrivateServer (127.0.0.1:1031)

LegendsOfEquestria.Shared.dll :
Modified to ignore the chat filters (TextFilter).
Modified to connect to the PrivateServer for chat filters (127.0.0.1:1031)

Lidgren.Network.dll :
Compiled from source the 23/04/2014 after applying this patch with git : https://github.com/jbruening/PNet/blob/master/lidgren_patch.patch
