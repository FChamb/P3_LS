/* SPDX-License-Identifier: MIT */

/* StACSOS - Kernel
 *
 * Copyright (c) University of St Andrews 2024
 * Tom Spink <tcs6@st-andrews.ac.uk>
 */
#include <stacsos/kernel/arch/x86/cregs.h>
#include <stacsos/kernel/arch/x86/pio.h>
#include <stacsos/kernel/debug.h>
#include <stacsos/kernel/fs/vfs.h>
#include <stacsos/kernel/mem/address-space.h>
#include <stacsos/kernel/obj/object-manager.h>
#include <stacsos/kernel/obj/object.h>
#include <stacsos/kernel/sched/process-manager.h>
#include <stacsos/kernel/sched/process.h>
#include <stacsos/kernel/sched/sleeper.h>
#include <stacsos/kernel/sched/thread.h>
#include <stacsos/syscalls.h>
#include <stacsos/kernel/fs/tar-filesystem.h>
#include <stacsos/string.h>

using namespace stacsos;
using namespace stacsos::kernel;
using namespace stacsos::kernel::sched;
using namespace stacsos::kernel::obj;
using namespace stacsos::kernel::fs;
using namespace stacsos::kernel::mem;
using namespace stacsos::kernel::arch::x86;

static syscall_result do_open(process &owner, const char *path)
{
	auto node = vfs::get().lookup(path);
	if (node == nullptr) {
		return syscall_result { syscall_result_code::not_found, 0 };
	}

	auto file = node->open();
	if (!file) {
		return syscall_result { syscall_result_code::not_supported, 0 };
	}

	auto file_object = object_manager::get().create_file_object(owner, file);
	return syscall_result { syscall_result_code::ok, file_object->id() };
}

static syscall_result operation_result_to_syscall_result(operation_result &&o)
{
	syscall_result_code rc = (syscall_result_code)o.code;
	return syscall_result { rc, o.data };
}

