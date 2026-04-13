#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "usipy_debug.h"
#include "public/usipy_msg_heap.h"
#include "public/usipy_sip_msg.h"
#include "public/usipy_sip_method_types.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_db.h"
#include "usipy_sip_tm_internal.h"
#include "usipy_sip_tm_priv.h"
#include "usipy_tvpair.h"

static const struct usipy_str *
usipy_sip_tm_dialog_nameaddr_get_param(const struct usipy_sip_hdr_nameaddr *nap,
  const char *name)
{
    const size_t nlen = strlen(name);

    USIPY_DASSERT(nap != NULL);
    USIPY_DASSERT(name != NULL);

    for (int i = 0; i < nap->nparams; i++) {
        const struct usipy_tvpair *pp = &nap->params[i];

        if (pp->token.l != nlen || memcmp(pp->token.s.ro, name, nlen) != 0) {
            continue;
        }
        return (&pp->value);
    }
    return (NULL);
}

static int
usipy_sip_tm_dialog_copy_route_set(struct usipy_msg_heap *mhp,
  struct usipy_str **dstpp, size_t nroutes, const struct usipy_str *srcp)
{
    struct usipy_str *dstp;

    USIPY_DASSERT(mhp != NULL);
    USIPY_DASSERT(dstpp != NULL);
    USIPY_DASSERT(nroutes == 0 || srcp != NULL);

    if (nroutes == 0) {
        *dstpp = NULL;
        return (USIPY_SIP_TM_OK);
    }
    dstp = usipy_msg_heap_alloc(mhp, sizeof(*dstp) * nroutes);
    if (dstp == NULL) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    for (size_t i = 0; i < nroutes; i++) {
        if (usipy_msg_heap_append(mhp, &dstp[i], &srcp[i]) != 0) {
            return (USIPY_SIP_TM_ERR_NOSPC);
        }
    }
    *dstpp = dstp;
    return (USIPY_SIP_TM_OK);
}

static int
usipy_sip_tm_dialog_uri_has_param(const struct usipy_sip_uri *up, const char *name)
{
    const size_t nlen = strlen(name);

    USIPY_DASSERT(up != NULL);
    USIPY_DASSERT(name != NULL);

    for (int i = 0; i < up->nparams; i++) {
        if (up->parameters[i].token.l == nlen &&
          memcmp(up->parameters[i].token.s.ro, name, nlen) == 0) {
            return (1);
        }
    }
    return (0);
}

static void
usipy_sip_tm_dialog_set_target_from_uri(struct usipy_sip_tm_addr *targetp,
  const struct usipy_sip_tm_addr *basep, const struct usipy_sip_uri *urip)
{
    USIPY_DASSERT(targetp != NULL);
    USIPY_DASSERT(basep != NULL);
    USIPY_DASSERT(urip != NULL);

    *targetp = *basep;
    targetp->host = urip->host;
    if (urip->port != 0) {
        targetp->port = urip->port;
        return;
    }
    targetp->port = basep->transport == USIPY_SIP_TM_TRANSPORT_TLS ? 5061u : 5060u;
}

int
usipy_sip_tm_init_in_dialog_request_params(const struct usipy_sip_tm *tm,
  size_t anchor_index, const struct usipy_msg *msg, uint8_t method_type,
  struct usipy_msg_heap *mhp,
  struct usipy_sip_tm_new_in_dialog_transaction_params *outp)
{
    const struct usipy_sip_tm_txi *anchorp;
    const struct usipy_sip_hdr_nameaddr *top, *contactp;
    const struct usipy_str *tagp;
    const struct usipy_str *request_urip;
    const struct usipy_str *target_urip;
    struct usipy_sip_tm_addr next_target;
    struct usipy_str strict_ruri = USIPY_STR_NULL;
    struct usipy_str *owned_routes = NULL;
    struct usipy_str *routes = NULL, *effective_routes = NULL;
    struct usipy_sip_hdr_match *matchp;
    struct usipy_sip_uri *first_route_uri, *target_uri;
    size_t nrecordroutes = 0, neffective_routes = 0;

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(msg != NULL);
    USIPY_DASSERT(mhp != NULL);
    USIPY_DASSERT(outp != NULL);
    USIPY_DASSERT(anchor_index < tm->max_transactions);

