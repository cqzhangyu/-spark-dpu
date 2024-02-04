#define SIZE_SERR  1048576 // 0x100000
#define SIZE_FLOW  256

action ac_init_flow(seg_addr) {
    /* 
     * seg_addr = myh.fid << 9;
     *
     * set_metadata(meta.base_addr, myh.seq)
     *
     * SERR ADDR = myh.fid << 9 + myh.seq & 0x1ff
     *           = seg_addr + base_addr & ~0xffe00
     * <==>
     * base_addr <- (base_addr & ~0xffe00) | (seg_addr & 0xffe00) 
     */
    modify_field(meta.base_addr, seg_addr, 0xfffff800);

    bit_and(meta.ib_0, myh.ib, 0x88888888);
    bit_and(meta.ib_1, myh.ib, 0x44444444);
    bit_and(meta.ib_2, myh.ib, 0x22222222);
    bit_and(meta.ib_3, myh.ib, 0x11111111);
}
/*
 * Entry : 
 * +-----------+-----------+
 * |    Match  |  Ac Para  |
 * +-----------+-----------+
 * |    fid    |  fid << 9 |
 * +-----------+-----------+
 */

table tab_init_flow {
    reads {
        myh.fid : exact;
    }
    actions {
        ac_init_flow;
    }
    default_action : ac_nop;
    size : SIZE_FLOW;
}


// register reg_legal {
//     width : 32;
//     instance_count : SIZE_FLOW;
// }
// blackbox stateful_alu bb_legal {
//     reg : reg_legal;
//     condition_lo          : register_lo > myh.seq;
//     condition_hi          : register_lo > myh.seq + 512;
//     update_lo_1_predicate : not condition_lo;
//     update_lo_1_value     : myh.seq;
//     output_value          : predicate;
//     output_dst            : meta.legal_predicate;
// }
// blackbox stateful_alu bb_legal_reset {
//     reg : reg_legal;
//     update_lo_1_value : 0;
// }
// action ac_legal() {
//     bb_legal.execute_stateful_alu(myh.fid);
// }
// table tab_legal {
//     actions {
//         ac_legal;
//     }
//     default_action : ac_legal;
//     size : 1;
// }

// action ac_legal_reset() {
//     bb_legal_reset.execute_stateful_alu(myh.fid);
// }
// table tab_legal_reset {
//     actions {
//         ac_legal_reset;
//     }
//     default_action : ac_legal_reset;
//     size : 1;
// }


register reg_serr_even {
    width : 1;
    instance_count : SIZE_SERR;
}
register reg_serr_odd {
    width : 1;
    instance_count : SIZE_SERR;
}
blackbox stateful_alu bb_serr_even_set {
    reg: reg_serr_even;
    update_lo_1_value     : set_bit;    
    output_value          : alu_lo;
    output_dst            : meta.serr_even;
}
blackbox stateful_alu bb_serr_even_clr {
    reg: reg_serr_even;
    update_lo_1_value     : clr_bit;    
    output_value          : alu_lo;
    output_dst            : meta.serr_even;
}
blackbox stateful_alu bb_serr_odd_set {
    reg: reg_serr_odd;
    update_lo_1_value     : set_bit;
    output_value          : alu_lo;
    output_dst            : meta.serr_odd;
}
blackbox stateful_alu bb_serr_odd_clr {
    reg: reg_serr_odd;
    update_lo_1_value     : clr_bit;
    output_value          : alu_lo;
    output_dst            : meta.serr_odd;
}

action ac_serr_even_set() {
    modify_field(meta.temp_addr, meta.base_addr);
    bb_serr_even_set.execute_stateful_alu(meta.base_addr);
}
action ac_serr_even_clr() {
    modify_field(meta.temp_addr, meta.base_addr);
    bb_serr_even_clr.execute_stateful_alu(meta.base_addr);
}
action ac_serr_odd_set() {
    bb_serr_odd_set.execute_stateful_alu(meta.base_addr);
}
action ac_serr_odd_clr() {
    bb_serr_odd_clr.execute_stateful_alu(meta.base_addr);
}

action ac_serr_even_seen() {
    modify_field(meta.serr_even, 1);
}
action ac_serr_odd_seen()  {
    modify_field(meta.serr_odd,  1);
}

/*
 * entry 1: 1/2, 0x0000 -> ac_serr_even_set
 * entry 2: 1/2, 0x0200 -> ac_serr_even_clr
 */
table tab_serr_even {
    reads {
        myh.seq : ternary; // mask = 0x0200
    }
    actions {
        ac_serr_even_set;
        ac_serr_even_clr;
        ac_serr_even_seen;
    }
    default_action : ac_serr_even_seen;
    size : 5;
}
/*
 * entry 1: 1/2, 0x0000 -> ac_serr_odd_clr
 * entry 2: 1/2, 0x0200 -> ac_serr_odd_set
 */
table tab_serr_odd {
    reads {
        myh.seq : ternary; // mask = 0x0200
    }
    actions {
        ac_serr_odd_set;
        ac_serr_odd_clr;
        ac_serr_odd_seen;
    }
    default_action : ac_serr_odd_seen;
    size : 5;
}

// table tab_serr_even_reset {
//     actions {
//         ac_serr_even_clr;
//     }
//     default_action : ac_serr_even_clr;
//     size : 1;
// }
// table tab_serr_odd_reset {
//     actions {
//         ac_serr_odd_clr;
//     }
//     default_action : ac_serr_odd_clr;
//     size : 1;
// }
