set(src_microsippy
 "usipy_sip_tm.c" "usipy_sip_tm.h"
 "public/usipy_str.h" "public/usipy_sip_hdr_types.h" "public/usipy_sip_msg.h" "usipy_sip_msg.c"
 "usipy_msg_heap.c" "public/usipy_msg_heap.h" "usipy_str.c" "usipy_sip_hdr.c"
 "usipy_sip_hdr.h" "usipy_fast_parser.c" "usipy_fast_parser.h"
 "usipy_sip_hdr_db.c" "usipy_sip_hdr_db.h" "usipy_sip_sline.c"
 "public/usipy_sip_sline.h" "usipy_misc.c" "usipy_misc.h" "usipy_sip_method_db.c"
 "usipy_sip_method_db.h" "public/usipy_sip_method_types.h" "usipy_sip_hdr_cseq.c"
 "usipy_sip_hdr_cseq.h" "usipy_sip_hdr_via.c"
 "usipy_sip_hdr_via.h" "usipy_types.h" "usipy_sip_uri.c" "usipy_sip_uri.h"
 "usipy_sip_req.c" "usipy_sip_req.h" "usipy_debug.h" "usipy_msg_heap_rb.h"
 "usipy_msg_heap_inl.h" "usipy_sip_hdr_db_pdata.h" "usipy_sip_hdr_onetoken.c"
 "usipy_sip_hdr_onetoken.h" "usipy_sip_hdr_nameaddr.c" "usipy_sip_hdr_nameaddr.h"
 "usipy_sip_tid.c" "usipy_sip_tid.h" "usipy_sip_res.c" "usipy_sip_res.h"
 "bits/turbocompare.h" "bits/endian.h"
)
add_library(microsippy STATIC ${src_microsippy})

target_include_directories(microsippy PRIVATE
  "."
)
