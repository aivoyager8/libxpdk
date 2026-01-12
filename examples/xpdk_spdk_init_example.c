#include <stdio.h>
#include <spdk/stdinc.h>
#include <spdk/event.h>
#include <spdk/log.h>


#include <spdk/bdev.h>
#include <spdk/bdev_module.h>
#include <spdk/env.h>
#include <spdk/thread.h>

struct xpdk_context {
    const char *bdev_name;
    struct spdk_bdev *bdev;
    struct spdk_bdev_desc *bdev_desc;
    struct spdk_io_channel *bdev_io_channel;
    void *buff;
    uint32_t buff_size;
};

static void xpdk_bdev_event_cb(enum spdk_bdev_event_type type, struct spdk_bdev *bdev, void *event_ctx) {
    SPDK_NOTICELOG("Unsupported bdev event: type %d\n", type);
}

static void xpdk_app_main(void *arg) {
    struct xpdk_context *ctx = (struct xpdk_context *)arg;
    int rc;
    uint32_t buf_align;

    SPDK_NOTICELOG("[XPDK] SPDK app started, bdev_name=%s\n", ctx->bdev_name ? ctx->bdev_name : "(null)");
    if (!ctx->bdev_name) {
        SPDK_ERRLOG("No bdev name specified!\n");
        spdk_app_stop(-1);
        return;
    }

    rc = spdk_bdev_open_ext(ctx->bdev_name, true, xpdk_bdev_event_cb, NULL, &ctx->bdev_desc);
    if (rc) {
        SPDK_ERRLOG("Could not open bdev: %s\n", ctx->bdev_name);
        spdk_app_stop(-1);
        return;
    }
    ctx->bdev = spdk_bdev_desc_get_bdev(ctx->bdev_desc);

    ctx->bdev_io_channel = spdk_bdev_get_io_channel(ctx->bdev_desc);
    if (!ctx->bdev_io_channel) {
        SPDK_ERRLOG("Could not create bdev I/O channel!\n");
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }

    ctx->buff_size = spdk_bdev_get_block_size(ctx->bdev) * spdk_bdev_get_write_unit_size(ctx->bdev);
    buf_align = spdk_bdev_get_buf_align(ctx->bdev);
    ctx->buff = spdk_dma_zmalloc(ctx->buff_size, buf_align, NULL);
    if (!ctx->buff) {
        SPDK_ERRLOG("Failed to allocate buffer\n");
        spdk_put_io_channel(ctx->bdev_io_channel);
        spdk_bdev_close(ctx->bdev_desc);
        spdk_app_stop(-1);
        return;
    }
    snprintf(ctx->buff, ctx->buff_size, "%s", "Hello World!\n");

    SPDK_NOTICELOG("[XPDK] bdev opened and buffer allocated.\n");
    // 这里只做初始化流程，实际读写可参考 hello_bdev
    spdk_dma_free(ctx->buff);
    spdk_put_io_channel(ctx->bdev_io_channel);
    spdk_bdev_close(ctx->bdev_desc);
    spdk_app_stop(0);
}


int main(int argc, char **argv)
{
    struct spdk_app_opts opts = {};
    int rc;
    struct xpdk_context ctx = {};

    spdk_app_opts_init(&opts, sizeof(opts));
    opts.name = "xpdk_spdk_init_example";
    opts.mem_size = 1024;
    opts.rpc_addr = NULL;

    // 简单参数解析，支持 -b bdev_name
    for (int i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-b") == 0) {
            ctx.bdev_name = argv[i + 1];
        }
    }

    SPDK_NOTICELOG("SPDK app opts:\n");
    SPDK_NOTICELOG("  name: %s\n", opts.name);
    SPDK_NOTICELOG("  mem_size: %d MB\n", opts.mem_size);
    SPDK_NOTICELOG("  rpc_addr: %s\n", opts.rpc_addr ? opts.rpc_addr : "(null)");
    SPDK_NOTICELOG("  bdev_name: %s\n", ctx.bdev_name ? ctx.bdev_name : "(null)");

    rc = spdk_app_start(&opts, xpdk_app_main, &ctx);
    if (rc) {
        SPDK_ERRLOG("SPDK app_start failed: %d\n", rc);
    }

    spdk_app_fini();
    return rc;
}
