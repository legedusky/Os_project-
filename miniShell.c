
#include <stdio.h>	
#include <stdlib.h>     
#include <string.h>    
#include <unistd.h>    
#include <sys/wait.h>
#include <fcntl.h>	
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h> 


#define BUILTIN_COMMANDS 6	// Number of builtin commands defined
#define MAX_BG_PROCS 100
pid_t bg_procs[MAX_BG_PROCS];
int bg_count = 0;

#define MAX_STATIONS 10
#define MAX_NAME_LENGTH 50
#define MAX_URL_LENGTH 200

typedef struct {
    char name[MAX_NAME_LENGTH];
    char url[MAX_URL_LENGTH];
} RadioStation;

RadioStation stations[MAX_STATIONS] = {
    {"Lofi Girl", "http://stream.lofi.sg:8080/stream"},
    {"Chillhop", "http://stream.zeno.fm/fyn8eh3h5f8uv"},
    {"Synthwave", "http://stream.synthwave.pl/synthwave"},
    {"Jazz", "http://jazz-wr04.ice.infomaniak.ch/jazz-wr04-128.mp3"},
    {"Classical", "http://stream.klassikradio.de/klassikradio.mp3"},
	{"Mirchi", "https://onlineradios.in/#mirchi-fm"},
    {"", ""} // Terminator
};

pid_t radio_pid = -1;


void sendfile_server(const char *filename);

// Function prototype
void sigchld_handler(int sig) {
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Remove from bg_procs array
        for (int i = 0; i < bg_count; i++) {
            if (bg_procs[i] == pid) {
                printf("[%d] Done\t%d\n", i+1, pid);
                bg_procs[i] = bg_procs[--bg_count];
                break;
            }
        }
    }
}
/*
 * Environment variables
 */
char PWD[1024];		// Present Working Directory
char PATH[1024];	// Path to find the commands

/*
 * Built-in command names
 */
char * builtin[] = {"change_dir", "exit", "help", "pwd", "echo", "radio"};

/*
 * Built-in command functions
 */

/*
 * Function:  shell_cd
 * -------------------
 *  changes current working directory
 *
 * args: arguments to the cd command, will consider only the first argument after the command name
 */
int shell_change_dir(char ** args){
	if (args[1] == NULL){
		fprintf(stderr, "minsh: one argument required\n");
	}
	else if (chdir(args[1]) < 0){
		perror("minsh");
	}
	getcwd(PWD, sizeof(PWD));	// Update present working directory
	return 1;
}

/*
 * Function:  shell_exit
 * ---------------------
 *  exits from the shell
 *
 * return: status 0 to indicate termination
 */
int shell_exit(char ** args){
	return 0;
}

/*
 * Function:  shell_help
 * ---------------------
 *  prints a small description
 *
 * return: status 1 to indicate successful termination
 */
int shell_help(char ** args){
	printf("\nCommands implemented: ");
	printf("\n\t- help");
	printf("\n\t- exit");
	printf("\n\t- cd dir");
	printf("\n\t- pwd");
	printf("\n\t- echo [string to echo]");
	printf("\n\t- clear");
	printf("\n\t- ls [-ail] [dir1 dir2 ...]");
	printf("\n\t- cp source target (or) cp file1 [file2 ...] dir");
	printf("\n\t- mv source target (or) mv file1 [file2 ...] dir");
	printf("\n\t- rm file1 [file2 ...]");
	printf("\n\t- mkdir dir1 [dir2 ...]");
	printf("\n\t- rmdir dir1 [dir2 ...]");
	printf("\n\t- ln [-s] source target");
	printf("\n\t- cat [file1 file2 ...]");
	printf("\n\t- finddupes [folder] (Find duplicate files in folder)");
	printf("\n\n");
	printf("Other features : ");
	printf("\n\t* Input, Output and Error Redirection (<, <<, >, >>, 2>, 2>> respectively)  : ");
	printf("\n\t* Example: ls -i >> outfile 2> errfile [Space mandatory around redirection operators!]");
	printf("\n\n");
	return 1;
}


// int shell_sendfile(char **args) {
//     if (!args[1]) {
//         fprintf(stderr, "Usage: sendfile <filename>\n");
//         return 1;
//     }
    
//     pid_t pid = fork();
//     if (pid == 0) {
//         // Child process runs the server
//         sendfile_server(args[1]);
//         exit(0);
//     } else if (pid < 0) {
//         perror("fork");
//         return 1;
//     }
    
//     // Parent continues
//     printf("[%d] File server started (PID: %d)\n", bg_count+1, pid);
//     bg_procs[bg_count++] = pid;
//     return 1;
// }


/*
 * Function:  shell_pwd
 * --------------------
 *  prints the present working directory
 *
 * return: status 1 to indicate successful termination
 */
int shell_pwd(char ** args){
	printf("%s\n", PWD);
	return 1;
}

/*
 * Function:  shell_echo
 * ---------------------
 *  displays the string provided
 * 
 * return: status 1 to indicate successful termination
 */
