/*

tart.h -- Tiny Actor Run-Time

"MIT License"

Copyright (c) 2013 Dale Schumacher, Tristan Slominski

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

#undef inline /*inline*/

#define TRACE(x) x        /* enable tracing */
#define TRACE_MEMORY(x)   /* enable tracing */

#define ALLOC(S)    (calloc((S), 1))
#define NEW(T)      ((T *)calloc(sizeof(T), 1))
#define FREE(p)     ((p) = (free(p), NULL))

typedef void *Any;

typedef struct pair PAIR, *Pair;
typedef struct config CONFIG, *Config;
typedef struct behavior BEHAVIOR, *Behavior;
typedef struct actor ACTOR, *Actor;
typedef struct event EVENT, *Event;

typedef void (*Action)(Event e);

struct pair {
    Any         h;
    Any         t;
};

#define NIL ((Pair)0)
#define PR(h,t) pair_new((h),(t))

extern Pair     pair_new(Any h, Any t);

extern Pair     list_new();
extern int      list_empty_p(Pair list);
extern Pair     list_pop(Pair list);
extern Pair     list_push(Pair list, Any item);

extern Pair     queue_new();
extern int      queue_empty_p(Pair q);
extern void     queue_give(Pair q, Any item);
extern Any      queue_take(Pair q);

extern Pair     dict_new();
extern int      dict_empty_p(Pair dict);
extern Any      dict_lookup(Pair dict, Any key);
extern Pair     dict_bind(Pair dict, Any key, Any value);

struct actor {
    Behavior    behavior;   // current behavior
};

struct behavior {
    Action      action;     // code
    Any         context;    // data
};

struct event {
    Config      sponsor;    // sponsor configuration
    Actor       actor;      // target actor
    Any         message;    // message to deliver
};

struct config {
    Pair        event_q;    // queue of messages in-transit
    Pair        actors;     // list of actors created
};

extern Actor    actor_new(Behavior beh);
extern void     actor_become(Actor a, Behavior beh);

extern Behavior behavior_new(Action act, Any data);

extern Event    event_new(Config cfg, Actor a, Any msg);

extern Config   config_new();
extern void     config_enqueue(Config cfg, Event e);
extern void     config_enlist(Config cfg, Actor a);
extern void     config_send(Config cfg, Actor target, Any msg);
extern void     config_create(Config cfg, Behavior beh);
extern int      config_dispatch(Config cfg);

extern BEHAVIOR sink_behavior;
extern ACTOR sink_actor;