#include <stddef.h>

#include "usipy_str.h"
#include "usipy_sip_hdr.h"

int
usipy_sip_hdr_preparse(struct usipy_sip_hdr *shp)
{

     if (usipy_str_split(&shp->onwire.full, ':', &shp->onwire.name,
       &shp->onwire.value) != 0)
         return (-1);
     usipy_str_trm_e(&shp->onwire.name);
     usipy_str_ltrm_b(&shp->onwire.value);
     usipy_str_ltrm_e(&shp->onwire.value);
     return (0);
}
