#include "shell_session_peer.h"

static const struct sophia_sf_record *next_event(struct rig *r, uint64_t sequence)
{
    const struct sophia_sf_record *e = NULL;
    unsigned i;
    for (i = 0; i < 8 && sophia_ss_event(&r->s, &e); i++)
        rig_step(r);
    assert(e && e->header.sequence == sequence);
    return e;
}
static const struct sophia_sf_record *fetch(struct rig *r, uint16_t kind, uint64_t generation,
                                            uint64_t qid)
{
    const struct sophia_sf_record *o = NULL;
    unsigned i;
    int status = SOPHIA_9P_BUSY;
    assert(!sophia_ss_object(&r->s, kind, generation, qid));
    for (i = 0; i < 16 && status == SOPHIA_9P_BUSY; i++) {
        rig_step(r);
        status = sophia_ss_object_result(&r->s, &o);
    }
    assert(!status && o && o->header.kind == kind);
    return o;
}
static void ack(struct rig *r, uint64_t sequence)
{
    assert(!sophia_ss_ack(&r->s));
    rig_run(r, 4);
    assert(r->p.acked == sequence && r->s.files.acked_sequence == sequence);
}
/* Negotiation leaves sequences 1 (Submitted) and 2 (Negotiated) consumed. */
static void ack_bounded_by_undelivered_events(void)
{
    struct rig r;
    struct sophia_ss_obligations o;
    rig_ready(&r, &rig_config);
    assert(sophia_ss_ack_limit(&r.s) == 2);
    assert(!sophia_ss_obligations(&r.s, &o) && o.consumed == 2 && !o.acked);
    assert(o.ack_due_ms == 1000 + SOPHIA_SS_ACK_PROGRESS_MS && !o.objects && !o.blocked);
    peer_app_event(&r.p);
    peer_app_event(&r.p);
    next_event(&r, 3);
    /* Delivered but not consumed: not acknowledgeable. */
    assert(sophia_ss_ack_limit(&r.s) == 2);
    ack(&r, 2);
    assert(!sophia_ss_obligations(&r.s, &o) && !o.ack_due_ms);
    assert(!sophia_ss_consume(&r.s));
    assert(sophia_ss_ack_limit(&r.s) == 3);
    next_event(&r, 4);
    assert(sophia_ss_ack_limit(&r.s) == 3);
    ack(&r, 3);
    assert(!sophia_ss_consume(&r.s));
    ack(&r, 4);
    assert(!sophia_ss_ack(&r.s) && !r.s.files.ack_op.active);
    rig_close(&r);
}
static void ack_bounded_by_unfetched_object(void)
{
    struct rig r;
    struct sophia_ss_obligations o;
    const struct sophia_sf_record *object;
    rig_ready(&r, &rig_config);
    r.p.outputs_generation = 4;
    peer_app_event(&r.p);
    peer_published(&r.p, SOPHIA_SF_OUTPUTS, 5, OUTPUTS_QID);
    peer_app_event(&r.p);
    next_event(&r, 3);
    assert(!sophia_ss_consume(&r.s));
    assert(next_event(&r, 4)->header.kind == SOPHIA_SF_OBJECT_PUBLISHED);
    assert(!sophia_ss_consume(&r.s));
    next_event(&r, 5);
    assert(!sophia_ss_consume(&r.s));
    /* Consumed through 5, but the announcement at 4 is owed a fetch. */
    assert(sophia_ss_ack_limit(&r.s) == 3);
    assert(!sophia_ss_obligations(&r.s, &o) && o.consumed == 5 && o.ack_limit == 3);
    assert(o.objects == 1u << (SOPHIA_SF_OUTPUTS - 1) && o.blocked);
    ack(&r, 3);
    /* A fetch that is not the announced object leaves it owed. */
    object = fetch(&r, SOPHIA_SF_OUTPUTS, 0, 0);
    assert(object->value.outputs.facts_generation == 4);
    assert(sophia_ss_ack_limit(&r.s) == 3);
    r.p.outputs_generation = 5;
    object = fetch(&r, SOPHIA_SF_OUTPUTS, 5, OUTPUTS_QID);
    assert(object->value.outputs.facts_generation == 5);
    assert(sophia_ss_ack_limit(&r.s) == 5);
    assert(!sophia_ss_obligations(&r.s, &o) && !o.objects && !o.blocked);
    ack(&r, 5);
    rig_close(&r);
}
/* A newer announcement of a feed must not move the ack bound past the first
 * unfetched one; only fetching the newest object discharges both. */
