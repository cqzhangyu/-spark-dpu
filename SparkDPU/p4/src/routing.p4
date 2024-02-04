action ac_set_egr(egress_spec) {
    modify_field(ig_intr_md_for_tm.ucast_egress_port, egress_spec);
}
table forward_dst {
    reads {
        eth.dst : exact;
    }
    actions {
        ac_set_egr; 
        ac_nop;
    }
    default_action : ac_nop;
    size : 64;
}

table forward_src {
    reads {
        meta.ib_0  : exact; 
        meta.ib_1  : exact;
        meta.ib_2  : exact; 
        meta.ib_3  : exact;
        eth.src    : exact;
    }
    actions {
        ac_set_egr; 
        ac_nop;
    }
    default_action : ac_nop;
    size : 64;
}

action ac_swap_src_dst() {
    swap(eth.src, eth.dst);
    swap(myh.sip, myh.dip);
    modify_field(ip4.len, 34);
    modify_field(myh.ptype, ptype_ack);
    remove_header(kv);
}

/*
 * Entry : [0,0,0,0] -> ac_swap_src_dst
 */
table tab_swap_src_dst {
    reads {
        meta.ib_0 : exact; 
        meta.ib_1 : exact;
        meta.ib_2 : exact; 
        meta.ib_3 : exact;
    }
    actions { 
        ac_nop;
        ac_swap_src_dst;  
    }
    default_action : ac_nop;
    size : 2;
}