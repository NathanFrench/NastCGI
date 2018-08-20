#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include <event2/event.h>
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>

struct evnn_msg {
    char * buf;
    size_t len;
};

struct evnn {
    struct event_base * evbase_;
    struct event      * ev_;
    int                 sock_;
    int                 rd_sock_;
    int                 (* cb_)(struct evnn *, struct evnn_msg *, void *);
    void              * arg_;
};


static int
evnn_recvmsg_(struct evnn * nn, struct evnn_msg * msg)
{
    struct nn_iovec  iov = { 0 };
    struct nn_msghdr hdr = { 0 };
    int              res;


    iov.iov_base   = &msg->buf;
    iov.iov_len    = NN_MSG;

    hdr.msg_iov    = &iov;
    hdr.msg_iovlen = 1;

    msg->len       = nn_recvmsg(nn->sock_, &hdr, NN_DONTWAIT);

    if (msg->len == -1) {
        return -1;
    }


    return 0;
}

static int
evnn_sendmsg_(struct evnn * nn, struct evnn_msg * msg)
{
    struct nn_msghdr hdr = { 0 };
    struct nn_iovec  iov[1];
    char           * b   = nn_allocmsg(msg->len, 0);

    memcpy(b, msg->buf, msg->len);

    iov[0].iov_base = &b;
    iov[0].iov_len  = NN_MSG;

    hdr.msg_iov     = iov;
    hdr.msg_iovlen  = 1;

    return nn_sendmsg(nn->sock_, &hdr, 0);
}

static void
evnn_readcb_(int sock, short which, void * arg)
{
    struct evnn   * nn;
    struct evnn_msg msg = { 0 };

    nn = (struct evnn *)arg;
    assert(nn != NULL);

    if (evnn_recvmsg_(nn, &msg) == -1) {
        return;
    }

    if (nn->cb_) {
        nn->cb_(nn, &msg, nn->arg_);
    }

    if (msg.buf) {
        nn_freemsg(msg.buf);
    }
}

static int
evnn_init_reader_(struct evnn * nn)
{
    size_t         slen;
    int            sock;
    int            rdfd;
    struct event * ev;

    assert(nn != NULL);

    sock = nn->sock_;
    slen = sizeof(sock);


    if (nn_getsockopt(sock, NN_SOL_SOCKET, NN_RCVFD, &rdfd, &slen) == -1) {
        return -1;
    }

    ev           = event_new(nn->evbase_, rdfd, EV_READ | EV_PERSIST, evnn_readcb_, nn);
    assert(ev != NULL);

    nn->ev_      = ev;
    nn->rd_sock_ = rdfd;

    return 0;
}

struct evnn *
evnn_new(struct event_base * evbase, int domain, int proto)
{
    struct evnn * nn;

    assert(evbase != NULL); 
    nn          = calloc(1, sizeof(*nn));
    assert(nn != NULL);

    nn->evbase_ = evbase;
    nn->sock_   = nn_socket(domain, proto);
    assert(nn->sock_ >= 0);

    evnn_init_reader_(nn);

    return nn;
}

int
evnn_connect(struct evnn * nn, const char * transport)
{
    assert(nn != NULL);
    assert(transport != NULL);

    return nn_connect(nn->sock_, transport);
}

int
evnn_bind(struct evnn * nn, const char * transport)
{
    assert(nn != NULL);
    assert(transport != NULL);

    return nn_bind(nn->sock_, transport);
}

int
evnn_setcb(struct evnn * nn,
           int (*cb)(struct evnn *, struct evnn_msg *, void *),
           void * arg)
{
    assert(nn != NULL);

    nn->cb_  = cb;
    nn->arg_ = arg;

    return 0;
}

int
evnn_start(struct evnn * nn)
{
    assert(nn != NULL);
    return event_add(nn->ev_, NULL);
}

#ifdef DEBUG_MAIN
static int
nn_readcb_(struct evnn * nn, struct evnn_msg * msg, void * arg)
{
    printf("hi %.*s\n", msg->len, msg->buf);

    /*
    if (strcmp("DATE", msg->buf) == 0) {
        struct evnn_msg m = {
            .buf = "HELLO\0",
            .len = 6
        };

        evnn_sendmsg_(nn, &m);
    }
    */

    return 0;
}

int
main(int argc, char ** argv)
{
    struct event_base * evbase = event_base_new();
    struct evnn       * nn     = evnn_new(evbase, AF_SP, NN_REQ);

    evnn_bind(nn, "ipc:///tmp/bleh.ipc");
    evnn_setcb(nn, nn_readcb_, NULL);
    evnn_start(nn);

    struct evnn_msg m = {
        .buf = "HELLO\0",
        .len = 6
    };

    evnn_sendmsg_(nn, &m);

    event_base_loop(evbase, 0);
    return 0;
}
#endif