extern "C" syscall_result handle_syscall(syscall_numbers index, u64 arg0, u64 arg1, u64 arg2, u64 arg3)
{
	auto &current_thread = thread::current();
	auto &current_process = current_thread.owner();

	// dprintf("SYSCALL: %u %x %x %x %x\n", index, arg0, arg1, arg2, arg3);

	switch (index) {
	case syscall_numbers::exit:
		current_process.stop();
		return syscall_result { syscall_result_code::ok, 0 };

	case syscall_numbers::set_fs:
		stacsos::kernel::arch::x86::fsbase::write(arg0);
		return syscall_result { syscall_result_code::ok, 0 };

	case syscall_numbers::set_gs:
		stacsos::kernel::arch::x86::gsbase::write(arg0);
		return syscall_result { syscall_result_code::ok, 0 };

	case syscall_numbers::open:
		return do_open(current_process, (const char *)arg0);

	case syscall_numbers::close:
		object_manager::get().free_object(current_process, arg0);
		return syscall_result { syscall_result_code::ok, 0 };

	case syscall_numbers::write: {
		auto o = object_manager::get().get_object(current_process, arg0);
		if (!o) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(o->write((const void *)arg1, arg2));
	}

	case syscall_numbers::pwrite: {
		auto o = object_manager::get().get_object(current_process, arg0);
		if (!o) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(o->pwrite((const void *)arg1, arg2, arg3));
	}

	case syscall_numbers::read: {
		auto o = object_manager::get().get_object(current_process, arg0);
		if (!o) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(o->read((void *)arg1, arg2));
	}

	case syscall_numbers::pread: {
		auto o = object_manager::get().get_object(current_process, arg0);
		if (!o) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(o->pread((void *)arg1, arg2, arg3));
	}

	case syscall_numbers::ioctl: {
		auto o = object_manager::get().get_object(current_process, arg0);
		if (!o) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(o->ioctl(arg1, (void *)arg2, arg3));
	}

	case syscall_numbers::alloc_mem: {
		auto rgn = current_thread.owner().addrspace().alloc_region(PAGE_ALIGN_UP(arg0), region_flags::readwrite, true);

		return syscall_result { syscall_result_code::ok, rgn->base };
	}

	case syscall_numbers::start_process: {
		dprintf("start process: %s %s\n", arg0, arg1);

		auto new_proc = process_manager::get().create_process((const char *)arg0, (const char *)arg1);
		if (!new_proc) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		new_proc->start();
		return syscall_result { syscall_result_code::ok, object_manager::get().create_process_object(current_process, new_proc)->id() };
	}

	case syscall_numbers::wait_for_process: {
		// dprintf("wait process: %lu\n", arg0);

		auto process_object = object_manager::get().get_object(current_process, arg0);
		if (!process_object) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(process_object->wait_for_status_change());
	}

	case syscall_numbers::start_thread: {
		auto new_thread = current_thread.owner().create_thread((u64)arg0, (void *)arg1);
		new_thread->start();

		return syscall_result { syscall_result_code::ok, object_manager::get().create_thread_object(current_process, new_thread)->id() };
	}

	case syscall_numbers::stop_current_thread: {
		current_thread.stop();
		asm volatile("int $0xff");

		return syscall_result { syscall_result_code::ok, 0 };
	}

	case syscall_numbers::join_thread: {
		auto thread_object = object_manager::get().get_object(current_process, arg0);
		if (!thread_object) {
			return syscall_result { syscall_result_code::not_found, 0 };
		}

		return operation_result_to_syscall_result(thread_object->join());
	}

	case syscall_numbers::sleep: {
		sleeper::get().sleep_ms(arg0);
		return syscall_result { syscall_result_code::ok, 0 };
	}

	case syscall_numbers::poweroff: {
		pio::outw(0x604, 0x2000);
		return syscall_result { syscall_result_code::ok, 0 };
	}

    /**
     * This case for syscall_numbers::ls handles ls. It receives the directory entries
     * and formats them into a buffer provided by the caller.
     *
     * @param arg0 - Path to the directory to be listed
     * @param arg1 - Buffer where the directory listing is to be written
     * @param arg2 - Size of the buffer
     * @param arg3 - Flag indicating whether to use long listing format
     */
    case syscall_numbers::ls: {
        // Convert u64 arguments to usable attributes
        auto path = (const char *) arg0;
        char *buffer = reinterpret_cast<char *>(arg1);
        auto buffer_size = static_cast<u64>(arg2);
        auto long_listing = (bool) arg3;

        // Looks up the path to the virtual file system to receive the corresponding node
        auto node = vfs::get().lookup(path);

        // Decrease the buffer_size by one to account for '\0'
        buffer_size--;

        // Integers for pretty printing format
        int maxFull = 50;
        int max = 0;
        int extra = 10;

        // Check if the retrieved node is a directory
        if (node && node->kind() == fs_node_kind::directory) {

            // Casts the node to tarfs_node type to access directory specific methods
            auto dir = static_cast<tarfs_node*>(node);

            // If directory doesn't exist return not_found
            if (!dir) {
                return syscall_result { syscall_result_code::not_found, 0 };
            }

            // Store the current index for the buffer and retrieve the children of the directory
            int index = 0;
            list<tarfs_node*> children = dir->children();

            // Cycle through all the files and determine the max length name of files for spacing
            for (fs_node* child : children) {
                if (child->name().length() > max) {
                    max = child->name().length();
                    if (max > maxFull) {
                        break;
                    }
                }
            }

            // Iterate over the files and get their name and name length
            for (tarfs_node* child : children) {
                int name_length = child->name().length();
                string name = child->name();

                // For some reason their sometimes exists a file with a name "" -> Skip this
                if (name == "") {
                    continue;
                }

                // If -l has been passed, add the tag [F] or [D] to front of file buffer
                if (long_listing) {
                    buffer[index++] = '[';
                    buffer[index++] = child->kind() == fs_node_kind::directory ? 'D' : 'F';
                    buffer[index++] = ']';
                    buffer[index++] = ' ';
                }

                // Add each file name to the buffer
                for (int i = 0; i < name.length(); ++i) {
                    buffer[index++] = name[i];

                    // Ensure not exceeding buffer capacity
                    if (index == buffer_size) {
                        buffer[++index] = '\0';
                        return syscall_result { syscall_result_code::ok, 0 };
                    }
                }

                // Add pretty format space for -l flad
                if (long_listing) {
                    for (int i = 0; i < max + extra - name_length; i++) {
                        buffer[index++] = ' ';
                    }

                    // Add the size of each file/directory to buffer
                    u64 size = child->size();
                    string str_size = string::to_string(size);
                    for (int i = 0; i < str_size.length(); i++) {
                        if (child->kind() == fs_node_kind::file) {
                            buffer[index++] = str_size[i];
                        }
                    }
                }

                // New line and check not exceeding buffer
                buffer[index++] = '\n';
                if (index == buffer_size) {
                    buffer[++index] = '\0';
                    return syscall_result { syscall_result_code::ok, 0 };
                }
            }

            // Ensures the buffer is null-terminated + return result
            buffer[index] = '\0';
            return syscall_result { syscall_result_code::ok, 0 };
        }

        // No result found
        return syscall_result { syscall_result_code::not_found, 0 };
    }

	default:
		dprintf("ERROR: unsupported syscall: %lx\n", index);
		return syscall_result { syscall_result_code::not_supported, 0 };
	}
}
