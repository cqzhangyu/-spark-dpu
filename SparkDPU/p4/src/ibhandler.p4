action ac_aggregate_ib() {
    add_to_field(meta.ib_0, meta.ib_1);
    add_to_field(meta.ib_2, meta.ib_3);
}
table tab_aggregate_ib {
    actions {
        ac_aggregate_ib;
    }
    default_action : ac_aggregate_ib;
    size : 1;
}

action ac_generate_ib() {
    bit_or(myh.ib, meta.ib_0, meta.ib_2);
}
table tab_generate_ib {
    actions {
        ac_generate_ib;
    }
    default_action : ac_generate_ib;
    size : 1;
}

#define SIZE_SR 32768
register reg_ibsr {
    width : 32;
    instance_count : SIZE_SR;
}
blackbox stateful_alu bb_ibsr_new {
    reg : reg_ibsr;
    update_lo_1_value     : myh.ib;
}
blackbox stateful_alu bb_ibsr_retrans {
    reg : reg_ibsr;
    output_value          : register_lo;
    output_dst            : myh.ib;
}
action ac_ibsr_new() {
    bb_ibsr_new.execute_stateful_alu(meta.temp_addr);
}
action ac_ibsr_retrans() {
    bb_ibsr_retrans.execute_stateful_alu(meta.temp_addr);
}
@pragma stage 11
table tab_ibsr_new {
    actions {
        ac_ibsr_new;
    }
    default_action : ac_ibsr_new;
    size : 1;
}
@pragma stage 11
table tab_ibsr_retrans {
    actions {
        ac_ibsr_retrans;
    }
    default_action : ac_ibsr_retrans;
    size : 1;
}
