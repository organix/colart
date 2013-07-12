/*

colart.c -- COmbined Lambda/object architecture Actor Run-Time

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
#include <string.h>

typedef struct vtable VTABLE, *Vtable;

struct vtable {
    int    size;
    int    tally;
    Pair   dict;
    Actor  parent;
};

Actor vtable_vt_actor = 0;
Actor actor_vt_actor  = 0;
Actor symbol_vt_actor = 0;

Actor s_addAction_actor          = 0;
Actor s_addActionResponse_actor  = 0;
Actor s_allocate_actor           = 0;
Actor s_allocateResponse_actor   = 0;
Actor s_delegated_actor          = 0;
Actor s_delegatedResponse_actor  = 0;
Actor s_intern_actor             = 0;
Actor s_intern_response_actor    = 0;
Actor s_lookup_actor             = 0;
Actor s_lookup_response_actor    = 0;

Actor symbol_actor      = 0;

Pair SymbolList = 0;

void act_late_bound (Event e);
void act_ground (Event e);
void act_vtable_lookup (Event e);

inline void
config_create_late_bind(Config cfg, Any data) 
{
    config_enlist(cfg, actor_new(
        behavior_new(act_late_bound, PR(vtable_vt_actor, data))));
}

void
act_late_bound (Event e)
{
    TRACE(fprintf(stderr, "%s act_late_bound\n", "***"));
    Pair message = (Pair)e->message;
    Actor action_selector = (Actor)message->h;
    // TRACE(fprintf(stderr, "action_selector(%p), s_lookup_actor(%p), e->actor(%p), vtable_vt_actor(%p)\n", action_selector, s_lookup_actor, e->actor, vtable_vt_actor));
    if ((action_selector == (Actor)s_lookup_actor) && (e->actor == (Actor)vtable_vt_actor)) {
        e->message = message->t;
        act_vtable_lookup(e); // top level lookup, ground out
    } else {
        Actor ground_actor = actor_new(
            behavior_new(act_ground, PR(e->actor, message->t)));
        Pair context = (Pair)e->actor->behavior->context;
        Vtable vtable = (Vtable)context->h;
        TRACE(fprintf(stderr, "(late bind) SEND s_lookup_actor(%p), ground_actor(%p), key(%p) TO self->vtable(%p)\n", s_lookup_actor, ground_actor, action_selector, vtable));
        config_send(e->sponsor,
            (Actor)vtable, /* vtable */
            PR(s_lookup_actor, PR(ground_actor, action_selector)));
    }
    TRACE(fprintf(stderr, "%s act_late_bound\n", ">>>"));
}

void
act_ground (Event e)
{   
    TRACE(fprintf(stderr, "%s act_ground\n", "***"));
    Pair context = (Pair)e->actor->behavior->context;
    Actor receiver = (Actor)context->h;
    Any args = (Any)context->t;

    Pair message = (Pair)e->message;
    Action action = (Action)message->t;

    Event ground_event = event_new(e->sponsor, receiver, args);

    TRACE(fprintf(stderr, "(ground bind) executing action(%p) on self(%p)\n", action, receiver));
    action(ground_event); // INVOKE ACTOR BEHAVIOR
    TRACE(fprintf(stderr, "%s act_ground\n", ">>>"));
}

void
act_symbol (Event e)
{
    TRACE(fprintf(stderr, "%s act_symbol\n", "***"));
    Pair context = (Pair)e->actor->behavior->context;
    // Vtable vtable = (Vtable)context->h;
    char * string = (char *)context->t;

    Actor customer = (Actor)e->message;

    config_send(e->sponsor, customer, string);
    TRACE(fprintf(stderr, "%s act_symbol\n", ">>>"));
}

void
act_vtable_addAction (Event e)
{
    TRACE(fprintf(stderr, "%s act_vtable_addAction\n", "***"));
    Pair context = (Pair)e->actor->behavior->context;
    // Vtable vtable = (Vtable)context->h;
    Vtable self = (Vtable)context->t;

    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    message = (Pair)message->t;
    Actor key = (Actor)message->h;
    Action action = (Action)message->t;

    // FIXME: make it mutable so we don't leak memory
    self->dict = dict_bind(self->dict, key, action);

    TRACE(fprintf(stderr, "(late bind) SEND s_addActionResponse_actor(%p), action(%p) TO customer(%p)\n", s_addActionResponse_actor, action, customer));
    config_send(e->sponsor, customer,
        PR(s_addActionResponse_actor, action));
    TRACE(fprintf(stderr, "%s act_vtable_addAction\n", ">>>"));
}

