#include <tofino/intrinsic_metadata.p4>
#include <tofino/stateful_alu_blackbox.p4>
#include <tofino/constants.p4>

#include "src/header.p4"
#include "src/parser.p4"
#include "src/routing.p4"
#include "src/seqchecker.p4"
#include "src/aggregator.p4"
#include "src/ibhandler.p4"
// #include "src/query.p4"

/*********************************************************/
/*************** control ingress && egress ***************/
/*********************************************************/
action ac_nop() {
}

control ingress { 

    apply(forward_dst);
    apply(tab_init_flow); 

    if (myh.ptype == ptype_aggr) {   
        // apply(tab_legal);
        apply(tab_serr_even);
        apply(tab_serr_odd);

        // new packet
        if (((myh.seq & 0x0800 == 0) and (meta.serr_even == 0)) or ((myh.seq & 0x0800 == 0x0800) and (meta.serr_odd == 0))) {
            apply(tab_key_0);
            apply(tab_key_1);
            apply(tab_key_2);
            apply(tab_key_3);
            apply(tab_ib_0_key_s_0);
            apply(tab_ib_1_key_s_1);
            apply(tab_ib_2_key_s_2);
            apply(tab_ib_3_key_s_3);

            apply(tab_key_4);
            apply(tab_key_5);
            apply(tab_key_6);
            apply(tab_key_7);
            apply(tab_ib_0_key_s_4);
            apply(tab_ib_1_key_s_5);
            apply(tab_ib_2_key_s_6);
            apply(tab_ib_3_key_s_7); 

            apply(tab_key_8);
            apply(tab_key_9);
            apply(tab_key_10);
            apply(tab_key_11);
            apply(tab_ib_0_key_s_8);
            apply(tab_ib_1_key_s_9);
            apply(tab_ib_2_key_s_10);
            apply(tab_ib_3_key_s_11);

            apply(tab_key_12);
            apply(tab_key_13);
            apply(tab_key_14);
            apply(tab_key_15);
            apply(tab_ib_0_key_s_12);
            apply(tab_ib_1_key_s_13);
            apply(tab_ib_2_key_s_14);
            apply(tab_ib_3_key_s_15); 

            apply(tab_key_16);
            apply(tab_key_17);
            apply(tab_key_18);
            apply(tab_key_19);
            apply(tab_ib_0_key_s_16);
            apply(tab_ib_1_key_s_17);
            apply(tab_ib_2_key_s_18);
            apply(tab_ib_3_key_s_19); 

            apply(tab_key_20);
            apply(tab_key_21);
            apply(tab_key_22);
            apply(tab_key_23);
            apply(tab_ib_0_key_s_20);
            apply(tab_ib_1_key_s_21);
            apply(tab_ib_2_key_s_22);
            apply(tab_ib_3_key_s_23); 

            apply(tab_key_24);
            apply(tab_key_25);
            apply(tab_key_26);
            apply(tab_key_27);
            apply(tab_ib_0_key_s_24);
            apply(tab_ib_1_key_s_25);
            apply(tab_ib_2_key_s_26);
            apply(tab_ib_3_key_s_27); 

            apply(tab_key_28);
            apply(tab_key_29);
            apply(tab_key_30);
            apply(tab_key_31);
            apply(tab_ib_0_key_s_28);
            apply(tab_ib_1_key_s_29);
            apply(tab_ib_2_key_s_30);
            apply(tab_ib_3_key_s_31); 
        } 
        apply(forward_src);
        apply(tab_swap_src_dst);  
    } else if (myh.ptype == ptype_query) {
        // apply(tab_read_key_s_0);
        // apply(tab_read_key_s_1);
        // apply(tab_read_key_s_2);
        // apply(tab_read_key_s_3);
        // apply(tab_read_key_s_4);
        // apply(tab_read_key_s_5);
        // apply(tab_read_key_s_6);
        // apply(tab_read_key_s_7);
        // apply(tab_read_key_s_8);
        // apply(tab_read_key_s_9);
        // apply(tab_read_key_s_10);
        // apply(tab_read_key_s_11);
        // apply(tab_read_key_s_12);
        // apply(tab_read_key_s_13);
        // apply(tab_read_key_s_14);
        // apply(tab_read_key_s_15);
        // apply(tab_read_key_s_16);
        // apply(tab_read_key_s_17);
        // apply(tab_read_key_s_18);
        // apply(tab_read_key_s_19);
        // apply(tab_read_key_s_20);
        // apply(tab_read_key_s_21);
        // apply(tab_read_key_s_22);
        // apply(tab_read_key_s_23);
        // apply(tab_read_key_s_24);
        // apply(tab_read_key_s_25);
        // apply(tab_read_key_s_26);
        // apply(tab_read_key_s_27);
        // apply(tab_read_key_s_28);
        // apply(tab_read_key_s_29);
        // apply(tab_read_key_s_30);
        // apply(tab_read_key_s_31);
    } else if (myh.ptype == ptype_clr) {
        // apply(tab_legal_reset);
        // apply(tab_serr_even_reset);
        // apply(tab_serr_odd_reset);
    }
}

control egress { 
    if (myh.ptype == ptype_aggr) {
        if (((myh.seq & 0x0800 == 0) and (meta.serr_even == 0)) or ((myh.seq & 0x0800 == 0x0800) and (meta.serr_odd == 0))) {
            apply(tab_aggregate_ib);
            apply(tab_generate_ib);
            apply(tab_ibsr_new);
        } else {
            apply(tab_ibsr_retrans);
        }
    }
}
