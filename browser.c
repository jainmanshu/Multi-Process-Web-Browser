#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_TAB 100
extern int errno;

/*
 * Name:                uri_entered_cb
 * Input arguments:     'entry'-address bar where the url was entered
 *                      'data'-auxiliary data sent along with the event
 * Output arguments:    none
 * Function:            When the user hits the enter after entering the url
 *                      in the address bar, 'activate' event is generated
 *                      for the Widget Entry, for which 'uri_entered_cb'
 *                      callback is called. Controller-tab captures this event
 *                      and sends the browsing request to the ROUTER (parent)
 *                      process.
 */
void uri_entered_cb(GtkWidget* entry, gpointer data) {
	if(data == NULL) {
		return;
	}
	browser_window *b_window = (browser_window *)data;
	// This channel have pipes to communicate with ROUTER.
	comm_channel channel = ((browser_window*)data)->channel;
	// Get the tab index where the URL is to be rendered
	int tab_index = query_tab_id_for_request(entry, data);
	if(tab_index <= 0) {
		fprintf(stderr, "Invalid tab index (%d).", tab_index);
		return;
	}
	// Get the URL.
	char * uri = get_entered_uri(entry);
	// Append your code here
	if (tab_index >= MAX_TAB)
	{
		fprintf(stderr, "cannot send URl %s to tab %d Tab is out of Range .\n", uri,tab_index);
	}	// Send 'which' message to 'which' process?
		//
	else{
		child_req_to_parent creqp;
		creqp.type = NEW_URI_ENTERED;
		creqp.req.uri_req.render_in_tab = tab_index;
		strcpy(creqp.req.uri_req.uri, uri);
		if (write(b_window->channel.child_to_parent_fd[1], &creqp, sizeof(child_req_to_parent)) == -1)
		{
			fprintf(stderr, "Failure to write to controller for tab index %d", tab_index);
		}
	}

}

/*
 * Name:                create_new_tab_cb
 * Input arguments:     'button' - whose click generated this callback
 *                      'data' - auxillary data passed along for handling
 *                      this event.
 * Output arguments:    none
 * Function:            This is the callback function for the 'create_new_tab'
 *                      event which is generated when the user clicks the '+'
 *                      button in the controller-tab. The controller-tab
 *                      redirects the request to the ROUTER (parent) process
 *                      which then creates a new child process for creating
 *                      and managing this new tab.
 */
void create_new_tab_cb(GtkButton *button, gpointer data)
{
	if(data == NULL) {
		return;
	}
	browser_window *b_window = (browser_window *)data;
	// This channel have pipes to communicate with ROUTER.
	comm_channel channel = ((browser_window*)data)->channel;
	int tab_index = ((browser_window*)data)->tab_index;
	// Append your code here
	// Send 'which' message to 'which' process?
	//
	child_req_to_parent newreq;
	newreq.type = CREATE_TAB;
	newreq.req.new_tab_req.tab_index= tab_index;

	if (write(channel.child_to_parent_fd[1], &newreq, sizeof(child_req_to_parent)) == -1)
	{
		fprintf(stderr, "Failure to write to controller for tab index: %d\n", tab_index);
	}
}

/*
 * Name:                url_rendering_process
 * Input arguments:     'tab_index': URL-RENDERING tab index
 *                      'channel': Includes pipes to communctaion with
 *                      Router process
 * Output arguments:    none
 * Function:            This function will make a URL-RENDRERING tab Note.
 *                      You need to use below functions to handle tab event.
 *                      1. process_all_gtk_events();
 *                      2. process_single_gtk_event();
*/
int url_rendering_process(int tab_index, comm_channel *channel) {
	browser_window * b_window = NULL;
	// Create url-rendering window
	create_browser(URL_RENDERING_TAB, tab_index, NULL, NULL, &b_window, channel);
	child_req_to_parent req;
	while (1) {
		usleep(1000);
		// Append your code here
		// Handle 'which' messages?
		
		// read from router process
		if (read(channel->parent_to_child_fd[0], &req, sizeof(child_req_to_parent)) != -1) {
			if (req.type == NEW_URI_ENTERED) {
				if (render_web_page_in_tab(req.req.uri_req.uri, b_window) == -1) {
					fprintf(stderr, "Failed to render new url for tab: %d\n", tab_index);
				}
			}
			else if (req.type == TAB_KILLED) {
				process_all_gtk_events();
				return 0;
			}
		}
		else {
			process_single_gtk_event();
		}
		
	}
}
/*
 * Name:                controller_process
 * Input arguments:     'channel': Includes pipes to communctaion with
 *                      Router process
 * Output arguments:    none
 * Function:            This function will make a CONTROLLER window and
 *                      be blocked until the program terminates.
 */