void
act_vtable_allocate (Event e)
{
    TRACE(fprintf(stderr, "%s act_vtable_allocate\n", "***"));
    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    message = (Pair)message->t;
    Actor vtable = (Actor)message->h;
    long payloadSize = (long)message->t; // FIXME: might need to be int

    Actor actor = actor_new(behavior_new(act_late_bound, 
        PR(vtable, calloc(1, payloadSize))));

    TRACE(fprintf(stderr, "(late bind) SEND s_allocateResponse_actor(%p), actor(%p) TO customer(%p)\n", s_allocateResponse_actor, actor, customer));
    config_send(e->sponsor, customer, PR(s_allocateResponse_actor, actor));
    TRACE(fprintf(stderr, "%s act_vtable_allocate\n", ">>>"));
}

void
act_bootstrap_vtable_allocate (Event e)
{
    TRACE(fprintf(stderr, "%s act_bootstrap_vtable_allocate\n", "***"));
    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    message = (Pair)message->t;
    Actor vtable = (Actor)message->h;
    long payloadSize = (long)message->t; // FIXME: might need to be int

    Actor actor = actor_new(behavior_new(act_late_bound,
        PR(vtable, calloc(1, sizeof(Vtable) + payloadSize))));
    TRACE(fprintf(stderr, "allocated_actor(%p)\n", actor));
    TRACE(fprintf(stderr, "(fast bind) SEND actor(%p) TO customer(%p)\n", actor, customer));
    config_send(e->sponsor, customer, actor);

    // free  memory after one-shot bootstrap_vtable_allocate_actor
    TRACE_MEMORY(fprintf(stderr, "FREE behavior(%p)\n", e->actor->behavior));
    e->actor->behavior = FREE(e->actor->behavior);
    TRACE_MEMORY(fprintf(stderr, "FREE actor(%p)\n", e->actor));
    e->actor = FREE(e->actor);
    TRACE(fprintf(stderr, "%s act_bootstrap_vtable_allocate\n", ">>>"));
}

void
act_vtable_delegated (Event e)
{
    TRACE(fprintf(stderr, "%s act_vtable_delegated\n", "***"));
    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    Actor vtable = (Actor)message->t;

    Actor handle_allocate_response_actor = actor_new(
        behavior_new(act_late_bound, PR(e->actor, PR(customer, vtable))));
    TRACE(fprintf(stderr, "handle_allocate_response_actor(%p)\n", handle_allocate_response_actor));
    TRACE(fprintf(stderr, "(late bind) SEND s_allocate_actor(%p), handle_allocate_response_actor(%p), kind(%p), %ld TO self(%p)\n", s_allocate_actor, handle_allocate_response_actor, vtable, sizeof(VTABLE), e->actor));
    config_send(e->sponsor, e->actor, 
        PR(s_allocate_actor, 
            PR(handle_allocate_response_actor, 
                PR(vtable, (Any)sizeof(VTABLE)))));
    TRACE(fprintf(stderr, "%s act_vtable_delegated\n", ">>>"));
}

void
act_vtable_allocate_response (Event e)
{
    TRACE(fprintf(stderr, "%s act_vtable_allocate_response\n", "***"));
    Pair context = (Pair)e->actor->behavior->context;
    // Vtable vtable = (Vtable)context->h;
    Pair data = (Pair)context->t;
    Actor customer = (Actor)data->h;
    TRACE(fprintf(stderr, "customer %p\n", customer));
    Actor parent = (Actor)data->t;

    Actor actor = (Actor)e->message;
    Vtable child = (Vtable)((Pair)actor->behavior->context)->t;
    child->size = 2;
    child->tally = 0;
    child->dict = dict_new();
    child->parent = parent;

    TRACE(fprintf(stderr, "(fast bind) SEND s_delegatedResponse_actor(%p), actor(%p) TO customer(%p)\n", s_delegatedResponse_actor, actor, customer));
    config_send(e->sponsor, customer, PR(s_delegatedResponse_actor, actor));

    // free  memory after one-shot handle_allocate_response_actor
    TRACE_MEMORY(fprintf(stderr, "FREE behavior(%p)\n", e->actor->behavior));
    e->actor->behavior = FREE(e->actor->behavior);
    TRACE_MEMORY(fprintf(stderr, "FREE actor(%p)\n", e->actor));
    e->actor = FREE(e->actor);
    TRACE(fprintf(stderr, "%s act_vtable_allocate_response\n", ">>>"));
}

