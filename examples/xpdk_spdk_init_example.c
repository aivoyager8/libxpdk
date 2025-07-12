#include <stdio.h>
#include <spdk/stdinc.h>
#include <spdk/event.h>
#include <spdk/log.h>

static void app_start_fn(void *arg)
{
    SPDK_NOTICELOG("SPDK app started successfully!\n");
    spdk_app_stop(0);
}

int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc;

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "xpdk_spdk_init_example";
    opts.mem_size = 1024; // 1GB hugepage
    opts.rpc_addr = NULL;

    SPDK_NOTICELOG("SPDK app opts:\n");
    SPDK_NOTICELOG("  name: %s\n", opts.name);
    SPDK_NOTICELOG("  mem_size: %d MB\n", opts.mem_size);
    SPDK_NOTICELOG("  rpc_addr: %s\n", opts.rpc_addr ? opts.rpc_addr : "(null)");

    rc = spdk_app_start(&opts, app_start_fn, NULL);
    if (rc) {
        SPDK_ERRLOG("SPDK app_start failed: %d\n", rc);
    }

    spdk_app_fini();
    return rc;
}
