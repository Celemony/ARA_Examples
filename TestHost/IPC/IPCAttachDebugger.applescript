# This AppleScript will attach Xcode to all instances of ARATestHost.
# It is intended to be used to attach the the remote instance of ARATestHost when using IPC.
# You can either add a breakpoint via the Xcode UI with a Shell Command action "osascript"
# with the path to this file as argument, or use the lldb command line similar to this:
# breakpoint set --name AttachToARATestHost --file CompanionAPIs.cpp --line 580 --auto-continue true
# breakpoint command add AttachToARATestHost --one-liner
#                        "shell osascript ARA_Examples/TestHost/IPC/IPCAttachDebugger.applescript"

do shell script ("pgrep ARATestHost")
set processIDs to every paragraph of the result

tell application "Xcode"
	repeat with processID in processIDs
		tell workspace document 1
			attach to process identifier processID without suspended
		end tell
	end repeat
end tell
