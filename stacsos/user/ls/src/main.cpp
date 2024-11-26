//
// Created by fc84 on 25/11/24.
//

#include <stacsos/console.h>
#include <stacsos/memops.h>
#include <stacsos/objects.h>
#include <stacsos/user-syscall.h>

using namespace stacsos;

#define BUFFER_SIZE 4096

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

    char buffer[BUFFER_SIZE];

    syscall_result result = syscalls::ls(18, (u64) path, (u64) buffer, BUFFER_SIZE, long_listing);
    console::get().writef("%s", buffer);

    return 0;
}
