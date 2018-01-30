# Multi-Process-Web-browser

## Purpose
The purpose of this Project is to correctly implement/debug/execute a multi-process, event driven web browser.  To this end the program utilizes pipes, fork(), wait(), IPC … specifically pipes and the open source GTK library. The main code lies in [browser.c](browser.c) that utilizes GTK library and grapics provided in wrapper.c and wrapper.h file. This multi process browser allow you to open webpages using multiple windows/tabs, which can be controlled from a controller window/tab. This web browser uses an isolated process for each tab so that browsing on a given tab is not interrupted in the event that another tab crashes.   

### How to compile this Project
This project comes with a make file

It's recommended that you use the make file to build the program, the following command is used to compile the program from the terminal prompt:

``` $ make all ```

This command will execute the first target in the make file.

After compilation, run the executble file using this command from terminal

``` $ ./browser ```

 This will open a rendered browser, where you can browse multiple pages at a time without need to worry about crashing the web browser.

 #### Overview of the browser program

 When executed from the terminal, the program first evokes the router process. The router process is responsible for forking the controller process, and waiting for process request.  Next, once created, the controller process is responsible for rendering the main (controller) browser window.

Through this controller window the user can create a new tab (child process)...by pressing the new tab button.
The user can also enter a URL for browsing in the Controller URL textbox and specify the tab number corresponding to the tab within which the desired page should be rendered.

The controller sends tab request to the router, and the router forks new processes for URL rendering.
When a tab is closed, the tab sends a notification to the router, and the router deals with any clean-up.
These requests and notifications are implemented using pipes.