void
act_bootstrap_vtable_delegated (Event e)
{
    TRACE(fprintf(stderr, "%s act_bootstrap_vtable_delegated\n", "***"));
    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    Actor vtable = (Actor)message->t;

    Actor bootstrap_vtable_allocate_actor = actor_new(
        behavior_new(act_bootstrap_vtable_allocate, NIL));
    TRACE(fprintf(stderr, "bootstrap_vtable_allocate_actor(%p)\n", bootstrap_vtable_allocate_actor));
    Actor bootstrap_vtable_allocate_response_actor = actor_new(
        behavior_new(act_vtable_allocate_response, 
            PR(0 /* vtable irrelevant */, PR(customer, vtable))));
    TRACE(fprintf(stderr, "bootstrap_vtable_allocate_response_actor(%p)\n", bootstrap_vtable_allocate_response_actor));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_vtable_allocate_response_actor(%p), kind(%p), %ld TO bootstrap_vtable_allocate_actor(%p)\n", bootstrap_vtable_allocate_response_actor, vtable, sizeof(VTABLE), bootstrap_vtable_allocate_actor));
    config_send(e->sponsor, bootstrap_vtable_allocate_actor, 
        PR(bootstrap_vtable_allocate_response_actor, 
            PR(vtable, (Any)sizeof(VTABLE))));
    TRACE(fprintf(stderr, "%s act_bootstrap_vtable_delegated\n", ">>>"));
}

void
act_lookup_failed_notification (Event e)
{
    Actor actor = (Actor)e->actor->behavior->context;

    char *string = (char *)e->message;

    fprintf(stderr, "lookup failed %p %s\n", actor, string);
}

void
act_vtable_lookup (Event e)
{
    TRACE(fprintf(stderr, "%s act_vtable_lookup\n", "***"));

    Pair context = (Pair)e->actor->behavior->context;
    Vtable self /* vtable */ = (Vtable)context->t;

    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    Actor key = (Actor)message->t;

    Action action = dict_lookup(self->dict, key);
    if (action != NULL) {
        TRACE(fprintf(stderr, "(late bind) SEND s_lookup_response_actor(%p), action(%p) TO customer(%p)\n", s_lookup_response_actor, action, customer));
        config_send(e->sponsor, customer, PR(s_lookup_response_actor, action));
        TRACE(fprintf(stderr, "%s act_vtable_lookup\n", ">>>"));
        return;
    }
    if (self->parent) {
        TRACE(fprintf(stderr, "(late bind) SEND s_lookup_actor(%p), customer(%p), key(%p) TO vtable->parent(%p)\n", s_lookup_actor, customer, key, self->parent));
        config_send(e->sponsor, self->parent, PR(s_lookup_actor, PR(customer, key)));
        TRACE(fprintf(stderr, "%s act_vtable_lookup\n", ">>>"));
        return;
    }

    Actor lookup_failed_notification_actor = actor_new(
        behavior_new(act_lookup_failed_notification, e->actor));
    TRACE(fprintf(stderr, "(late bind) SEND key(%p) TO lookup_failed_notification_actor(%p)\n", key, lookup_failed_notification_actor));
    config_send(e->sponsor, lookup_failed_notification_actor, key);
    TRACE(fprintf(stderr, "%s act_vtable_lookup\n", ">>>"));
}

void
act_symbol_intern (Event e)
{
    TRACE(fprintf(stderr, "%s act_symbol_intern\n", "***"));
    Pair message = (Pair)e->message;
    Actor customer = (Actor)message->h;
    char *string = (char *)message->t;

    Actor symbol_actor;
    Pair pair = SymbolList;
    while (pair != NIL) {
        symbol_actor = (Actor)pair->h;
        if (!strcmp(string, (char *)((Pair)symbol_actor->behavior->context)->t)) {
            TRACE(fprintf(stderr, "(late bind) SEND s_intern_response_actor(%p), symbol_actor(%p) TO customer(%p)\n", s_intern_response_actor, symbol_actor, customer));
            config_send(e->sponsor, customer,
                PR(s_intern_response_actor, symbol_actor));
            TRACE(fprintf(stderr, "%s act_symbol_intern\n", ">>>"));
            return;
        }
        pair = pair->t;
    }   
    symbol_actor = actor_new(behavior_new(act_late_bound, 
        PR(symbol_vt_actor, string)));
    SymbolList = list_push(SymbolList, symbol_actor);
    TRACE(fprintf(stderr, "(late bind) SEND s_intern_response_actor(%p), symbol_actor(%p) TO customer(%p)\n", s_intern_response_actor, symbol_actor, customer));
    config_send(e->sponsor, customer,
        PR(s_intern_response_actor, symbol_actor));
    TRACE(fprintf(stderr, "%s act_symbol_intern\n", ">>>"));
}

