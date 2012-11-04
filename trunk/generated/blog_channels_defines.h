#define BLOG_CHANNEL_server 0
#define BLOG_CHANNEL_client 1
#define BLOG_CHANNEL_flooder 2
#define BLOG_CHANNEL_tun2socks 3
#define BLOG_CHANNEL_ncd 4
#define BLOG_CHANNEL_ncd_var 5
#define BLOG_CHANNEL_ncd_list 6
#define BLOG_CHANNEL_ncd_depend 7
#define BLOG_CHANNEL_ncd_multidepend 8
#define BLOG_CHANNEL_ncd_dynamic_depend 9
#define BLOG_CHANNEL_ncd_concat 10
#define BLOG_CHANNEL_ncd_concatv 11
#define BLOG_CHANNEL_ncd_if 12
#define BLOG_CHANNEL_ncd_strcmp 13
#define BLOG_CHANNEL_ncd_regex_match 14
#define BLOG_CHANNEL_ncd_logical 15
#define BLOG_CHANNEL_ncd_sleep 16
#define BLOG_CHANNEL_ncd_print 17
#define BLOG_CHANNEL_ncd_blocker 18
#define BLOG_CHANNEL_ncd_run 19
#define BLOG_CHANNEL_ncd_runonce 20
#define BLOG_CHANNEL_ncd_daemon 21
#define BLOG_CHANNEL_ncd_spawn 22
#define BLOG_CHANNEL_ncd_call 23
#define BLOG_CHANNEL_ncd_imperative 24
#define BLOG_CHANNEL_ncd_ref 25
#define BLOG_CHANNEL_ncd_index 26
#define BLOG_CHANNEL_ncd_alias 27
#define BLOG_CHANNEL_ncd_process_manager 28
#define BLOG_CHANNEL_ncd_ondemand 29
#define BLOG_CHANNEL_ncd_foreach 30
#define BLOG_CHANNEL_ncd_choose 31
#define BLOG_CHANNEL_ncd_net_backend_waitdevice 32
#define BLOG_CHANNEL_ncd_net_backend_waitlink 33
#define BLOG_CHANNEL_ncd_net_backend_badvpn 34
#define BLOG_CHANNEL_ncd_net_backend_wpa_supplicant 35
#define BLOG_CHANNEL_ncd_net_backend_rfkill 36
#define BLOG_CHANNEL_ncd_net_up 37
#define BLOG_CHANNEL_ncd_net_dns 38
#define BLOG_CHANNEL_ncd_net_iptables 39
#define BLOG_CHANNEL_ncd_net_ipv4_addr 40
#define BLOG_CHANNEL_ncd_net_ipv4_route 41
#define BLOG_CHANNEL_ncd_net_ipv4_dhcp 42
#define BLOG_CHANNEL_ncd_net_ipv4_arp_probe 43
#define BLOG_CHANNEL_ncd_net_watch_interfaces 44
#define BLOG_CHANNEL_ncd_sys_watch_input 45
#define BLOG_CHANNEL_ncd_sys_watch_usb 46
#define BLOG_CHANNEL_ncd_sys_evdev 47
#define BLOG_CHANNEL_ncd_sys_watch_directory 48
#define BLOG_CHANNEL_StreamPeerIO 49
#define BLOG_CHANNEL_DatagramPeerIO 50
#define BLOG_CHANNEL_BReactor 51
#define BLOG_CHANNEL_BSignal 52
#define BLOG_CHANNEL_FragmentProtoAssembler 53
#define BLOG_CHANNEL_BPredicate 54
#define BLOG_CHANNEL_ServerConnection 55
#define BLOG_CHANNEL_Listener 56
#define BLOG_CHANNEL_DataProto 57
#define BLOG_CHANNEL_FrameDecider 58
#define BLOG_CHANNEL_BSocksClient 59
#define BLOG_CHANNEL_BDHCPClientCore 60
#define BLOG_CHANNEL_BDHCPClient 61
#define BLOG_CHANNEL_NCDIfConfig 62
#define BLOG_CHANNEL_BUnixSignal 63
#define BLOG_CHANNEL_BProcess 64
#define BLOG_CHANNEL_PRStreamSink 65
#define BLOG_CHANNEL_PRStreamSource 66
#define BLOG_CHANNEL_PacketProtoDecoder 67
#define BLOG_CHANNEL_DPRelay 68
#define BLOG_CHANNEL_BThreadWork 69
#define BLOG_CHANNEL_DPReceive 70
#define BLOG_CHANNEL_BInputProcess 71
#define BLOG_CHANNEL_NCDUdevMonitorParser 72
#define BLOG_CHANNEL_NCDUdevMonitor 73
#define BLOG_CHANNEL_NCDUdevCache 74
#define BLOG_CHANNEL_NCDUdevManager 75
#define BLOG_CHANNEL_BTime 76
#define BLOG_CHANNEL_BEncryption 77
#define BLOG_CHANNEL_SPProtoDecoder 78
#define BLOG_CHANNEL_LineBuffer 79
#define BLOG_CHANNEL_BTap 80
#define BLOG_CHANNEL_lwip 81
#define BLOG_CHANNEL_NCDConfigTokenizer 82
#define BLOG_CHANNEL_NCDConfigParser 83
#define BLOG_CHANNEL_NCDValParser 84
#define BLOG_CHANNEL_nsskey 85
#define BLOG_CHANNEL_addr 86
#define BLOG_CHANNEL_PasswordListener 87
#define BLOG_CHANNEL_NCDInterfaceMonitor 88
#define BLOG_CHANNEL_NCDRfkillMonitor 89
#define BLOG_CHANNEL_udpgw 90
#define BLOG_CHANNEL_UdpGwClient 91
#define BLOG_CHANNEL_SocksUdpGwClient 92
#define BLOG_CHANNEL_BNetwork 93
#define BLOG_CHANNEL_BConnection 94
#define BLOG_CHANNEL_BSSLConnection 95
#define BLOG_CHANNEL_BDatagram 96
#define BLOG_CHANNEL_PeerChat 97
#define BLOG_CHANNEL_BArpProbe 98
#define BLOG_CHANNEL_NCDModuleIndex 99
#define BLOG_CHANNEL_NCDModuleProcess 100
#define BLOG_CHANNEL_NCDValGenerator 101
#define BLOG_CHANNEL_ncd_from_string 102
#define BLOG_CHANNEL_ncd_to_string 103
#define BLOG_CHANNEL_ncd_value 104
#define BLOG_CHANNEL_ncd_try 105
#define BLOG_CHANNEL_ncd_sys_request_server 106
#define BLOG_CHANNEL_NCDRequest 107
#define BLOG_CHANNEL_ncd_net_ipv6_wait_dynamic_addr 108
#define BLOG_CHANNEL_NCDRequestClient 109
#define BLOG_CHANNEL_ncd_request 110
#define BLOG_CHANNEL_ncd_sys_request_client 111
#define BLOG_CHANNEL_ncd_exit 112
#define BLOG_CHANNEL_ncd_getargs 113
#define BLOG_CHANNEL_ncd_arithmetic 114
#define BLOG_CHANNEL_ncd_parse 115
#define BLOG_CHANNEL_ncd_valuemetic 116
#define BLOG_CHANNEL_ncd_file 117
#define BLOG_CHANNEL_ncd_netmask 118
#define BLOG_CHANNEL_ncd_implode 119
#define BLOG_CHANNEL_ncd_call2 120
#define BLOG_CHANNEL_ncd_assert 121
#define BLOG_CHANNEL_ncd_reboot 122
#define BLOG_CHANNEL_ncd_explode 123
#define BLOG_CHANNEL_NCDPlaceholderDb 124
#define BLOG_CHANNEL_NCDVal 125
#define BLOG_CHANNEL_ncd_net_ipv6_addr 126
#define BLOG_CHANNEL_ncd_net_ipv6_route 127
#define BLOG_CHANNEL_ncd_net_ipv4_addr_in_network 128
#define BLOG_CHANNEL_ncd_net_ipv6_addr_in_network 129
#define BLOG_CHANNEL_dostest_server 130
#define BLOG_CHANNEL_dostest_attacker 131
#define BLOG_CHANNEL_ncd_timer 132
#define BLOG_CHANNEL_ncd_file_open 133
#define BLOG_CHANNEL_ncd_backtrack 134
#define BLOG_CHANNEL_ncd_socket 135
#define BLOG_NUM_CHANNELS 136
