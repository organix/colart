/*

tart.c -- Tiny Actor Run-Time

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
#include "tart.h"

inline Pair     pair_new(Any h, Any t) {
    Pair q = NEW(PAIR);
    TRACE_MEMORY(fprintf(stderr, "ALLOC pair(%p)\n", q));
    q->h = h;
    q->t = t;
    return q;
}

inline Pair     list_new() { return NIL; }
inline int      list_empty_p(Pair list) { return (list == NIL); }
inline Pair     list_pop(Pair list) { return list; }
inline Pair     list_push(Pair list, Any item) { 
    Pair p = PR(item, list); 
    TRACE_MEMORY(fprintf(stderr, "ALLOC pair(%p)\n", p));
    return p;
}

inline Pair     queue_new() { return PR(NIL, NIL); }
inline int      queue_empty_p(Pair q) { return (q->h == NIL); }
inline void     queue_give(Pair q, Any item) {
    Pair p = PR(item, NIL);
    TRACE_MEMORY(fprintf(stderr, "ALLOC pair(%p)\n", p));
    if (queue_empty_p(q)) {
        q->h = p;
    } else {
        Pair t = q->t;
        t->t = p;
    }
    q->t = p;
}
inline Any      queue_take(Pair q) {
    // if (queue_empty_p(q)) ERROR("can't take from empty queue");
    Pair p = q->h;
    Any item = p->h;
    q->h = p->t;
    TRACE_MEMORY(fprintf(stderr, "FREE pair(%p)\n", p));
    p = FREE(p);
    return item;
}

inline Pair
dict_new()
{
    return NIL;
}

inline int
dict_empty_p(Pair dict)
{
    return (dict == NIL);
}

inline Any
dict_lookup(Pair dict, Any key)
{
    while (!dict_empty_p(dict)) {
        Pair p = dict->h;
        if (p->h == key) {
            return p->t;
        }
        dict = dict->t;
    }
    return NULL;  // NOT FOUND!
}

inline Pair
dict_bind(Pair dict, Any key, Any value)
{
    Pair p = PR(PR(key, value), dict);
    TRACE_MEMORY(fprintf(stderr, "ALLOC pair(%p)\n", p->h));
    TRACE_MEMORY(fprintf(stderr, "ALLOC pair(%p)\n", p));
    return p;
}

inline Actor    actor_new(Behavior beh) {
    Actor a = NEW(ACTOR);
    TRACE_MEMORY(fprintf(stderr, "ALLOC actor(%p)\n", a));
    a->behavior = beh;
    return a;
}
inline void     actor_become(Actor a, Behavior beh) { a->behavior = beh; }

inline Behavior behavior_new(Action act, Any data) {
    Behavior beh = NEW(BEHAVIOR);
    TRACE_MEMORY(fprintf(stderr, "ALLOC behavior(%p)\n", beh));
    beh->action = act;
    beh->context = data;
    return beh;
}

inline Event    event_new(Config cfg, Actor a, Any msg) {
    Event e = NEW(EVENT);
    TRACE_MEMORY(fprintf(stderr, "ALLOC event(%p)\n", e));
    e->sponsor = cfg;
    e->actor = a;
    e->message = msg;
    return e;
}

inline Config   config_new() {
    Config cfg = NEW(CONFIG);
    TRACE_MEMORY(fprintf(stderr, "ALLOC config(%p)\n", cfg));
    cfg->event_q = queue_new();
    cfg->actors = NIL;
    return cfg;
}
inline void     config_enqueue(Config cfg, Event e) { queue_give(cfg->event_q, e); }
inline void     config_enlist(Config cfg, Actor a) { cfg->actors = list_push(cfg->actors, a); }
inline void     config_send(Config cfg, Actor target, Any msg) { config_enqueue(cfg, event_new(cfg, target, msg)); }
inline void     config_create(Config cfg, Behavior beh) { config_enlist(cfg, actor_new(beh)); }
int             config_dispatch(Config cfg) {
    if (queue_empty_p(cfg->event_q)) {
        return 0;
    }
    Event e = queue_take(cfg->event_q);
    (e->actor->behavior->action)(e);  // INVOKE ACTOR BEHAVIOR
    TRACE_MEMORY(fprintf(stderr, "FREE event(%p)\n", e));
    e = FREE(e); // FIXME: KEEP HISTORY HERE?
    return 1;
}

static void     act_sink(Event e) { /* no action */ }

BEHAVIOR sink_behavior = { act_sink, NIL };
ACTOR sink_actor = { &sink_behavior };