Actor bootstrap_vtable_delegated_actor = 0;
Actor bootstrap_symbol_intern_actor    = 0;
Actor bootstrap_vtable_addAction_actor = 0;

void act_bootstrap_1 (Event e);
void act_bootstrap_2 (Event e);
void act_bootstrap_3 (Event e);
void act_bootstrap_4 (Event e);
void act_bootstrap_5 (Event e);
void act_bootstrap_6 (Event e);
void act_bootstrap_7 (Event e);
void act_bootstrap_8 (Event e);
void act_bootstrap_9 (Event e);
void act_bootstrap_10 (Event e);
void act_bootstrap_11 (Event e);
void act_bootstrap_12 (Event e);
void act_bootstrap_13 (Event e);
void act_bootstrap_14 (Event e);
void act_bootstrap_15 (Event e);

/*
    vtable_vt = vtable_delegated(0); // complete
    vtable_vt->_vt[-1] = vtable_vt;

    object_vt = vtable_delegated(0); // begin
*/
void
act_bootstrap_1 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 1));
    Pair message = (Pair)e->message;
    vtable_vt_actor = (Actor)message->t;
    Pair vtable_vt_actor_context = (Pair)vtable_vt_actor->behavior->context;
    vtable_vt_actor_context->h /* vtable */ = vtable_vt_actor;
    ((Vtable)vtable_vt_actor_context->t)->parent = vtable_vt_actor;
    TRACE(fprintf(stderr, "vtable_vt_actor(%p)\n", vtable_vt_actor));
    TRACE(fprintf(stderr, "vtable_vt_actor->vtable(%p)\n", vtable_vt_actor_context->h));
    TRACE(fprintf(stderr, "vtable_vt_actor->vtable->parent(%p)\n", ((Vtable)vtable_vt_actor_context->t)->parent));

    actor_become(e->actor, behavior_new(act_bootstrap_2, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), 0 TO bootstrap_vtable_delegated_actor(%p)\n", e->actor, bootstrap_vtable_delegated_actor));
    config_send(e->sponsor, bootstrap_vtable_delegated_actor, 
        PR(e->actor, 0));
    TRACE(fprintf(stderr, "%s act_bootstrap_1\n", ">>>"));
}

/*
    object_vt = vtable_delegated(0); // complete
    object_vt->_vt[-1] = vtable_vt;
    vtable_vt->parent = object_vt;

    symbol_vt = vtable_delegated(object_vt); // begin
*/
void
act_bootstrap_2 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 2));
    Pair message = (Pair)e->message;
    actor_vt_actor = (Actor)message->t;
    Pair actor_vt_actor_context = (Pair)actor_vt_actor->behavior->context;
    actor_vt_actor_context->h /* vtable */ = vtable_vt_actor;
    ((Vtable)actor_vt_actor_context->t)->parent = vtable_vt_actor;
    TRACE(fprintf(stderr, "actor_vt_actor(%p)\n", actor_vt_actor));
    TRACE(fprintf(stderr, "actor_vt_actor->vtable(%p)\n", actor_vt_actor_context->h /* vtable */));
    TRACE(fprintf(stderr, "actor_vt_actor->vtable->parent(%p)\n", ((Vtable)actor_vt_actor_context->t)->parent));
    TRACE(fprintf(stderr, "vtable_vt_actor->vtable->parent(%p)\n", ((Vtable)((Pair)vtable_vt_actor->behavior->context)->t)->parent));

    actor_become(e->actor, behavior_new(act_bootstrap_3, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), actor_vt_actor(%p) TO bootstrap_vtable_delegated_actor(%p)\n", e->actor, actor_vt_actor, bootstrap_vtable_delegated_actor));
    config_send(e->sponsor, bootstrap_vtable_delegated_actor,
        PR(e->actor, actor_vt_actor));
    TRACE(fprintf(stderr, "%s act_bootstrap_2\n", ">>>"));
}