int shell_echo(char ** args){
	int i = 1;
	while (1){
		// End of arguments
		if (args[i] == NULL){
			break;
		}
		printf("%s ", args[i]);
		i++;
	}
	printf("\n");
}
void stop_radio() {
    if (radio_pid != -1) {
        // Kill the entire process group
        kill(-radio_pid, SIGTERM);
        
        // Wait briefly then force kill if needed
        usleep(50000);
        if (waitpid(radio_pid, NULL, WNOHANG) == 0) {
            kill(-radio_pid, SIGKILL);
            waitpid(radio_pid, NULL, 0);
        }
        
        // Clean up any remaining processes
        system("pkill -9 mpg123 2>/dev/null");
        radio_pid = -1;
        printf("Radio stopped.\n");
    }
}

void play_radio(int station_num) {
    // Stop any existing playback
    stop_radio();
    
    int station_index = station_num - 1;
    if (station_index < 0 || station_index >= MAX_STATIONS || 
        stations[station_index].name[0] == '\0') {
        printf("Invalid station number!\n");
        return;
    }
    
    printf("Tuning to %s...\n", stations[station_index].name);
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - create new session
        setsid();
        
        // Redirect all output
        int null_fd = open("/dev/null", O_WRONLY);
        dup2(null_fd, STDIN_FILENO);
        dup2(null_fd, STDOUT_FILENO);
        dup2(null_fd, STDERR_FILENO);
        close(null_fd);
        
        execlp("mpg123", "mpg123", "-q", stations[station_index].url, NULL);
        exit(1);
    } else if (pid > 0) {
        radio_pid = pid;
        printf("Now playing: %s (PID: %d)\n", stations[station_index].name, pid);
        printf("Use 'radio stop' to stop playback.\n");
        
        // Add to background processes if you want to track it
        if (bg_count < MAX_BG_PROCS) {
            bg_procs[bg_count++] = pid;
        }
    } else {
        perror("Failed to start radio");
    }
}
int shell_radio(char **args) {
    if (args[1] == NULL) {
        printf("Radio commands:\n");
        printf("  radio play <station#> - Play a station\n");
        printf("  radio stop           - Stop playback\n");
        printf("  radio list           - List stations\n");
        
        printf("\nAvailable stations:\n");
        for (int i = 0; stations[i].name[0] != '\0' && i < MAX_STATIONS; i++) {
            printf("%d. %s\n", i+1, stations[i].name);
        }
        return 1;
    }
    
    if (strcmp(args[1], "stop") == 0) {
        stop_radio();
    } 
    else if (strcmp(args[1], "list") == 0) {
        printf("Available stations:\n");
        for (int i = 0; stations[i].name[0] != '\0' && i < MAX_STATIONS; i++) {
            printf("%d. %s\n", i+1, stations[i].name);
        }
    }
    else if (strcmp(args[1], "play") == 0 && args[2] != NULL) {
        play_radio(atoi(args[2]));
    }
    else {
        printf("Invalid radio command\n");
    }
    
    return 1;
}

/*
 * Array of function pointers to built-in command functions
 */
int (* builtin_function[]) (char **) = {
	&shell_change_dir,
	&shell_exit,
	&shell_help,
	&shell_pwd,
	&shell_echo,
	&shell_radio
};


/*
 * Function:  split_command_line
 * -----------------------------
 *  splits a commandline into tokens using strtok()
 *
 * command: a line of command read from terminal
 *
 * returns: an array of pointers to individual tokens
 */
char ** split_command_line(char * command){
        int position = 0;
        int no_of_tokens = 64;
        char ** tokens = malloc(sizeof(char *) * no_of_tokens);
        char delim[2] = " ";

        // Split the command line into tokens with space as delimiter
        char * token = strtok(command, delim);
        while (token != NULL){
                tokens[position] = token;
                position++;
                token = strtok(NULL, delim);
        }
        tokens[position] = NULL;
        return tokens;
}

/*
 * Function:  read_command_line
 * ----------------------------
 *  reads a commandline from terminal
 *
 * returns: a line of command read from terminal
 */
char * read_command_line(void){
        int position = 0;
        int buf_size = 1024;
        char * command = (char *)malloc(sizeof(char) * 1024);
        char c;

        // Read the command line character by character
        c = getchar();
        while (c != EOF && c != '\n'){
                command[position] = c;

                // Reallocate buffer as and when needed
                if (position >= buf_size){
                        buf_size += 64;
                        command = realloc(command, buf_size);
                }

                position++;
                c = getchar();
        }
        return command;
}




