All changes were made with reflexil.for.ILSpy.2.0-preview3

Assembly-CSharp.dll :
Modified to connect to the PrivateServer for login (yousei.qc.to:1031)
Modified to connect to the PrivateServer for cutiemarks (yousei.qc.to:1031)
Changed livestream in SCC to PonyvilleFM (http://37.59.138.56:7090/pvfm1.ogg)
Changed the version string to v 0.0.20140416-MPS, and made it actually visible
Changed "Connecting to the official Legends of Equestria server" by "Connecting to the Legends of Equestria private server"
Changed the "Register" button to load "https://github.com/tux3/LoE-PrivateServer/"
Change the body and horn size checks when importing ponycodes to only restrict between 0.01 and 100.0
Disable the race-locking checks when importing a ponycode.

LegendsOfEquestria.Data.dll :
Modified to connect to the PrivateServer (yousei.qc.to:1031)

LegendsOfEquestria.Shared.dll :
Modified to ignore the chat filters (TextFilter).
Modified to connect to the PrivateServer for chat filters (yousei.qc.to:1031)

Lidgren.Network.dll :
Compiled from source the 23/04/2014 after applying this patch with git : https://github.com/jbruening/PNet/blob/master/lidgren_patch.patch
