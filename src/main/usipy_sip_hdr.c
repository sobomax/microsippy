#include <stddef.h>

#include "usipy_str.h"
#include "usipy_sip_hdr.h"
#include "usipy_sip_hdr_types.h"
#include "usipy_sip_hdr_db.h"

int
usipy_sip_hdr_preparse(struct usipy_sip_hdr *shp)
{
     const struct usipy_hdr_db_entr *hdep;

     if (usipy_str_split(&shp->onwire.full, ':', &shp->onwire.name,
       &shp->onwire.value) != 0)
         return (-1);
     usipy_str_trm_e(&shp->onwire.name);
     hdep = usipy_hdr_db_lookup(&shp->onwire.name);
     if (hdep == NULL)
	 return (-1);
     shp->onwire.hf_type = hdep;
     if (!usipy_hdr_iscmpct(hdep)) {
         shp->hf_type = hdep;
     } else {
         shp->hf_type = usipy_hdr_db_byid(hdep->cantype);
     }
     usipy_str_ltrm_b(&shp->onwire.value);
     usipy_str_ltrm_e(&shp->onwire.value);
     return (0);
}
