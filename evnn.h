#ifndef __EVNN_H__
#define __EVNN_H__

struct evnn;
struct evnn_msg;

typedef int (* evnn_cb)(struct evnn *, struct evnn_msg *, void *);

struct evnn * evnn_new(struct event_base *, int domain, int proto);
int           evnn_connect(struct evnn *, const char *);
int           evnn_bind(struct evnn *, const char *);
int           evnn_start(struct evnn *);
int           evnn_setcb(struct evnn *, evnn_cb cb, void *);


#endif