/*
    symbol_vt = vtable_delegated(object_vt); // complete
    SymbolList = vtable_delegated(0); // begin

    s_lookup = symbol_intern(0, "lookup"); // begin
*/
void
act_bootstrap_3 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 3));
    Pair message = (Pair)e->message;
    symbol_vt_actor = (Actor)message->t;
    Pair symbol_vt_actor_context = (Pair)symbol_vt_actor->behavior->context;
    TRACE(fprintf(stderr, "symbol_vt_actor(%p)\n", symbol_vt_actor));
    TRACE(fprintf(stderr, "symbol_vt_actor->vtable(%p)\n", symbol_vt_actor_context->h /* vtable */));
    TRACE(fprintf(stderr, "symbol_vt_actor->vtable->parent(%p)\n", ((Vtable)symbol_vt_actor_context->t)->parent));
    TRACE(fprintf(stderr, "actor_vt_actor->vtable->parent(%p)\n", ((Vtable)((Pair)actor_vt_actor->behavior->context)->t)->parent));

    SymbolList = list_new();

    bootstrap_symbol_intern_actor = actor_new(
        behavior_new(act_symbol_intern, NIL));
    TRACE(fprintf(stderr, "bootstrap_symbol_intern_actor(%p)\n", bootstrap_symbol_intern_actor));
    actor_become(e->actor, behavior_new(act_bootstrap_4, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"lookup\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor, 
        PR(e->actor, "lookup"));

    // bootstrap_vtable_delegated_actor is no longer needed
    TRACE_MEMORY(fprintf(stderr, "FREE behavior(%p)\n", bootstrap_vtable_delegated_actor->behavior));
    bootstrap_vtable_delegated_actor->behavior = FREE(bootstrap_vtable_delegated_actor->behavior);
    TRACE_MEMORY(fprintf(stderr, "FREE actor(%p)\n", bootstrap_vtable_delegated_actor));
    bootstrap_vtable_delegated_actor = FREE(bootstrap_vtable_delegated_actor);

    TRACE(fprintf(stderr, "%s act_bootstrap_3\n", ">>>"));
}

/*
    s_lookup = symbol_intern(0, "lookup"); // complete
    vtable_addMethod(vtable_vt, s_lookup, (oop)vtable_lookup);

    s_lookup_response = symbol_intern(0, "lookup_response"); // begin
*/
void
act_bootstrap_4 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 4));
    Pair message = (Pair)e->message;
    s_lookup_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_lookup_actor(%p)\n", s_lookup_actor));

    bootstrap_vtable_addAction_actor = actor_new(
        behavior_new(act_vtable_addAction, vtable_vt_actor->behavior->context));
    TRACE(fprintf(stderr, "bootstrap_vtable_addAction_actor(%p)\n", bootstrap_vtable_addAction_actor));
    TRACE(fprintf(stderr, "(fast bind) SEND sink_actor(%p), s_lookup_actor(%p), act_vtable_lookup(%p) TO bootstrap_vtable_addAction_actor(%p)\n", &sink_actor, s_lookup_actor, act_vtable_lookup, bootstrap_vtable_addAction_actor));
    config_send(e->sponsor, bootstrap_vtable_addAction_actor,
        PR(&sink_actor, PR(s_lookup_actor, act_vtable_lookup)));
    actor_become(e->actor, behavior_new(act_bootstrap_5, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"lookupResponse\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "lookupResponse"));
    TRACE(fprintf(stderr, "%s act_bootstrap_4\n", ">>>"));
}

/*
    s_lookup_response = symbol_intern(0, "lookup_response"); // complete
  
    s_addMethod = symbol_intern(0, "addMethod"); // begin
*/
void
act_bootstrap_5 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 5));
    Pair message = (Pair)e->message;
    s_lookup_response_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_lookup_response_actor(%p)\n", s_lookup_response_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_6, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"addAction\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "addAction"));
    TRACE(fprintf(stderr, "%s act_bootstrap_5\n", ">>>"));
}