/*
 * Function:  start_process
 * ------------------------
 *  starts and executes a process for a command
 *
 * args: arguments tokenized from the command line
 *
 * return: status 1
 */
 int start_process(char **args, int background) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        char cmd_path[1024];
        snprintf(cmd_path, sizeof(cmd_path), "%s%s", PATH, args[0]);
        
        // Change to the shell's current working directory before executing
        if (chdir(PWD) < 0) {
            perror("minsh");
            exit(EXIT_FAILURE);
        }
        
        if (execv(cmd_path, args) == -1) {
            perror("minsh");
            exit(EXIT_FAILURE);
        }
    } 
    else if (pid < 0) {
        perror("minsh");
        return 1;
    } 
    else {
        // Parent process
        if (background) {
            if (bg_count < MAX_BG_PROCS) {
                printf("[%d] %d\n", bg_count+1, pid);
                bg_procs[bg_count++] = pid;
            } else {
                fprintf(stderr, "Too many background processes\n");
            }
        } 
        else {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}

/*
 * Function:  shell_execute
 * ------------------------
 *  determines and executes a command as a built-in command or an external command
 *
 * args: arguments tokenized from the command line
 *
 * return: return status of the command
 */
int shell_execute(char ** args){

	if (args[0] == NULL) {
        return 1;
    }

    // Save standard file descriptors
    int std_in = dup(0);
    int std_out = dup(1);
    int std_err = dup(2);


	// Check if redirection operators are present
	int i = 1;

	while ( args[i] != NULL ){
		if ( strcmp( args[i], "<" ) == 0 ){	// Input redirection
			int inp = open( args[i+1], O_RDONLY );
			if ( inp < 0 ){
				perror("minsh");
				return 1;
			}

			if ( dup2(inp, 0) < 0 ){
				perror("minsh");
				return 1;
			}
			close(inp);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;
		}
		else if ( strcmp( args[i], "<<" ) == 0 ){	// Input redirection
			int inp = open( args[i+1], O_RDONLY );
			if ( inp < 0 ){

				perror("minsh");
				return 1;
			}

			if ( dup2(inp, 0) < 0 ){
				perror("minsh");
				return 1;
			}
			close(inp);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;
		}
		else if( strcmp( args[i], ">") == 0 ){	// Output redirection

			int out = open( args[i+1], O_WRONLY | O_TRUNC | O_CREAT, 0755 );
			if ( out < 0 ){
				perror("minsh");
				return 1;
			}

			if ( dup2(out, 1) < 0 ){
				perror("minsh");
				return 1;
			}
			close(out);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;
		}
		else if( strcmp( args[i], ">>") == 0 ){	// Output redirection (append)
			int out = open( args[i+1], O_WRONLY | O_APPEND | O_CREAT, 0755 );
			if ( out < 0 ){
				perror("minsh");
				return 1;
			}

			if ( dup2(out, 1) < 0 ){
				perror("minsh");
				return 1;

			}
			close(out);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;
		}
		else if( strcmp( args[i], "2>") == 0 ){	// Error redirection
			int err = open( args[i+1], O_WRONLY | O_CREAT, 0755 );
			if ( err < 0 ){
				perror("minsh");
				return 1;
			}

			if ( dup2(err, 2) < 0 ){
				perror("minsh");
				return 1;
			}
			close(err);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;
		}
		else if( strcmp( args[i], "2>>") == 0 ){	// Error redirection
			int err = open( args[i+1], O_WRONLY | O_CREAT | O_APPEND, 0755 );

			if ( err < 0 ){
				perror("minsh");
				return 1;
			}

			if ( dup2(err, 2) < 0 ){
				perror("minsh");
				return 1;
			}
			close(err);
			args[i] = NULL;
			args[i+1] = NULL;
			i += 2;

		}
		else{
			i++;
		}
	}

	// If the command is a built-in command, execute that function
	for (int i = 0; i < BUILTIN_COMMANDS; i++) {
        if (strcmp(args[0], builtin[i]) == 0) {
            int ret_status = (*builtin_function[i])(args);
            
            // Restore standard descriptors
            dup2(std_in, 0);
            dup2(std_out, 1);
            dup2(std_err, 2);
            return ret_status;
        }
    }

    // Handle background/foreground for external commands
    int background = 0;
    int last_arg = 0;
    
    // Find last argument
    while (args[last_arg] != NULL) last_arg++;
    if (last_arg > 0 && strcmp(args[last_arg-1], "&") == 0) {
        background = 1;
        args[last_arg-1] = NULL; // Remove '&'
    }

    // Execute external command
    int ret_status = start_process(args, background);

    // Restore standard descriptors
    dup2(std_in, 0);
    dup2(std_out, 1);
    dup2(std_err, 2);
    close(std_in);
    close(std_out);
    close(std_err);

    return ret_status;
}

/*
 * Function:  shell_loop
 * ---------------------
 *  main loop of the Mini-Shell
 */
void shell_loop(void){

	// Display help at startup
	int status = shell_help(NULL);

        char * command_line;
        char ** arguments;
	status = 1;

        while (status){
                printf("minsh> ");
                command_line = read_command_line();
		if ( strcmp(command_line, "") == 0 ){
			continue;
		}
                arguments = split_command_line(command_line);
                status = shell_execute(arguments);
        }
}

void cleanup() {
    stop_radio();
}

/*
 * Function:  main
 */
 int main(int argc, char **argv) {
    // Shell initialization
    getcwd(PWD, sizeof(PWD));    
    strcpy(PATH, PWD);
    strcat(PATH, "/cmds/");

    // Signal handling setup
    signal(SIGCHLD, sigchld_handler); 
	// signal(SIGINT, SIG_IGN);  
    signal(SIGTERM, cleanup);
    atexit(cleanup); 
    
    // Main loop of the shell
    shell_loop();

    return 0;
}
