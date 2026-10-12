bp_sim_main = files(
  '../../src/main.cpp',
)

bp_sim_gtest = files(
  '../../src/bp_gtest.cpp',
  '../../src/gtest/bp_timer_gtest.cpp',
  '../../src/gtest/bp_tcp_gtest.cpp',
  '../../src/gtest/bp_astf_interactive_gtest.cpp',
  '../../src/gtest/tuple_gen_test.cpp',
  '../../src/gtest/client_cfg_test.cpp',
  '../../src/gtest/nat_test.cpp',
  '../../src/gtest/trex_stateless_gtest.cpp',
  '../../src/gtest/bp_tunnel_gtest.cpp',
  '../../src/gtest/bp_dyn_sts_gtest.cpp',
  '../../src/gtest/bp_tunnel_topo_gtest.cpp',
)

main_src = files(
  '../../src/44bsd/tcp_output.cpp',
  '../../src/44bsd/tcp_subr.cpp',
  '../../src/44bsd/flow_table.cpp',
  '../../src/44bsd/tcp_input.cpp',
  '../../src/44bsd/tcp_socket.cpp',
  '../../src/44bsd/tcp_dpdk.cpp',
  '../../src/44bsd/sim_cs_tcp.cpp',
  '../../src/44bsd/sch_rampup.cpp',
  '../../src/44bsd/tick_cmd_clock.cpp',
  '../../src/44bsd/udp.cpp',
  '../../src/hdrh/hdr_time.c',
  '../../src/hdrh/hdr_encoding.c',
  '../../src/hdrh/hdr_histogram.c',
  '../../src/hdrh/hdr_histogram_log.c',
  '../../src/astf/astf_template_db.cpp',
  '../../src/stt_cp.cpp',
  '../../src/dyn_sts.cpp',
  '../../src/bp_sim_tcp.cpp',
  '../../src/inet_pton.cpp',
  '../../src/trex_global.cpp',
  '../../src/trex_modes.cpp',
  '../../src/bp_sim.cpp',
  '../../src/trex_platform.cpp',
  '../../src/bp_sim_stf.cpp',
  '../../src/os_time.cpp',
  '../../src/rx_check.cpp',
  '../../src/tuple_gen.cpp',
  '../../src/platform_cfg.cpp',
  '../../src/tw_cfg.cpp',
  '../../src/rx_check_header.cpp',
  '../../src/nat_check.cpp',
  '../../src/nat_check_flow_table.cpp',
  '../../src/pkt_gen.cpp',
  '../../src/timer_wheel_pq.cpp',
  '../../src/time_histogram.cpp',
  '../../src/msg_manager.cpp',
  '../../src/trex_port_attr.cpp',
  '../../src/publisher/trex_publisher.cpp',
  '../../src/stateful_rx_core.cpp',
  '../../src/flow_stat_parser.cpp',
  '../../src/trex_watchdog.cpp',
  '../../src/trex_client_config.cpp',
  '../../src/pal/linux/pal_utl.cpp',
  '../../src/pal/linux/mbuf.cpp',
  '../../src/pal/common/common_mbuf.cpp',
  '../../src/sim/trex_sim_stateless.cpp',
  '../../src/sim/trex_sim_stateful.cpp',
  '../../src/sim/trex_sim_astf.cpp',
  '../../src/h_timer.cpp',
  '../../src/astf/astf_db.cpp',
  '../../src/astf/astf_json_validator.cpp',
  '../../src/trex_build_info.cpp',
  '../../src/utils/utl_counter.cpp',
  '../../src/utils/utl_cpuu.cpp',
  '../../src/utils/utl_dbl_human.cpp',
  '../../src/utils/utl_ip.cpp',
  '../../src/utils/utl_json.cpp',
  '../../src/utils/utl_mbuf.cpp',
  '../../src/utils/utl_policer.cpp',
  '../../src/utils/utl_port_map.cpp',
  '../../src/utils/utl_sync_barrier.cpp',
  '../../src/utils/utl_yaml.cpp',
  '../../src/tunnels/gtp_man.cpp',
  '../../src/tunnels/tunnel_db.cpp',
)

cmn_src = files(
  '../../src/common/gtest-all.cc',
  '../../src/common/gtest_main.cc',
  '../../src/common/basic_utils.cpp',
  '../../src/common/captureFile.cpp',
  '../../src/common/erf.cpp',
  '../../src/common/pcap.cpp',
  '../../src/common/base64.cpp',
  '../../src/common/sim_event_driven.cpp',
  '../../src/common/n_uniform_prob.cpp',
)

net_src = files(
  '../../src/common/Network/Packet/CPktCmn.cpp',
  '../../src/common/Network/Packet/EthernetHeader.cpp',
  '../../src/common/Network/Packet/IPHeader.cpp',
  '../../src/common/Network/Packet/IPv6Header.cpp',
  '../../src/common/Network/Packet/TCPHeader.cpp',
  '../../src/common/Network/Packet/TCPOptions.cpp',
  '../../src/common/Network/Packet/UDPHeader.cpp',
  '../../src/common/Network/Packet/MacAddress.cpp',
  '../../src/common/Network/Packet/VLANHeader.cpp',
)

tcp_src = files(
  '../../src/44bsd/netinet/tcp_output.c',
  '../../src/44bsd/netinet/tcp_input.c',
  '../../src/44bsd/netinet/tcp_debug.c',
  '../../src/44bsd/netinet/tcp_sack.c',
  '../../src/44bsd/netinet/tcp_timer.c',
  '../../src/44bsd/netinet/tcp_subr.c',
  '../../src/44bsd/netinet/cc/cc_newreno.c',
  '../../src/44bsd/netinet/cc/cc_cubic.c',
)