/*
    s_addMethod = symbol_intern(0, "addMethod"); // complete
    vtable_addMethod(vtable_vt, s_addMethod, (oop)vtable_addMethod);

    s_AddMethod_response = symbol_inter(0, "addMethod_response"); // begin
*/
void
act_bootstrap_6 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 6));
    Pair message = (Pair)e->message;
    s_addAction_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_addAction_actor(%p)\n", s_addAction_actor));

    TRACE(fprintf(stderr, "(fast bind) SEND sink_actor(%p), s_addAction_actor(%p), act_vtable_addAction(%p) TO bootstrap_vtable_addAction_actor(%p)\n", &sink_actor, s_addAction_actor, act_vtable_addAction, bootstrap_vtable_addAction_actor));
    config_send(e->sponsor, bootstrap_vtable_addAction_actor,
        PR(&sink_actor, PR(s_addAction_actor, act_vtable_addAction)));
    actor_become(e->actor, behavior_new(act_bootstrap_7, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"addActionResponse\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "addActionResponse"));
    TRACE(fprintf(stderr, "%s act_bootstrap_6\n", ">>>"));
}

/*
    s_AddMethod_response = symbol_inter(0, "addMethod_response"); // complete

    s_allocate = symbol_intern(0, "allocate"); // begin
*/
void
act_bootstrap_7 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 7));
    Pair message = (Pair)e->message;
    s_addActionResponse_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_addActionResponse_actor(%p)\n", s_addActionResponse_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_8, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"allocate\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "allocate"));

    // bootstrap_vtable_addAction_actor is no longer needed
    TRACE_MEMORY(fprintf(stderr, "FREE behavior(%p)\n", bootstrap_vtable_addAction_actor->behavior));
    bootstrap_vtable_addAction_actor->behavior = FREE(bootstrap_vtable_addAction_actor->behavior);
    TRACE_MEMORY(fprintf(stderr, "FREE actor(%p)\n", bootstrap_vtable_addAction_actor));
    bootstrap_vtable_addAction_actor = FREE(bootstrap_vtable_addAction_actor);

    TRACE(fprintf(stderr, "%s act_bootstrap_7\n", ">>>"));
}

/*
    s_allocate = symbol_intern(0, "allocate"); // complete
    send(vtable_vt, s_addMethod, s_allocate, vtable_allocate);

    symbol = send(symbol_vt, s_allocate, sizeof(struct symbol)); // begin
*/
void
act_bootstrap_8 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 8));
    Pair message = (Pair)e->message;
    s_allocate_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_allocate_actor(%p)\n", s_allocate_actor));

    // being using config_dispatch_late_bind()
    TRACE(fprintf(stderr, "(late bind) SEND s_addAction_actor(%p), sink_actor(%p), s_allocate_actor(%p), act_vtable_allocate(%p) TO vtable_vt_actor(%p)\n", s_addAction_actor, &sink_actor, s_allocate_actor, act_vtable_allocate, vtable_vt_actor));
    config_send(e->sponsor, (Actor)vtable_vt_actor,
        PR(s_addAction_actor, 
            PR(&sink_actor,
                PR(s_allocate_actor, act_vtable_allocate))));
    actor_become(e->actor, behavior_new(act_bootstrap_9, NIL));
    TRACE(fprintf(stderr, "(late bind) SEND s_allocate_actor(%p), bootstrap_actor(%p), symbol_vt_actor(%p), 0 TO symbol_vt_actor(%p)\n", s_allocate_actor, e->actor, symbol_vt_actor, symbol_vt_actor));
    config_send(e->sponsor, (Actor)symbol_vt_actor,
        PR(s_allocate_actor,
            PR(e->actor, 
                PR(symbol_vt_actor, 0))));
    TRACE(fprintf(stderr, "%s act_bootstrap_8\n", ">>>"));
}

/*
    symbol = send(symbol_vt, s_allocate, sizeof(struct symbol)); // complete

    s_intern = symbol_intern(0, "intern"); // begin    
*/
void
act_bootstrap_9 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 9));
    Pair message = (Pair)e->message;
    symbol_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "symbol_actor(%p)\n", symbol_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_10, NIL));

    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"intern\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "intern"));
    TRACE(fprintf(stderr, "%s act_bootstrap_9\n", ">>>"));
}

/*
    s_intern = symbol_intern(0, "intern"); // begin    
    send(symbol_vt, s_addMethod, s_intern, symbol_intern);

    s_intern_response = symbol_intern(0, "intern_response"); // begin   
*/
void
act_bootstrap_10 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 10));
    Pair message = (Pair)e->message;
    s_intern_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_intern_actor(%p)\n", s_intern_actor));

    // use config_dispatch_late_bind()
    TRACE(fprintf(stderr, "(late bind) SEND s_addAction_actor(%p), sink_actor(%p), s_intern_actor(%p), act_symbol_intern(%p) TO symbol_vt_actor(%p)\n", s_addAction_actor, &sink_actor, s_intern_actor, act_symbol_intern, symbol_vt_actor));
    config_send(e->sponsor, (Actor)symbol_vt_actor,
        PR(s_addAction_actor,
            PR(&sink_actor,
                PR(s_intern_actor, act_symbol_intern))));
    actor_become(e->actor, behavior_new(act_bootstrap_11, NIL));
    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), \"internResponse\" TO bootstrap_symbol_intern_actor(%p)\n", e->actor, bootstrap_symbol_intern_actor));
    config_send(e->sponsor, bootstrap_symbol_intern_actor,
        PR(e->actor, "internResponse"));
    TRACE(fprintf(stderr, "%s act_bootstrap_10\n", ">>>"));
}

