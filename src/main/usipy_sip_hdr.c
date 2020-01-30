#include <stddef.h>

#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"

int
usipy_sip_hdr_preparse(struct usipy_sip_hdr **shp)
{
     const struct usipy_hdr_db_entr *hdep;
     struct usipy_sip_hdr *chp;
     int nextra;

     chp = *shp;
     if (usipy_str_split(&chp->onwire.full, ':', &chp->onwire.name,
       &chp->onwire.value) != 0)
         return (-1);
     usipy_str_trm_e(&chp->onwire.name);
     hdep = usipy_hdr_db_lookup(&chp->onwire.name);
     if (hdep == NULL)
	 return (-1);
     chp->onwire.hf_type = hdep;
     if (!usipy_hdr_iscmpct(hdep)) {
         chp->hf_type = hdep;
     } else {
         chp->hf_type = usipy_hdr_db_byid(hdep->cantype);
     }
     nextra = 0;
     if (hdep->flags.csl_allowed) {
         for (struct usipy_str csp = chp->onwire.value; csp.l > 0;) {
             if (usipy_str_split(&csp, ',', &chp->onwire.value, &csp) != 0)
                 break;
             }
             usipy_str_ltrm_b(&chp->onwire.value);
             usipy_str_ltrm_e(&chp->onwire.value);
             nextra += 1;
             chp++;
             chp->hf_type = chp[-1].hf_type;
             chp->onwire.name = chp[-1].onwire.name;
             (*shp)++;
         }
     }
     usipy_str_ltrm_b(&chp->onwire.value);
     usipy_str_ltrm_e(&chp->onwire.value);
     return (nextra);
}