stx_src = files(
  '../../src/stx/common/trex_stx.cpp',
  '../../src/stx/common/trex_pkt.cpp',
  '../../src/stx/common/trex_rx_packet_parser.cpp',
  '../../src/stx/common/trex_capture.cpp',
  '../../src/stx/common/trex_port.cpp',
  '../../src/stx/common/trex_dp_port_events.cpp',
  '../../src/stx/common/trex_dp_core.cpp',
  '../../src/stx/common/trex_latency_counters.cpp',
  '../../src/stx/common/trex_tpg_stats.cpp',
  '../../src/stx/common/trex_messaging.cpp',
  '../../src/stx/common/trex_owner.cpp',
  '../../src/stx/common/trex_rx_core.cpp',
  '../../src/stx/common/trex_rx_port_mngr.cpp',
  '../../src/stx/common/trex_rx_tx.cpp',
  '../../src/stx/common/trex_stack_base.cpp',
  '../../src/stx/common/trex_stack_counters.cpp',
  '../../src/stx/common/trex_stack_linux_based.cpp',
  '../../src/stx/common/trex_stack_legacy.cpp',
  '../../src/stx/common/trex_rx_rpc_tunnel.cpp',
  '../../src/stx/common/trex_rpc_cmds_common.cpp',
  '../../src/stx/common/trex_vlan_filter.cpp',
  '../../src/stx/common/trex_cmd_mngr.cpp',
)

stf_src = files(
  '../../src/stx/stf/trex_stf.cpp',
)

stateless_src = files(
  '../../src/stx/stl/trex_stl_stream.cpp',
  '../../src/stx/stl/trex_stl_stream_vm.cpp',
  '../../src/stx/stl/trex_stl.cpp',
  '../../src/stx/stl/trex_stl_port.cpp',
  '../../src/stx/stl/trex_stl_streams_compiler.cpp',
  '../../src/stx/stl/trex_stl_vm_splitter.cpp',
  '../../src/stx/stl/trex_stl_dp_core.cpp',
  '../../src/stx/stl/trex_stl_fs.cpp',
  '../../src/stx/stl/trex_stl_messaging.cpp',
  '../../src/stx/stl/trex_stl_tpg.cpp',
  '../../src/stx/stl/trex_stl_rpc_cmds.cpp',
)

astf_batch_src = files(
  '../../src/stx/astf_batch/trex_astf_batch.cpp',
)

md5_src = files(
  '../../external_libs/md5/md5.cpp',
)

astf_src = files(
  '../../src/stx/astf/trex_astf.cpp',
  '../../src/stx/astf/trex_astf_dp_core.cpp',
  '../../src/stx/astf/trex_astf_messaging.cpp',
  '../../src/stx/astf/trex_astf_port.cpp',
  '../../src/stx/astf/trex_astf_rpc_cmds.cpp',
  '../../src/stx/astf/trex_astf_rx_core.cpp',
  '../../src/stx/astf/trex_astf_topo.cpp',
  '../../src/stx/astf/trex_astf_mbuf_redirect.cpp',
)

rpc_server_src = files(
  '../../src/rpc-server/trex_rpc_server.cpp',
  '../../src/rpc-server/trex_rpc_req_resp_server.cpp',
  '../../src/rpc-server/trex_rpc_jsonrpc_v2_parser.cpp',
  '../../src/rpc-server/trex_rpc_cmds_table.cpp',
  '../../src/rpc-server/trex_rpc_cmd.cpp',
  '../../src/rpc-server/trex_rpc_zip.cpp',
)

json_src = files(
  '../../external_libs/json/jsoncpp.cpp',
)

yaml_src = files(
  '../../external_libs/yaml-cpp/src/aliasmanager.cpp',
  '../../external_libs/yaml-cpp/src/binary.cpp',
  '../../external_libs/yaml-cpp/src/contrib/graphbuilder.cpp',
  '../../external_libs/yaml-cpp/src/contrib/graphbuilderadapter.cpp',
  '../../external_libs/yaml-cpp/src/conversion.cpp',
  '../../external_libs/yaml-cpp/src/directives.cpp',
  '../../external_libs/yaml-cpp/src/emitfromevents.cpp',
  '../../external_libs/yaml-cpp/src/emitter.cpp',
  '../../external_libs/yaml-cpp/src/emitterstate.cpp',
  '../../external_libs/yaml-cpp/src/emitterutils.cpp',
  '../../external_libs/yaml-cpp/src/exp.cpp',
  '../../external_libs/yaml-cpp/src/iterator.cpp',
  '../../external_libs/yaml-cpp/src/node.cpp',
  '../../external_libs/yaml-cpp/src/nodebuilder.cpp',
  '../../external_libs/yaml-cpp/src/nodeownership.cpp',
  '../../external_libs/yaml-cpp/src/null.cpp',
  '../../external_libs/yaml-cpp/src/ostream.cpp',
  '../../external_libs/yaml-cpp/src/parser.cpp',
  '../../external_libs/yaml-cpp/src/regex.cpp',
  '../../external_libs/yaml-cpp/src/scanner.cpp',
  '../../external_libs/yaml-cpp/src/scanscalar.cpp',
  '../../external_libs/yaml-cpp/src/scantag.cpp',
  '../../external_libs/yaml-cpp/src/scantoken.cpp',
  '../../external_libs/yaml-cpp/src/simplekey.cpp',
  '../../external_libs/yaml-cpp/src/singledocparser.cpp',
  '../../external_libs/yaml-cpp/src/stream.cpp',
  '../../external_libs/yaml-cpp/src/tag.cpp',
)

stubs = files(
  '../../src/stub/zmq_stub.c',
  '../../src/stub/bpf_stub.c',
)
