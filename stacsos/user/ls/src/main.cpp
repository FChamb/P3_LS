//
// Created by fc84 on 25/11/24.
//

#include <stacsos/console.h>
#include <stacsos/memops.h>
#include <stacsos/objects.h>
#include <stacsos/user-syscall.h>

using namespace stacsos;

#define BUFFER_SIZE 4096

/**
 * This function interprets the command line input to list files. It also
 * determines whether to print in long format or not which lists files and
 * directories with their size. It then invokes a system call to perform the
 * file listing and displays the result.
 *
 * @param command_line The command line input string.
 * @return int Returns 0 on success, or an error code on failure.
 */
int main(const char *command_line) {
    // If usage of LS is incorrect through an error
	if (!command_line || memops::strlen(command_line) == 0) {
		console::get().write("error: usage: ls [-l] <path>\n");
		return 1;
	}

    // Grab the command line arguments and convert them to variables
    bool long_listing = false;
    if (*command_line == '\0') {
        console::get().write("error: no path provided\n");
        return 1;
    }
    if (*command_line == '-' && *(command_line + 1) == 'l') {
        long_listing = true;
        command_line += 2;
        command_line++;
    }
    if (*command_line != '/') {
        console::get().write("error: path invalid\n");
        return 1;
    }
    const char *path = command_line;

    // Store the results of ls for a given directory
    char buffer[BUFFER_SIZE];

    // Call syscall::ls with the provided arguments and store result
    syscall_result result = syscalls::ls(18, (u64) path, (u64) buffer, BUFFER_SIZE, long_listing);

    // Write output to console
    console::get().writef("%s", buffer);

    return 0;
}