static void superseded_announcement_keeps_bound(void)
{
    struct rig r;
    const struct sophia_sf_record *object;
    uint64_t sequence;
    rig_ready(&r, &rig_config);
    r.p.outputs_generation = 5;
    peer_published(&r.p, SOPHIA_SF_OUTPUTS, 5, OUTPUTS_QID);
    peer_app_event(&r.p);
    peer_published(&r.p, SOPHIA_SF_OUTPUTS, 6, OUTPUTS_QID);
    peer_app_event(&r.p);
    for (sequence = 3; sequence <= 6; sequence++) {
        next_event(&r, sequence);
        assert(!sophia_ss_consume(&r.s));
    }
    assert(sophia_ss_ack_limit(&r.s) == 2);
    ack(&r, 2);
    object = fetch(&r, SOPHIA_SF_OUTPUTS, 5, OUTPUTS_QID);
    assert(object->value.outputs.facts_generation == 5);
    assert(sophia_ss_ack_limit(&r.s) == 2);
    r.p.outputs_generation = 6;
    object = fetch(&r, SOPHIA_SF_OUTPUTS, 6, OUTPUTS_QID);
    assert(object->value.outputs.facts_generation == 6);
    assert(sophia_ss_ack_limit(&r.s) == 6);
    ack(&r, 6);
    rig_close(&r);
}
static void poll_reports_queued_output(void)
{
    struct rig r;
    struct sophia_sf_record request = rig_request(1);
    uint64_t first;
    rig_ready(&r, &rig_config);
    rig_run(&r, 4);
    assert(sophia_ss_poll_fd(&r.s) == r.fd[0]);
    assert(sophia_ss_poll_events(&r.s) == POLLIN && !sophia_9p_wants_write(&r.s.wire));
    assert(!sophia_ss_submit(&r.s, &request, 1, &first));
    /* Admission queued the transaction write without sending it. */
    assert(sophia_ss_poll_events(&r.s) == (POLLIN | POLLOUT));
    assert(!sophia_ss_dispatch(&r.s, POLLOUT, 65536, r.now));
    assert(sophia_ss_poll_events(&r.s) == POLLIN);
    assert(rig_until(&r, first, 32) == SOPHIA_SS_SUBMITTED);
    /* Quiescent once the transaction is reopened and the events read is held. */
    rig_run(&r, 12);
    assert(r.s.files.submit_stage == 0 && r.p.event_held);
    assert(sophia_ss_poll_events(&r.s) == POLLIN);
    sophia_ss_close(&r.s);
    assert(!sophia_ss_poll_events(&r.s));
    rig_close(&r);
}
static void negotiation_refusal(void)
{
    struct rig r;
    struct sophia_sf_record request = rig_request(1);
    uint16_t reason = 0;
    uint64_t denied = 0, first = 0;
    rig_start(&r, &rig_config);
    r.p.refuse_negotiation = 1;
    assert(sophia_ss_refusal(&r.s, &reason, &denied) == SOPHIA_9P_AGAIN);
    rig_settle(&r);
    assert(sophia_ss_state(&r.s) == SOPHIA_SS_REFUSED_NEGOTIATION);
    assert(!sophia_ss_refusal(&r.s, &reason, &denied) && reason == 4 && denied == 0x10);
    assert(sophia_ss_submit(&r.s, &request, 1, &first) == SOPHIA_9P_INVALID && !first);
    assert(!sophia_ss_epoch(&r.s) && !sophia_ss_limits(&r.s) && !sophia_ss_poll_events(&r.s));
    rig_close(&r);
}
static void outcome_eviction(void)
{
    struct rig r;
    struct sophia_sf_record request = rig_request(1);
    enum sophia_ss_outcome o;
    uint32_t error = 1;
    uint64_t ticket, i;
    rig_ready(&r, &rig_config);
    for (i = 1; i <= SOPHIA_SS_OUTCOMES + 2; i++) {
        assert(!sophia_ss_submit(&r.s, &request, 1, &ticket) && ticket == i);
        assert(rig_until(&r, ticket, 32) == SOPHIA_SS_SUBMITTED);
    }
    /* Evicted tickets report unavailable; they never alias a later one. */
    assert(!sophia_ss_outcome(&r.s, 1, &o, &error) && o == SOPHIA_SS_UNAVAILABLE && !error);
    assert(!sophia_ss_outcome(&r.s, 2, &o, NULL) && o == SOPHIA_SS_UNAVAILABLE);
    assert(!sophia_ss_outcome(&r.s, 3, &o, NULL) && o == SOPHIA_SS_SUBMITTED);
    assert(sophia_ss_outcome(&r.s, SOPHIA_SS_OUTCOMES + 3, &o, NULL) == SOPHIA_9P_ARGUMENT);
    assert(sophia_ss_outcome(&r.s, 0, &o, NULL) == SOPHIA_9P_ARGUMENT);
    rig_close(&r);
}
int main(void)
{
    ack_bounded_by_undelivered_events();
    ack_bounded_by_unfetched_object();
    superseded_announcement_keeps_bound();
    poll_reports_queued_output();
    negotiation_refusal();
    outcome_eviction();
    puts("shell session: ack bounds, object obligations, poll, refusal, outcome ring passed");
    return 0;
}
