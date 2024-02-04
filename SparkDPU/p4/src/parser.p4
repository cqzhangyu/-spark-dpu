/***************************************************/
/*************** parser definition ***************/
/***************************************************/

parser start {
    extract(eth);
    return select(latest.proto) {
        0x0800 : parse_ip4;
        default: ingress;
    }
}

parser parse_ip4 { 
    extract(ip4);
    return select(latest.proto) { 
        PROTO_MYH : parse_myh;   
        default: ingress;
    }
}

parser parse_myh {
    extract(myh);
    set_metadata(meta.base_addr, myh.seq);
    return select(myh.ptype) {
        ptype_aggr : parse_kv;
        // ptype_query: parse_kv;
        default    : ingress;
    }
}

parser parse_kv {
    extract(kv);
    // extract(val);
    // extract(key);
    return ingress;
}