    anchorp = &tm->transactions[anchor_index];
    if (!anchorp->active || anchorp->pub.role != USIPY_SIP_TM_ROLE_UAC ||
      anchorp->pub.common.id.method_type != USIPY_SIP_METHOD_INVITE ||
      anchorp->pub.role_data.uac.response_class != 2 ||
      msg->kind != USIPY_SIP_MSG_RES ||
      msg->sline.parsed.sl.status.code < 200 ||
      msg->sline.parsed.sl.status.code > 299) {
        return (USIPY_SIP_TM_ERR_UNSUPPORTED);
    }
    matchp = __builtin_alloca(USIPY_SIP_HDR_MATCH_SIZE(msg->nhdrs));
    *matchp = (struct usipy_sip_hdr_match){.hdrslen = msg->nhdrs};
    if (usipy_sip_msg_parse_hdrs_get((struct usipy_msg *)msg,
      USIPY_HFT_MASK(USIPY_HF_TO) | USIPY_HFT_MASK(USIPY_HF_CONTACT) |
      USIPY_HFT_MASK(USIPY_HF_RECORDROUTE), 0, matchp) != 0) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    top = NULL;
    contactp = NULL;
    for (size_t i = 0; i < matchp->nhdrs; i++) {
        if (matchp->hdrsp[i]->hf_type->cantype == USIPY_HF_TO) {
            if (top == NULL) {
                top = matchp->hdrsp[i]->parsed.to;
            }
        } else if (matchp->hdrsp[i]->hf_type->cantype == USIPY_HF_CONTACT) {
            if (contactp == NULL) {
                contactp = matchp->hdrsp[i]->parsed.contact;
            }
        } else if (matchp->hdrsp[i]->hf_type->cantype == USIPY_HF_RECORDROUTE) {
            nrecordroutes += 1;
        }
    }
    if (top == NULL || contactp == NULL) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    tagp = usipy_sip_tm_dialog_nameaddr_get_param(top, "tag");
    if (tagp == NULL || tagp->l == 0 || contactp->addr_spec.l == 0) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    request_urip = &contactp->addr_spec;
    target_urip = request_urip;
    if (nrecordroutes != 0) {
        size_t rindex = 0;

        routes = __builtin_alloca(sizeof(*routes) * (nrecordroutes + 1));
        for (size_t i = matchp->nhdrs; i > 0; i--) {
            const struct usipy_sip_hdr *shp = matchp->hdrsp[i - 1];

            if (shp->hf_type->cantype != USIPY_HF_RECORDROUTE) {
                continue;
            }
            routes[rindex++] = shp->parsed.recordroute->addr_spec;
        }
        first_route_uri = usipy_sip_uri_parse(mhp, &routes[0]);
        if (first_route_uri == NULL || first_route_uri->host.l == 0) {
            return (USIPY_SIP_TM_ERR_BADMSG);
        }
        if (usipy_sip_tm_dialog_uri_has_param(first_route_uri, "lr")) {
            effective_routes = routes;
            neffective_routes = nrecordroutes;
            target_urip = &effective_routes[0];
        } else {
            const char *qp;

            effective_routes = &routes[1];
            routes[nrecordroutes] = contactp->addr_spec;
            neffective_routes = nrecordroutes;
            strict_ruri = routes[0];
            qp = memchr(strict_ruri.s.ro, '?', strict_ruri.l);
            if (qp != NULL) {
                strict_ruri.l = (size_t)(qp - strict_ruri.s.ro);
            }
            request_urip = &strict_ruri;
            target_urip = request_urip;
        }
    }
    target_uri = usipy_sip_uri_parse(mhp, target_urip);
    if (target_uri == NULL || target_uri->host.l == 0) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    usipy_sip_tm_dialog_set_target_from_uri(&next_target,
      &anchorp->pub.common.peer, target_uri);
    memset(outp, '\0', sizeof(*outp));
    if (usipy_msg_heap_append(mhp, &outp->request_id.call_id,
      &anchorp->cache.call_id) != 0 ||
      usipy_msg_heap_append(mhp, &outp->request_target.request_uri, request_urip) != 0 ||
      usipy_msg_heap_append(mhp, &outp->parties_by_uri.contact,
        &anchorp->cache.uac.contact_uri) != 0 ||
      usipy_msg_heap_append(mhp, &outp->parties_by_uri.from,
        &anchorp->cache.uac.from_uri) != 0 ||
      usipy_msg_heap_append(mhp, &outp->parties_by_uri.to,
        &anchorp->cache.uac.to_uri) != 0 ||
      usipy_msg_heap_append(mhp, &outp->dialog_tags.local_tag,
        &anchorp->cache.from_tag) != 0 ||
      usipy_msg_heap_append(mhp, &outp->dialog_tags.remote_tag, tagp) != 0 ||
      usipy_sip_tm_dialog_copy_route_set(mhp, &owned_routes, neffective_routes,
        effective_routes) != 0) {
        return (USIPY_SIP_TM_ERR_NOSPC);
    }
    outp->route_set.routes = owned_routes;
    outp->route_set.nroutes = neffective_routes;
    outp->request_target.target = next_target;
    outp->request_id.cseq = anchorp->cache.cseq.val;
    outp->request_id.method_type = method_type;
    outp->timers = anchorp->pub.common.timers;
    return (USIPY_SIP_TM_OK);
}

int
usipy_sip_tm_apply_uac_2xx_ack_dialog(const struct usipy_sip_tm *tm,
  size_t anchor_index, const struct usipy_msg *msg, struct usipy_sip_tm_txi *ackp)
{
    struct usipy_sip_tm_new_in_dialog_transaction_params tpp = {0};

    USIPY_DASSERT(tm != NULL);
    USIPY_DASSERT(msg != NULL);
    USIPY_DASSERT(ackp != NULL);

    const int rval = usipy_sip_tm_init_in_dialog_request_params(tm, anchor_index,
      msg, USIPY_SIP_METHOD_ACK, &ackp->scratch, &tpp);

    if (rval != USIPY_SIP_TM_OK) {
        return (rval);
    }
    ackp->cache.uac.request_uri = usipy_sip_uri_parse(&ackp->scratch,
      &tpp.request_target.request_uri);
    if (ackp->cache.uac.request_uri == NULL) {
        return (USIPY_SIP_TM_ERR_BADMSG);
    }
    ackp->cache.uac.from_uri = tpp.parties_by_uri.from;
    ackp->cache.uac.to_uri = tpp.parties_by_uri.to;
    ackp->cache.uac.contact_uri = tpp.parties_by_uri.contact;
    ackp->cache.to_tag = tpp.dialog_tags.remote_tag;
    ackp->cache.uac.routes = (struct usipy_str *)tpp.route_set.routes;
    ackp->cache.uac.nroutes = tpp.route_set.nroutes;
    ackp->pub.common.peer = tpp.request_target.target;
    ackp->outbound.pub.target = tpp.request_target.target;
    ackp->pub.common.outbound = ackp->outbound.pub;
    return (USIPY_SIP_TM_OK);
}
