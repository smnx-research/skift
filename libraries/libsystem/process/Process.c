
#include <libsystem/Assert.h>
#include <libsystem/Result.h>
#include <libsystem/__plugs__.h>
#include <libsystem/process/Launchpad.h>

int process_this(void)
{
    return __plug_process_this();
}

Result process_run(const char *command, int *pid)
{
    Launchpad *launchpad = launchpad_create("shell", "/bin/shell");

    launchpad_argument(launchpad, "-c");
    launchpad_argument(launchpad, command);

    return launchpad_launch(launchpad, pid);
}

void __attribute__((noreturn)) process_exit(int code)
{
    __plug_process_exit(code);

    ASSERT_NOT_REACHED();
}

Result process_cancel(int pid)
{
    return __plug_process_cancel(pid);
}

int process_map(uintptr_t addr, size_t count)
{
    return __plug_process_map(addr, count);
}

int process_unmap(uintptr_t addr, size_t count)
{
    return __plug_process_map(addr, count);
}

uint process_alloc(size_t count)
{
    return __plug_process_alloc(count);
}

int process_free(uintptr_t addr, size_t count)
{
    return __plug_process_free(addr, count);
}

Result process_get_cwd(char *buffer, size_t size)
{
    return __plug_process_get_cwd(buffer, size);
}

Result process_set_cwd(const char *cwd)
{
    return __plug_process_set_cwd(cwd);
}

int process_sleep(int time)
{
    return __plug_process_sleep(time);
}

int process_wakeup(int pid)
{
    return __plug_process_wakeup(pid);
}

int process_wait(int pid, int *exit_value)
{
    return __plug_process_wait(pid, exit_value);
}
