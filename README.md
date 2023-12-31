# EdgeWindowTabManagerBlock

**UPDATE:**

The flag to disable the WindowTabManager in Edge, `#edge-window-tab-manager`, is back on version 120.0.2210.61. (Issue [#3](https://github.com/gexgd0419/EdgeWindowTabManagerBlock/issues/3))

Type `edge://flags` in the address bar. Search for `#edge-window-tab-manager`, then set `Browser tab experiences in Windows` to Disabled.

After that, you can get rid of the problems this program is trying to solve, and you won't need to use this program to launch Edge anymore.

As long as this flag doesn't get removed again.

---

A program that blocks the WindowTabManager feature of Microsoft Edge, when you launch Edge through this program.

This WindowTabManager feature, supposedly, enables Microsoft Edge to show its tabs in Alt-Tab view. However, it has a bug that can slow down Explorer and make Explorer unstable when you have enough Edge tabs open.

My Reddit post about this issue: https://www.reddit.com/r/edge/comments/1090h93/having_too_many_1000_microsoft_edge_tabs_open_can/

This program works by injecting code that prevents object creation of the Windows Runtime class `Windows.UI.Shell.WindowTabManager` into the first Edge process.

To make this program work, you need to make sure that the first Edge process is launched by running this program.

If you launch Edge by any other method, either through task bar, the Start menu, desktop icon, or by opening a link when Edge is the default browser, or by opening a file associated with Edge, this program will not work.

To download the program, check the [Releases](https://github.com/gexgd0419/EdgeWindowTabManagerBlock/releases) section on the right.

If anyone has some idea about how I can make the program work without manually starting the program every time I want to launch Edge, feel free to let me know.
