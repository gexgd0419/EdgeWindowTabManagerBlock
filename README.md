# EdgeWindowTabManagerBlock

A program that blocks the WindowTabManager feature of Microsoft Edge, when you launch Edge through this program.

This WindowTabManager feature, supposedly, enables Microsoft Edge to show its tabs in Alt-Tab view. However, it has a bug that can slow down Explorer and make Explorer unstable when you have enough Edge tabs open.

My Reddit post about this issue: https://www.reddit.com/r/edge/comments/1090h93/having_too_many_1000_microsoft_edge_tabs_open_can/

There was an experimental flag `#edge-window-tab-manager` that you could disable to disable this feature. However, the flag has been removed. This program is an alternative way to disable this feature and get around its issue.

## How to use

First, download the zip file that matches the processor architecture (x86, x64, or ARM64) of the Microsoft Edge on your system, in the [Releases](https://github.com/gexgd0419/EdgeWindowTabManagerBlock/releases) section. Next, extract it and put the two files in the same directory.

There are two ways to use this program.

### The automatic way: Register as a debugger for Edge (version 0.4)

This program supports registering itself as the IFEO (Image File Execution Options) debugger for Microsoft Edge. This allows the program to intercept every attempt to run Microsoft Edge, so it can patch the Edge processes properly.

This program will ask you if you want it to register as a debugger. You can choose to register, or to use it manually without registration. You can make this dialog show again by holding CTRL when starting the program.

After registration, you can just use Microsoft Edge in the usual way.

Note the following:

- Registering IFEO debuggers requires adminstrator's permission. This will also affect all users on the system. If you have other user accounts, put this program in a folder that every user can access.
- It's recommended to exit Edge before registration.
- Modifying IFEO is seemed as a dangerous action by some antivirus/HIPS software. In fact, this program patches Edge by injecting a third-party DLL into Edge processes, which can already be seen as a dangerous action.
- Do not move, rename, or delete the program after registration. Edge won't be able to run if the registered debugger does not exist.
- If you want to unregister, go to Control Panel > Programs and Features, or Settings > Apps, then choose to "uninstall" EdgeWindowTabManagerBlock. This won't delete the program, this will just unregister it.
- Registering as the IFEO debugger may cause some other debuggers that launch and attach to Edge processes to fail (see issue #8).

### The manual way: Start this program manually, when you want to start Edge

This program works by injecting code to block the creation of the WindowTabManager into the first Edge process, which must happen before the Edge process gets a chance to run its own code. If Edge is already started, it will finish loading the WindowTabManager at startup, and it will be too late to block its creation.

So, to make this program work, you need to run this program instead of the real Edge, and let it start the first Edge process for you, so that this program can have full control over the Edge process.

If you launch Edge by any other method, either through task bar, the Start menu, the desktop icon, or by opening a link when Edge is the default browser, or by opening a file associated with Edge, this program will not work. If there are other Edge processes running already, this program will also not work.

If you choose to register the program as an IFEO debugger, those will be taken care of by the operating system: this program will always be started before Edge. But the manual way is still possible.

## Some technical details

This program uses Microsoft's [Detours](https://github.com/microsoft/Detours) library to hook two Windows API functions: [`RoGetActivationFactory`](https://learn.microsoft.com/windows/win32/api/roapi/nf-roapi-rogetactivationfactory) and [`RoActivateInstance`](https://learn.microsoft.com/windows/win32/api/roapi/nf-roapi-roactivateinstance), which are responsible for creating Windows Runtime objects. The hook functions check if the object being created belongs to class `Windows.UI.Shell.WindowTabManager`, and if it does, fail the creation by returning `E_ACCESSDENIED`. Edge will then think that the system does not have the WindowsTabManager capability, and continue without the feature.