int controller_process(comm_channel *channel) {
	// Do not need to change code in this function
	close(channel->child_to_parent_fd[0]);
	close(channel->parent_to_child_fd[1]);
	browser_window * b_window = NULL;
	// Create controler window
	create_browser(CONTROLLER_TAB, 0, G_CALLBACK(create_new_tab_cb), G_CALLBACK(uri_entered_cb), &b_window, channel);
	show_browser();
	return 0;
}

/*
 * Name:                router_process
 * Input arguments:     none
 * Output arguments:    none
 * Function:            This function will make a CONTROLLER window and be blocked until the program terminate.
 */
int router_process() {
	comm_channel *channel[MAX_TAB];
	// Append your code here
	int in_use[MAX_TAB];
	int pid, tab_pid, poll_iter;
	int i;
	int status;
	int new_tab;
	int flags;
	bool did_read = false;
	child_req_to_parent message;
	for (i = 0; i < MAX_TAB; i++) {
		in_use[i] = 0;
	}
	// Prepare communication pipes with the CONTROLLER process
	channel[0] = (comm_channel *) malloc(sizeof(comm_channel));
	if (pipe((channel[0])->parent_to_child_fd) < 0)
	{
		perror("Failed to open router to controller pipe\n");
		exit (-1);
	}
	if (pipe(channel[0]->child_to_parent_fd) < 0) {
		perror("Failed to open controller to router pipe\n");
		exit(-1);
	}

	// Fork the CONTROLLER process
	pid = fork();
	if (pid < 0) {
		perror("Could not fork CONTROLLER process\n");
		exit(-1);
	}
	else if (pid == 0) {
		//   call controller_process() in the forked CONTROLLER process
		controller_process(channel[0]);
	}
	else { // router work
		in_use[0] = pid;
		close(channel[0]->parent_to_child_fd[0]); // close read end of parent to child in controller pipes
		close(channel[0]->child_to_parent_fd[1]); // close write end of child to parent in controller pipes
		flags = fcntl(channel[0]->child_to_parent_fd[0], F_GETFL, 0); // get flags for read end
		fcntl(channel[0]->child_to_parent_fd[0], F_SETFL, flags | O_NONBLOCK);

		// Poll child processes' communication channels using non-blocking pipes.
		// While controller is still running
		while (1) {

			for (poll_iter = 0; poll_iter < MAX_TAB; poll_iter++) { /* Polling */
				if (in_use[poll_iter] != 0) {
					if (read((channel[poll_iter])->child_to_parent_fd[0], &message, sizeof(child_req_to_parent)) != -1) {
						did_read = true;
						break;
					}
				}
			} /* Polling */

			if (did_read) {
				did_read = false; // reset read indicator
				//   handle received messages:
				if (message.type == CREATE_TAB) { /* handle create tab */
				//       Prepare communication pipes with a new URL-RENDERING process
					new_tab = 0;
					while (in_use[new_tab] != 0 && new_tab < MAX_TAB)
						new_tab++;
					if (new_tab < MAX_TAB) {
						channel[new_tab] = (comm_channel *) malloc(sizeof(comm_channel));
						// Prepare pipes
						if (pipe((channel[new_tab])->parent_to_child_fd) < 0) {
							perror("Failed to open router to controller pipe\n");
							exit (-1);
						}
						if (pipe(channel[new_tab]->child_to_parent_fd) < 0) {
							perror("Failed to open controller to router pipe\n");
							exit(-1);
						}
						//       Fork the new URL-RENDERING process
						if ((tab_pid = fork()) < 0) {
							perror("Couldn't fork child tab\n");
							exit(-1);
						}
						else if (tab_pid == 0) {
							close((channel[new_tab])->parent_to_child_fd[1]); // Close write end of parent_to_child in child
							close((channel[new_tab])->child_to_parent_fd[0]); // Close read end of child_to_parent in child
							flags = fcntl((channel[new_tab])->parent_to_child_fd[0], F_GETFL, 0); // get flags for read end
							fcntl((channel[new_tab])->parent_to_child_fd[0], F_SETFL, flags | O_NONBLOCK); // make read end non blocking
							if (url_rendering_process(new_tab, channel[new_tab]) == 0) {
								exit(1);
							} //call url rendering process
						}
						else {
							in_use[new_tab] = tab_pid;
							close((channel[new_tab])->parent_to_child_fd[0]); // close read end of parent_to_child in parent
							close((channel[new_tab])->child_to_parent_fd[1]); // close write end of child_to_parent in parent
							flags = fcntl((channel[new_tab])->child_to_parent_fd[0], F_GETFL, 0); // get flags for read end
							fcntl((channel[new_tab])->child_to_parent_fd[0], F_SETFL, flags | O_NONBLOCK); // make read end non blocking
						}


					}
				} /* handle create tab */
	//     NEW_URI_ENTERED:
	//       Send message to the URL-RENDERING process in which the new url is going to be rendered
				else if (message.type == NEW_URI_ENTERED) {
					if (in_use[message.req.uri_req.render_in_tab] == 0) {
						fprintf(stderr, "Attempt to render new URI in tab %d failed: tab doesn't exist\n", message.req.uri_req.render_in_tab);
					}
					else if (write(channel[message.req.uri_req.render_in_tab]->parent_to_child_fd[1], &message, sizeof(child_req_to_parent)) == -1) {
						fprintf(stderr, "Cannot change url for tab number %d\n", message.req.uri_req.render_in_tab);	
					}
				}
	//     TAB_KILLED:
	//       If the killed process is the CONTROLLER process, send messages to kill all URL-RENDERING processes
	//       If the killed process is a URL-RENDERING process, send message to the URL-RENDERING to kill it
				else if (message.type == TAB_KILLED) {
					if (message.req.killed_req.tab_index > 0) {
						if (write(channel[message.req.killed_req.tab_index]->parent_to_child_fd[1], &message, sizeof(child_req_to_parent)) == -1) {
							perror("Cannot send tab killed message\n");
						}
						else {
							waitpid(in_use[message.req.killed_req.tab_index], &status, 0);
							in_use[message.req.killed_req.tab_index] = 0;
							close(channel[message.req.killed_req.tab_index]->parent_to_child_fd[1]);
							close(channel[message.req.killed_req.tab_index]->child_to_parent_fd[0]);
							free(channel[message.req.killed_req.tab_index]);
						}
					}
					else {
						for (i = MAX_TAB - 1; i > -1; i--) {
							if (in_use[i] > 0) {
								message.req.killed_req.tab_index = i;
								if (write(channel[i]->parent_to_child_fd[1], &message, sizeof(child_req_to_parent)) == -1) {
									perror("Cannot send tab killed message\n");
								}
								else {
									waitpid(in_use[i], &status, 0);
									in_use[i] = 0;
									close(channel[i]->parent_to_child_fd[1]);
									close(channel[i]->child_to_parent_fd[0]);
									free(channel[i]);
								}
							}
						}
						exit(0);
					}
				} /* handle tab killed */

			} /* did read */
	//   sleep some time if no message received
	//
			usleep(1000);
		}
	} /* router stuff */
	return 0;
}

int main() {
	return router_process();
}
