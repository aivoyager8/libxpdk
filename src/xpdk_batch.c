#include "xpdk_internal.h"
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

/* Batch I/O implementation */

/* Forward declarations */
static void xpdk_batch_completion_callback(void *ctx, int status);
static void xpdk_batch_io_complete(struct xpdk_batch_ctx *batch_ctx, 
                                  struct xpdk_batch_io *io, int status);

/* Initialize a batch I/O context */
struct xpdk_batch_ctx *xpdk_batch_init(int max_ios)
{
    if (max_ios <= 0 || max_ios > 1024) {
        return NULL;
    }

    struct xpdk_batch_ctx *ctx = malloc(sizeof(struct xpdk_batch_ctx));
    if (!ctx) {
        return NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
    
    ctx->max_ios = max_ios;
    ctx->pending_ios = 0;
    ctx->completed_ios = 0;

    /* Allocate queues */
    ctx->pending_queue = malloc(sizeof(struct xpdk_batch_io) * max_ios);
    ctx->completed_queue = malloc(sizeof(struct xpdk_batch_io) * max_ios);
    
    if (!ctx->pending_queue || !ctx->completed_queue) {
        free(ctx->pending_queue);
        free(ctx->completed_queue);
        free(ctx);
        return NULL;
    }

    /* Initialize synchronization primitives */
    if (pthread_mutex_init(&ctx->lock, NULL) != 0) {
        free(ctx->pending_queue);
        free(ctx->completed_queue);
        free(ctx);
        return NULL;
    }

    if (pthread_cond_init(&ctx->completion_cond, NULL) != 0) {
        pthread_mutex_destroy(&ctx->lock);
        free(ctx->pending_queue);
        free(ctx->completed_queue);
        free(ctx);
        return NULL;
    }

    return ctx;
}

/* Cleanup a batch I/O context */
void xpdk_batch_cleanup(struct xpdk_batch_ctx *ctx)
{
    if (!ctx) {
        return;
    }

    pthread_mutex_lock(&ctx->lock);
    
    /* Wait for all pending I/O to complete */
    while (ctx->pending_ios > 0) {
        pthread_cond_wait(&ctx->completion_cond, &ctx->lock);
    }
    
    pthread_mutex_unlock(&ctx->lock);

    /* Cleanup synchronization primitives */
    pthread_cond_destroy(&ctx->completion_cond);
    pthread_mutex_destroy(&ctx->lock);

    /* Free memory */
    free(ctx->pending_queue);
    free(ctx->completed_queue);
    free(ctx);
}

/* Submit a single I/O to batch context (non-blocking) */
int xpdk_batch_submit_one(struct xpdk_batch_ctx *ctx, const struct xpdk_batch_io *io)
{
    if (!ctx || !io) {
        return XPDK_ERROR_INVALID;
    }

    pthread_mutex_lock(&ctx->lock);

    /* Check if queue is full */
    if (ctx->pending_ios >= ctx->max_ios) {
        pthread_mutex_unlock(&ctx->lock);
        return XPDK_ERROR_BUSY;
    }

    /* Add to pending queue */
    ctx->pending_queue[ctx->pending_ios] = *io;
    ctx->pending_ios++;

    pthread_mutex_unlock(&ctx->lock);

    /* Submit the I/O operation */
    int rc;
    switch (io->type) {
    case XPDK_IO_READ:
        rc = xpdk_read_async(io->fd, io->buffer, io->count, io->offset,
                            xpdk_batch_completion_callback, (void *)ctx);
        break;
    case XPDK_IO_WRITE:
        rc = xpdk_write_async(io->fd, io->buffer, io->count, io->offset,
                             xpdk_batch_completion_callback, (void *)ctx);
        break;
    case XPDK_IO_FLUSH:
        /* Flush doesn't need buffer/count/offset parameters for this simple implementation */
        rc = XPDK_ERROR_INVALID; /* TODO: Implement async flush */
        break;
    default:
        rc = XPDK_ERROR_INVALID;
        break;
    }

    if (rc != XPDK_SUCCESS) {
        /* Remove from pending queue on failure */
        pthread_mutex_lock(&ctx->lock);
        ctx->pending_ios--;
        pthread_mutex_unlock(&ctx->lock);
    }

    return rc;
}

/* Process completions from batch context */
int xpdk_batch_process_completions(struct xpdk_batch_ctx *ctx, int max_completions)
{
    if (!ctx) {
        return XPDK_ERROR_INVALID;
    }

    int processed = 0;

    pthread_mutex_lock(&ctx->lock);

    while (ctx->completed_ios > 0 && processed < max_completions) {
        /* Get completed I/O */
        struct xpdk_batch_io *completed_io = &ctx->completed_queue[processed];
        
        /* Call user callback if set */
        if (ctx->global_callback) {
            pthread_mutex_unlock(&ctx->lock);
            ctx->global_callback(completed_io->user_ctx, completed_io->status);
            pthread_mutex_lock(&ctx->lock);
        }

        processed++;
        ctx->completed_ios--;
    }

    /* Shift remaining completed I/Os to the front */
    if (ctx->completed_ios > 0 && processed > 0) {
        memmove(ctx->completed_queue, 
                &ctx->completed_queue[processed],
                sizeof(struct xpdk_batch_io) * ctx->completed_ios);
    }

    pthread_mutex_unlock(&ctx->lock);

    return processed;
}

/* Internal completion callback for batch operations */
static void xpdk_batch_completion_callback(void *ctx, int status)
{
    struct xpdk_batch_ctx *batch_ctx = (struct xpdk_batch_ctx *)ctx;
    
    pthread_mutex_lock(&batch_ctx->lock);

    /* Find the corresponding pending I/O */
    for (int i = 0; i < batch_ctx->pending_ios; i++) {
        struct xpdk_batch_io *pending_io = &batch_ctx->pending_queue[i];
        
        /* Simple matching - in real implementation you'd need better tracking */
        /* For now, just complete the first pending I/O */
        if (i == 0) {
            /* Move to completed queue */
            batch_ctx->completed_queue[batch_ctx->completed_ios] = *pending_io;
            batch_ctx->completed_queue[batch_ctx->completed_ios].status = status;
            batch_ctx->completed_ios++;

            /* Remove from pending queue */
            memmove(&batch_ctx->pending_queue[0],
                    &batch_ctx->pending_queue[1],
                    sizeof(struct xpdk_batch_io) * (batch_ctx->pending_ios - 1));
            batch_ctx->pending_ios--;
            
            break;
        }
    }

    /* Signal completion */
    pthread_cond_signal(&batch_ctx->completion_cond);
    pthread_mutex_unlock(&batch_ctx->lock);
}

/* Submit batch I/O operations (legacy interface) */
int xpdk_batch_submit(struct xpdk_batch_io *ios, int count, xpdk_io_callback_t callback)
{
    if (!ios || count <= 0) {
        return XPDK_ERROR_INVALID;
    }

    struct xpdk_msg *msg = xpdk_msg_alloc(XPDK_MSG_BATCH_SUBMIT);
    if (!msg) {
        return XPDK_ERROR_NOMEM;
    }

    msg->batch.ios = ios;
    msg->batch.count = count;
    msg->batch.callback = callback;

    int rc = xpdk_msg_send_async(msg);
    if (rc != XPDK_SUCCESS) {
        xpdk_msg_free(msg);
    }

    return rc;
}

/* Wait for batch I/O completions with timeout */
int xpdk_batch_wait(int max_completions, int timeout_ms)
{
    if (max_completions <= 0) {
        return XPDK_ERROR_INVALID;
    }

    int total_completions = 0;
    struct timespec timeout_spec;
    
    if (timeout_ms > 0) {
        clock_gettime(CLOCK_REALTIME, &timeout_spec);
        timeout_spec.tv_sec += timeout_ms / 1000;
        timeout_spec.tv_nsec += (timeout_ms % 1000) * 1000000;
        
        /* Handle nanosecond overflow */
        if (timeout_spec.tv_nsec >= 1000000000) {
            timeout_spec.tv_sec++;
            timeout_spec.tv_nsec -= 1000000000;
        }
    }

    /* Poll for completions */
    while (total_completions < max_completions) {
        int completions = xpdk_poll(max_completions - total_completions);
        if (completions < 0) {
            return completions; /* Error */
        }
        
        total_completions += completions;
        
        if (completions == 0) {
            /* No completions, check timeout */
            if (timeout_ms > 0) {
                struct timespec current_time;
                clock_gettime(CLOCK_REALTIME, &current_time);
                
                if (current_time.tv_sec > timeout_spec.tv_sec ||
                    (current_time.tv_sec == timeout_spec.tv_sec && 
                     current_time.tv_nsec >= timeout_spec.tv_nsec)) {
                    break; /* Timeout reached */
                }
            }
            
            /* Brief sleep to avoid busy waiting */
            usleep(100); /* 100 microseconds */
        }
    }

    return total_completions;
}

/* SPDK thread handler for batch submit */
void xpdk_spdk_handle_batch_submit(struct xpdk_msg *msg)
{
    struct xpdk_batch_io *ios = msg->batch.ios;
    int count = msg->batch.count;
    xpdk_io_callback_t callback = msg->batch.callback;
    int successful_submits = 0;

    /* Submit each I/O operation */
    for (int i = 0; i < count; i++) {
        struct xpdk_batch_io *io = &ios[i];
        int rc;

        switch (io->type) {
        case XPDK_IO_READ:
            rc = xpdk_read_async(io->fd, io->buffer, io->count, io->offset,
                               callback, io->user_ctx);
            break;
        case XPDK_IO_WRITE:
            rc = xpdk_write_async(io->fd, io->buffer, io->count, io->offset,
                                callback, io->user_ctx);
            break;
        case XPDK_IO_FLUSH:
            /* For flush, we'll do a synchronous operation for simplicity */
            rc = xpdk_flush(io->fd);
            if (callback) {
                callback(io->user_ctx, rc);
            }
            break;
        default:
            rc = XPDK_ERROR_INVALID;
            if (callback) {
                callback(io->user_ctx, rc);
            }
            break;
        }

        if (rc == XPDK_SUCCESS) {
            successful_submits++;
        }

        /* Update the status in the original I/O structure */
        io->status = rc;
    }

    msg->status = successful_submits;
    msg->completed = true;
}