/*
    s_intern_response = symbol_intern(0, "intern_response"); // complete

    s_delegated = send(symbol, s_intern, (oop)"delegated"); // begin
*/
void
act_bootstrap_11 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 11));
    Pair message = (Pair)e->message;
    s_intern_response_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_intern_response_actor(%p)\n", s_intern_response_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_12, NIL));
    TRACE(fprintf(stderr, "(late bind) SEND s_intern_actor(%p), bootstrap_actor(%p), \"delegated\" TO symbol_actor(%p)\n", s_intern_actor, e->actor, symbol_actor));
    config_send(e->sponsor, (Actor)symbol_actor,
        PR(s_intern_actor,
            PR(e->actor, "delegated")));

    // bootstrap_symbol_intern_actor is no longer needed
    TRACE_MEMORY(fprintf(stderr, "FREE behavior(%p)\n", bootstrap_symbol_intern_actor->behavior));
    bootstrap_symbol_intern_actor->behavior = FREE(bootstrap_symbol_intern_actor->behavior);
    TRACE_MEMORY(fprintf(stderr, "FREE actor(%p)\n", bootstrap_symbol_intern_actor));
    bootstrap_symbol_intern_actor = FREE(bootstrap_symbol_intern_actor);

    TRACE(fprintf(stderr, "%s act_bootstrap_11\n", ">>>"));
}

/*
    s_delegated = send(symbol, s_intern, (oop)"delegated"); // complete
    s_delegatedResponse = send(symbol, s_intern, (oop)"delegatedResponse"); // begin
*/
void
act_bootstrap_12 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 12));
    Pair message = (Pair)e->message;
    s_delegated_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_delegated_actor(%p)\n", s_delegated_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_13, NIL));
    TRACE(fprintf(stderr, "(late bind) SEND s_intern_actor(%p), bootstrap_actor(%p), \"delegatedResponse\" TO symbol_actor(%p)\n", s_intern_actor, e->actor, symbol_actor));
    config_send(e->sponsor, (Actor)symbol_actor,
        PR(s_intern_actor,
            PR(e->actor, "delegatedResponse")));
    TRACE(fprintf(stderr, "%s act_bootstrap_12\n", ">>>"));
}

/*
    s_delegatedResponse = send(symbol, s_intern, (oop)"delegatedResponse"); // complete
    send(vtable_vt, s_addMethod, s_delegated, vtable_delegated);
*/
void
act_bootstrap_13 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 13));
    Pair message = (Pair)e->message;
    s_delegatedResponse_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_delegatedResponse_actor(%p)\n", s_delegatedResponse_actor));

    actor_become(e->actor, behavior_new(act_bootstrap_14, NIL));
    TRACE(fprintf(stderr, "(late bind) SEND s_addAction_actor(%p), bootstrap_actor(%p), s_delegated_actor(%p), act_vtable_delegated(%p) TO vtable_vt_actor(%p)\n", s_addAction_actor, e->actor, s_delegated_actor, act_vtable_delegated, vtable_vt_actor));
    config_send(e->sponsor, (Actor)vtable_vt_actor,
        PR(s_addAction_actor,
            PR(e->actor,
                PR(s_delegated_actor, act_vtable_delegated))));
    TRACE(fprintf(stderr, "%s act_bootstrap_13\n", ">>>"));
}

/*
    s_allocateResponse = send(symbol, s_intern, (oop)"allocateResponse"); // begin
*/
void
act_bootstrap_14 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 14));
    actor_become(e->actor, behavior_new(act_bootstrap_15, NIL));
    TRACE(fprintf(stderr, "(late bind) SEND s_intern_actor(%p), bootstrap_actor(%p), \"allocateResponse\" TO symbol_actor(%p)\n", s_intern_actor, e->actor, symbol_actor));
    config_send(e->sponsor, (Actor)symbol_actor,
        PR(s_intern_actor,
            PR(e->actor, "allocateResponse")));
    TRACE(fprintf(stderr, "%s act_bootstrap_14\n", ">>>"));
}

