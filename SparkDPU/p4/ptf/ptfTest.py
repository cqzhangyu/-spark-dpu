import logging
import os
import pd_base_tests
import pltfm_pm_rpc
import pal_rpc
import random
import sys
import time
import unittest

from sparkaggr.p4_pd_rpc.ttypes import *
from pltfm_pm_rpc.ttypes import *
from pal_rpc.ttypes import *
from ptf import config
from ptf.testutils import *
from ptf.thriftutils import *
from res_pd_rpc.ttypes import *

this_dir = os.path.dirname(os.path.abspath(__file__))
fp_ports = ["9/0", "10/0", "11/0", "12/0", "13/0", "14/0", "15/0", "16/0", "17/0", "18/0", "19/0"]
# fp_ports = ["1/0","2/0","3/0","4/0","5/0","6/0","7/0","8/0","9/0","10/0","11/0","12/0","13/0","14/0","15/0","16/0","17/0","18/0","19/0","20/0","21/0","22/0","23/0","24/0","25/0","26/0","27/0","28/0","29/0","30/0","31/0","32/0"]
NF_NAME  = "sparkaggr"

class L2Test(pd_base_tests.ThriftInterfaceDataPlane):
    def __init__(self):
        pd_base_tests.ThriftInterfaceDataPlane.__init__(self, [NF_NAME])

    # The setUp() method is used to prepare the test fixture. Typically
    # you would use it to establich connection to the Thrift server.
    #
    # You can also put the initial device configuration there. However,
    # if during this process an error is encountered, it will be considered
    # as a test error (meaning the test is incorrect),
    # rather than a test failure
    def setUp(self):
        # initialize the connection
        pd_base_tests.ThriftInterfaceDataPlane.setUp(self)
        self.sess_hdl = self.conn_mgr.client_init()
        self.dev_tgt = DevTarget_t(0, hex_to_i16(0xFFFF))
        self.devPorts = []

        self.platform_type = "mavericks"
        board_type = self.pltfm_pm.pltfm_pm_board_type_get()
        if re.search("0x0234|0x1234|0x4234|0x5234", hex(board_type)):
            self.platform_type = "mavericks"
        elif re.search("0x2234|0x3234", hex(board_type)):
            self.platform_type = "montara"

        # get the device ports from front panel ports
        try:
            for fpPort in fp_ports:
                port, chnl = fpPort.split("/")
                devPort = \
                    self.pal.pal_port_front_panel_port_to_dev_port_get(0,
                                                                    int(port),
                                                                    int(chnl))
                self.devPorts.append(devPort)

            if test_param_get('setup') == True or (test_param_get('setup') != True
                and test_param_get('cleanup') != True):

                # add and enable the platform ports
                for i in self.devPorts:
                    self.pal.pal_port_add(0, i,
                                        pal_port_speed_t.BF_SPEED_100G,
                                        pal_fec_type_t.BF_FEC_TYP_NONE)
                    self.pal.pal_port_an_set(0, i, 2);
                    self.pal.pal_port_enable(0, i)
                self.conn_mgr.complete_operations(self.sess_hdl)
        except Exception as e:
		print "Some Error in port init"

    def runTest(self):
        print()
        print("*************** Installing table entries ***************")
        self.entries = {} 

        self.entries["tab_init_flow"] = []
        flow_entry = list(range(0, 256))
        SIZE_CWND=2048
        for entry in flow_entry:
            self.entries["tab_init_flow"].append(
                self.client.tab_init_flow_table_add_with_ac_init_flow(self.sess_hdl, self.dev_tgt,
                    sparkaggr_tab_init_flow_match_spec_t(myh_fid=entry),
                    sparkaggr_ac_init_flow_action_spec_t(action_seg_addr=(entry*SIZE_CWND))))

        self.entries["tab_serr_even"] = []
        self.entries["tab_serr_even"].append(self.client.tab_serr_even_table_add_with_ac_serr_even_set(self.sess_hdl, self.dev_tgt, sparkaggr_tab_serr_even_match_spec_t(myh_seq=0x00000000,myh_seq_mask=0x00000800), priority=1))
        self.entries["tab_serr_even"].append(self.client.tab_serr_even_table_add_with_ac_serr_even_clr(self.sess_hdl, self.dev_tgt, sparkaggr_tab_serr_even_match_spec_t(myh_seq=0x00000800,myh_seq_mask=0x00000800), priority=1))

        self.entries["tab_serr_odd"] = []
        self.entries["tab_serr_odd"].append(self.client.tab_serr_odd_table_add_with_ac_serr_odd_set(self.sess_hdl, self.dev_tgt, sparkaggr_tab_serr_odd_match_spec_t(myh_seq=0x00000800,myh_seq_mask=0x00000800), priority=1))
        self.entries["tab_serr_odd"].append(self.client.tab_serr_odd_table_add_with_ac_serr_odd_clr(self.sess_hdl, self.dev_tgt, sparkaggr_tab_serr_odd_match_spec_t(myh_seq=0x00000000,myh_seq_mask=0x00000800), priority=1))

        for loop in range(0, 32):
            ib_idx = loop % 4
            self.entries["tab_ib_" + str(ib_idx) + "_key_s_" + str(loop)] = []

        self.entries["tab_ib_0_key_s_0"].append(self.client.tab_ib_0_key_s_0_table_add_with_ac_ib_0_key_s_0_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_0_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_1"].append(self.client.tab_ib_1_key_s_1_table_add_with_ac_ib_1_key_s_1_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_1_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_2"].append(self.client.tab_ib_2_key_s_2_table_add_with_ac_ib_2_key_s_2_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_2_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_3"].append(self.client.tab_ib_3_key_s_3_table_add_with_ac_ib_3_key_s_3_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_3_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_4"].append(self.client.tab_ib_0_key_s_4_table_add_with_ac_ib_0_key_s_4_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_4_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_5"].append(self.client.tab_ib_1_key_s_5_table_add_with_ac_ib_1_key_s_5_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_5_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_6"].append(self.client.tab_ib_2_key_s_6_table_add_with_ac_ib_2_key_s_6_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_6_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_7"].append(self.client.tab_ib_3_key_s_7_table_add_with_ac_ib_3_key_s_7_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_7_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_8"].append(self.client.tab_ib_0_key_s_8_table_add_with_ac_ib_0_key_s_8_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_8_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_9"].append(self.client.tab_ib_1_key_s_9_table_add_with_ac_ib_1_key_s_9_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_9_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_10"].append(self.client.tab_ib_2_key_s_10_table_add_with_ac_ib_2_key_s_10_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_10_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_11"].append(self.client.tab_ib_3_key_s_11_table_add_with_ac_ib_3_key_s_11_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_11_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_12"].append(self.client.tab_ib_0_key_s_12_table_add_with_ac_ib_0_key_s_12_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_12_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_13"].append(self.client.tab_ib_1_key_s_13_table_add_with_ac_ib_1_key_s_13_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_13_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_14"].append(self.client.tab_ib_2_key_s_14_table_add_with_ac_ib_2_key_s_14_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_14_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_15"].append(self.client.tab_ib_3_key_s_15_table_add_with_ac_ib_3_key_s_15_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_15_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_16"].append(self.client.tab_ib_0_key_s_16_table_add_with_ac_ib_0_key_s_16_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_16_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_17"].append(self.client.tab_ib_1_key_s_17_table_add_with_ac_ib_1_key_s_17_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_17_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_18"].append(self.client.tab_ib_2_key_s_18_table_add_with_ac_ib_2_key_s_18_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_18_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_19"].append(self.client.tab_ib_3_key_s_19_table_add_with_ac_ib_3_key_s_19_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_19_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_20"].append(self.client.tab_ib_0_key_s_20_table_add_with_ac_ib_0_key_s_20_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_20_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_21"].append(self.client.tab_ib_1_key_s_21_table_add_with_ac_ib_1_key_s_21_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_21_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_22"].append(self.client.tab_ib_2_key_s_22_table_add_with_ac_ib_2_key_s_22_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_22_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_23"].append(self.client.tab_ib_3_key_s_23_table_add_with_ac_ib_3_key_s_23_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_23_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_24"].append(self.client.tab_ib_0_key_s_24_table_add_with_ac_ib_0_key_s_24_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_24_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_25"].append(self.client.tab_ib_1_key_s_25_table_add_with_ac_ib_1_key_s_25_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_25_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_26"].append(self.client.tab_ib_2_key_s_26_table_add_with_ac_ib_2_key_s_26_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_26_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_27"].append(self.client.tab_ib_3_key_s_27_table_add_with_ac_ib_3_key_s_27_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_27_match_spec_t(meta_ib_3_predicate=4)))
        self.entries["tab_ib_0_key_s_28"].append(self.client.tab_ib_0_key_s_28_table_add_with_ac_ib_0_key_s_28_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_28_match_spec_t(meta_ib_0_predicate=4)))
        self.entries["tab_ib_1_key_s_29"].append(self.client.tab_ib_1_key_s_29_table_add_with_ac_ib_1_key_s_29_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_29_match_spec_t(meta_ib_1_predicate=4)))
        self.entries["tab_ib_2_key_s_30"].append(self.client.tab_ib_2_key_s_30_table_add_with_ac_ib_2_key_s_30_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_30_match_spec_t(meta_ib_2_predicate=4)))
        self.entries["tab_ib_3_key_s_31"].append(self.client.tab_ib_3_key_s_31_table_add_with_ac_ib_3_key_s_31_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_31_match_spec_t(meta_ib_3_predicate=4)))

        # self.entries["tab_ib_0_key_s_0"].append(self.client.tab_ib_0_key_s_0_table_add_with_ac_ib_0_key_s_0_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_0_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_1"].append(self.client.tab_ib_1_key_s_1_table_add_with_ac_ib_1_key_s_1_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_1_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_2"].append(self.client.tab_ib_2_key_s_2_table_add_with_ac_ib_2_key_s_2_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_2_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_3"].append(self.client.tab_ib_3_key_s_3_table_add_with_ac_ib_3_key_s_3_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_3_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_4"].append(self.client.tab_ib_0_key_s_4_table_add_with_ac_ib_0_key_s_4_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_4_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_5"].append(self.client.tab_ib_1_key_s_5_table_add_with_ac_ib_1_key_s_5_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_5_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_6"].append(self.client.tab_ib_2_key_s_6_table_add_with_ac_ib_2_key_s_6_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_6_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_7"].append(self.client.tab_ib_3_key_s_7_table_add_with_ac_ib_3_key_s_7_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_7_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_8"].append(self.client.tab_ib_0_key_s_8_table_add_with_ac_ib_0_key_s_8_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_8_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_9"].append(self.client.tab_ib_1_key_s_9_table_add_with_ac_ib_1_key_s_9_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_9_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_10"].append(self.client.tab_ib_2_key_s_10_table_add_with_ac_ib_2_key_s_10_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_10_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_11"].append(self.client.tab_ib_3_key_s_11_table_add_with_ac_ib_3_key_s_11_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_11_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_12"].append(self.client.tab_ib_0_key_s_12_table_add_with_ac_ib_0_key_s_12_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_12_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_13"].append(self.client.tab_ib_1_key_s_13_table_add_with_ac_ib_1_key_s_13_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_13_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_14"].append(self.client.tab_ib_2_key_s_14_table_add_with_ac_ib_2_key_s_14_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_14_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_15"].append(self.client.tab_ib_3_key_s_15_table_add_with_ac_ib_3_key_s_15_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_15_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_16"].append(self.client.tab_ib_0_key_s_16_table_add_with_ac_ib_0_key_s_16_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_16_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_17"].append(self.client.tab_ib_1_key_s_17_table_add_with_ac_ib_1_key_s_17_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_17_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_18"].append(self.client.tab_ib_2_key_s_18_table_add_with_ac_ib_2_key_s_18_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_18_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_19"].append(self.client.tab_ib_3_key_s_19_table_add_with_ac_ib_3_key_s_19_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_19_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_20"].append(self.client.tab_ib_0_key_s_20_table_add_with_ac_ib_0_key_s_20_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_20_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_21"].append(self.client.tab_ib_1_key_s_21_table_add_with_ac_ib_1_key_s_21_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_21_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_22"].append(self.client.tab_ib_2_key_s_22_table_add_with_ac_ib_2_key_s_22_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_22_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_23"].append(self.client.tab_ib_3_key_s_23_table_add_with_ac_ib_3_key_s_23_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_23_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_24"].append(self.client.tab_ib_0_key_s_24_table_add_with_ac_ib_0_key_s_24_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_24_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_25"].append(self.client.tab_ib_1_key_s_25_table_add_with_ac_ib_1_key_s_25_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_25_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_26"].append(self.client.tab_ib_2_key_s_26_table_add_with_ac_ib_2_key_s_26_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_26_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_27"].append(self.client.tab_ib_3_key_s_27_table_add_with_ac_ib_3_key_s_27_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_27_match_spec_t(meta_ib_3_predicate=2)))
        # self.entries["tab_ib_0_key_s_28"].append(self.client.tab_ib_0_key_s_28_table_add_with_ac_ib_0_key_s_28_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_0_key_s_28_match_spec_t(meta_ib_0_predicate=2)))
        # self.entries["tab_ib_1_key_s_29"].append(self.client.tab_ib_1_key_s_29_table_add_with_ac_ib_1_key_s_29_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_1_key_s_29_match_spec_t(meta_ib_1_predicate=2)))
        # self.entries["tab_ib_2_key_s_30"].append(self.client.tab_ib_2_key_s_30_table_add_with_ac_ib_2_key_s_30_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_2_key_s_30_match_spec_t(meta_ib_2_predicate=2)))
        # self.entries["tab_ib_3_key_s_31"].append(self.client.tab_ib_3_key_s_31_table_add_with_ac_ib_3_key_s_31_set_0(self.sess_hdl, self.dev_tgt, sparkaggr_tab_ib_3_key_s_31_match_spec_t(meta_ib_3_predicate=2)))

        self.entries["tab_swap_src_dst"] = []
        self.entries["tab_swap_src_dst"].append(
            self.client.tab_swap_src_dst_table_add_with_ac_swap_src_dst(self.sess_hdl, self.dev_tgt,
                sparkaggr_tab_swap_src_dst_match_spec_t(meta_ib_0=0, meta_ib_1=0, meta_ib_2=0, meta_ib_3=0)))

        self.entries["forward_dst"] = []
        routing_entry = [("b8:59:9f:1d:04:f2", 56),
                         ("b8:59:9f:0b:30:72", 48),
                         ("98:03:9b:03:46:50", 40),
                         ("b8:59:9f:02:0d:14", 32),
                         ("b8:59:9f:b0:2d:50", 24),
                         ("b8:59:9f:b0:2b:b0", 16),
                         ("b8:59:9f:b0:2b:b8", 8),
                         ("b8:59:9f:b0:2d:18", 0),
                         ("b8:59:9f:b0:2d:58", 4),
                         ("0c:42:a1:7a:b6:69", 136),
                         ("0c:42:a1:7a:ca:29", 128)]

        for entry in routing_entry:
            self.entries["forward_dst"].append(
                self.client.forward_dst_table_add_with_ac_set_egr(self.sess_hdl, self.dev_tgt,
                    sparkaggr_forward_dst_match_spec_t(eth_dst=macAddr_to_string(entry[0])),
                    sparkaggr_ac_set_egr_action_spec_t(action_egress_spec=entry[1])))

        self.entries["forward_src"] = []
        for entry in routing_entry:
            self.entries["forward_src"].append(
                self.client.forward_src_table_add_with_ac_set_egr(self.sess_hdl, self.dev_tgt,
                    sparkaggr_forward_src_match_spec_t(meta_ib_0=0, meta_ib_1=0, meta_ib_2=0, meta_ib_3=0, eth_src=macAddr_to_string(entry[0])),
                    sparkaggr_ac_set_egr_action_spec_t(action_egress_spec=entry[1])))
        
        # while True:
        #     instruction=raw_input("type stop to finish the task : ")
        #     if instruction == 'stop':
        #         break

        # hw_sync_flag_reg = sparkaggr_register_flags_t(read_hw_sync = True) 
        # hw_sync_flag_cnt = sparkaggr_counter_flags_t(read_hw_sync = True) 
        # key = self.client.register_read_reg_key_row_3(self.sess_hdl, self.dev_tgt, loop, hw_sync_flag_reg)        
        # cnt = self.client.counter_read_cnt_key_row_3(self.sess_hdl, self.dev_tgt, loop, hw_sync_flag_cnt)

    # Use this method to return the DUT to the initial state by cleaning
    # all the configuration and clearing up the connection
    def tearDown(self):
        try:
            print("*************** Clearing table entries ***************")
            # for table in self.entries.keys():
            #     print("TABLE_DEL : " + table)
            #     delete_func = "self.client." + table + "_table_delete"
            #     for entry in self.entries[table]:
            #         exec delete_func + "(self.sess_hdl, 0, entry)"
        except:
            print("Error while cleaning up. ")
            print("You might need to restart the driver")
        finally:
            self.conn_mgr.complete_operations(self.sess_hdl)
            self.conn_mgr.client_cleanup(self.sess_hdl)
            print("Closed Session %d" % self.sess_hdl)
            pd_base_tests.ThriftInterfaceDataPlane.tearDown(self)
