Legends of Equestria Private Server - Yousei
============================================

This is an open source Private Server for the game Legends of Equestria, to play even when the official servers are closed.
The official release is for Windows. The client should work on Linux and Mac too, but you'll need to patch the client yourself.<br/>
<h5><b>All Dowloads:</b></h5>
<a href="https://mega.co.nz/#F!D5xxQCCA!PsCr5W1_5mBYBzIR5uvj9A">Mega</a><br/>
<a href="https://drive.google.com/folderview?id=0B91uVmaVElUhNTlOZlFTUGJhU3c&usp=sharing">Google Drive</a><br/>

<h3>How to play</h3>
- download <a href="https://mega.nz/#!uk5lUJrA!wBK-GCNcPUJx4JQd1PP-HLhOVbeC1PZ3CgqckQgry84">LoE - Yousei v20140416.3.zip</a>
- extract to a directory of your choice
- start loe.exe
- In the game pick a name/password (no need to register first)<br/>
  <b>username and password may only be alphanumeric</b>

<h3>update / patch existing Babscon 2014 client (for Mac & Linux)</h3>
- download <a href="https://mega.nz/#!rtRQibLZ!48Wi-KLkjRVZ9Z73Ktwwu90C3e4KYLjwtRYBXUj9QHs">LoE - Yousei v20140416.3 (patched dll only).zip</a>
- replace the existing dll in /Contents/Data/Managed/ with these
- start the game

<h3>known Bugs</h3>
The private server is still in beta, expect bugs. Patches and pull requests are welcome.<br/>
- client hangs at loading items ... Error or loading Atlas ... Done:<br/>
  Windows: delete data folder under "C:\users\%username%\AppData\Locallow\LoE\Legends of Equestria"<br/>
  OSX: open terminal and enter "rm -r library/caches/unity.LoE.Legends\ of\ Equestria/data"
- others will see you standing on water rather than swimming
- spawning below ground: <br/>
  restart client to fix
- positions not syncing for all with more than 6 players on a map

Some important features are still lacking at the moment:
- no friend or herd system
- Almost no monsters, and none of them can fight.
- Not as many quests as the official servers.
- No 'natural' items to collect (flowers, gems, ...)

<h3>Chat commands</h3>
- /stuck : Reloads and sends you to the spawn. Use it if you can't move.
- :help : Gives a list of chat commands
- :roll : Roll a dice, result between 0 and 99
- :w player msg : Sends a private message to a player
- :players : Lists of the players on the server and where they are
- :me action : States your current action
- :tp location : Teleports your pony to this location (scene)

<h3>List of scenes</h3>
To use with the command ":tp scene_name" command in the game chat.

- PonyVille
- SugarCubeCorner
- GemMines
- Appaloosa
- SweetAppleAcres
- Everfree1
- Zecoras
- Everfree3
- Tartarus
- RaritysBoutique
- Canterlot
- Cottage
- Cloudsdale
- Ponyville Library 1st floor
- Ponyville Library 2nd floor
- minigameLoader
- PM-Lvl1

Notes (as of Babscon 2014 RC2):

- Appaloosa is unfinished and the most of buildings has no textures
- Cottage has no textures nor visible sky box, and the warp points aren't working properly. Walking outside of window will cause you to fall into void
- Tartarus was excluded from the game build. Accessing it will cause game to display loading screen indefinitely. The only way to get out is to delete your character entirely
- PM-Lvl1 is the Pony Muncher game (Pac-Man clone) with unfinished functionality. You cannot escape it by normal means
- If you land in the void after loading a scene, try "/stuck" in the chat
- For bugs add an issue on GitHub