/*
    s_allocateResponse = send(symbol, s_intern, (oop)"allocateResponse"); // complete
*/
void
act_bootstrap_15 (Event e)
{
    TRACE(fprintf(stderr, "*** act_bootstrap_%d\n", 15));
    Pair message = (Pair)e->message;
    s_allocateResponse_actor = (Actor)message->t;
    TRACE(fprintf(stderr, "s_allocateResponse_actor(%p)\n", s_allocateResponse_actor));
    TRACE(fprintf(stderr, "*** bootstrap completed %s\n", "***"));
    TRACE(fprintf(stderr, "%s act_bootstrap_15\n", ">>>"));
}

int
main()
{
    int i;
    Config config = config_new();

    TRACE(fprintf(stderr, "act_ground(%p)\n", act_ground));
    TRACE(fprintf(stderr, "act_symbol(%p)\n", act_symbol));
    TRACE(fprintf(stderr, "act_vtable_addAction(%p)\n", act_vtable_addAction));
    TRACE(fprintf(stderr, "act_vtable_allocate(%p)\n", act_vtable_allocate));
    TRACE(fprintf(stderr, "act_bootstrap_vtable_allocate(%p)\n", act_bootstrap_vtable_allocate));
    TRACE(fprintf(stderr, "act_vtable_delegated(%p)\n", act_vtable_delegated));
    TRACE(fprintf(stderr, "act_vtable_allocate_response(%p)\n", act_vtable_allocate_response));
    TRACE(fprintf(stderr, "act_bootstrap_vtable_delegated(%p)\n", act_bootstrap_vtable_delegated));
    TRACE(fprintf(stderr, "act_lookup_failed_notification(%p)\n", act_lookup_failed_notification));
    TRACE(fprintf(stderr, "act_vtable_lookup(%p)\n", act_vtable_lookup));
    TRACE(fprintf(stderr, "act_symbol_intern(%p)\n", act_symbol_intern));
    TRACE(fprintf(stderr, "act_bootstrap_1(%p)\n", act_bootstrap_1));
    TRACE(fprintf(stderr, "act_bootstrap_2(%p)\n", act_bootstrap_2));
    TRACE(fprintf(stderr, "act_bootstrap_3(%p)\n", act_bootstrap_3));
    TRACE(fprintf(stderr, "act_bootstrap_4(%p)\n", act_bootstrap_4));
    TRACE(fprintf(stderr, "act_bootstrap_5(%p)\n", act_bootstrap_5));
    TRACE(fprintf(stderr, "act_bootstrap_6(%p)\n", act_bootstrap_6));
    TRACE(fprintf(stderr, "act_bootstrap_7(%p)\n", act_bootstrap_7));
    TRACE(fprintf(stderr, "act_bootstrap_8(%p)\n", act_bootstrap_8));
    TRACE(fprintf(stderr, "act_bootstrap_9(%p)\n", act_bootstrap_9));
    TRACE(fprintf(stderr, "act_bootstrap_10(%p)\n", act_bootstrap_10));
    TRACE(fprintf(stderr, "act_bootstrap_11(%p)\n", act_bootstrap_11));
    TRACE(fprintf(stderr, "act_bootstrap_12(%p)\n", act_bootstrap_12));
    TRACE(fprintf(stderr, "act_bootstrap_13(%p)\n", act_bootstrap_13));
    TRACE(fprintf(stderr, "act_bootstrap_14(%p)\n", act_bootstrap_14));
    TRACE(fprintf(stderr, "act_bootstrap_15(%p)\n", act_bootstrap_15));

    Actor bootstrap_actor = actor_new(behavior_new(act_bootstrap_1, NIL));
    TRACE(fprintf(stderr, "bootstrap_actor(%p)\n", bootstrap_actor));

    bootstrap_vtable_delegated_actor = actor_new(
        behavior_new(act_bootstrap_vtable_delegated, NIL));
    TRACE(fprintf(stderr, "bootstrap_vtable_delegated_actor(%p)\n", bootstrap_vtable_delegated_actor));

    TRACE(fprintf(stderr, "(fast bind) SEND bootstrap_actor(%p), 0 TO bootstrap_vtable_delegated_actor(%p)\n", bootstrap_actor, bootstrap_vtable_delegated_actor));
    config_send(config, bootstrap_vtable_delegated_actor, 
        PR(bootstrap_actor, 0));
    while(config_dispatch(config))
        ;
}