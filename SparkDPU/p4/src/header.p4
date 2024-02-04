/***************************************************/
/*************** headers' definition ***************/
/***************************************************/

header_type eth_t {
    fields {
        dst            : 48;
        src            : 48;
        proto          : 16;
    }
}

header_type ip4_t { 
    fields {
        ver_ihl_dscp   : 16;
        len            : 16;
        iden           : 16;
        frag           : 16;
        ttl            : 8;
        proto          : 8;
        csum           : 16;
    }
}

#define PROTO_MYH        0xFF
#define ptype_nop        0x00
#define ptype_syn        0x01
#define ptype_fin        0x02
#define ptype_ack        0x03
#define ptype_clr        0x04
#define ptype_empty      0x05
#define ptype_aggr       0x06
#define ptype_query      0x07
#define ptype_aq         0x08
#define ptype_back       0x09
#define ptype_req        0xF0
#define ptype_res        0xF1

header_type myh_t {
    fields {
        sip            : 32;   // ipv4 src
        dip            : 32;   // ipv4 dst
        fid            : 16;
        // reg_idx        : 16;
        ib             : 32;   // indicator bit for short key
        seq            : 32;
        ptype          : 8;  
        fill           : 8;
    }
}

header_type kv_t {
    fields {
        k_00           : 32;
        v_00           : 32;
        k_01           : 32;
        v_01           : 32;
        k_02           : 32;
        v_02           : 32;
        k_03           : 32;
        v_03           : 32;
        k_04           : 32;
        v_04           : 32;
        k_05           : 32;
        v_05           : 32;
        k_06           : 32;
        v_06           : 32;
        k_07           : 32;
        v_07           : 32;
        k_08           : 32;
        v_08           : 32;
        k_09           : 32;
        v_09           : 32;
        k_10           : 32;
        v_10           : 32;
        k_11           : 32;
        v_11           : 32;
        k_12           : 32;
        v_12           : 32;
        k_13           : 32;
        v_13           : 32;
        k_14           : 32;
        v_14           : 32;
        k_15           : 32;
        v_15           : 32;
        k_16           : 32;
        v_16           : 32;
        k_17           : 32;
        v_17           : 32;
        k_18           : 32;
        v_18           : 32;
        k_19           : 32;
        v_19           : 32;
        k_20           : 32;
        v_20           : 32;
        k_21           : 32;
        v_21           : 32;
        k_22           : 32;
        v_22           : 32;
        k_23           : 32;
        v_23           : 32;
        k_24           : 32;
        v_24           : 32;
        k_25           : 32;
        v_25           : 32;
        k_26           : 32;
        v_26           : 32;
        k_27           : 32;
        v_27           : 32;
        k_28           : 32;
        v_28           : 32;
        k_29           : 32;
        v_29           : 32;
        k_30           : 32;
        v_30           : 32;
        k_31           : 32;
        v_31           : 32;
    }
}

header_type metadata_t {
    fields {
        // the address for the incoming packet in the State Evolutive Ring Register (SERR)
        base_addr      : 32;
        // we need use base_addr to index ib register in egress, but it will be re-written by parser (set_metadata)
        // so we copy base_addr to temp_addr in ingress
        temp_addr      : 32;
        serr_even      : 1;
        serr_odd       : 1;

        ib_0           : 32;  // myh.ib & 0x88888888
        ib_1           : 32;  // myh.ib & 0x44444444
        ib_2           : 32;  // myh.ib & 0x22222222
        ib_3           : 32;  // myh.ib & 0x11111111

        ib_0_predicate : 4;
        ib_1_predicate : 4;
        ib_2_predicate : 4;
        ib_3_predicate : 4;

        // legal_predicate: 4;
    }
}

metadata metadata_t meta;

header eth_t eth;
header ip4_t ip4;
header myh_t myh;
header kv_t  kv;



