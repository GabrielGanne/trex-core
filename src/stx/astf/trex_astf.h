/*
 Itay Marom
 Cisco Systems, Inc.
*/

/*
Copyright (c) 2015-2015 Cisco Systems, Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#ifndef __TREX_ASTF_H__
#define __TREX_ASTF_H__

#include <stdint.h>
#include <vector>
#include <map>

#include "common/trex_stx.h"
#include "trex_astf_defs.h"
#include "stt_cp.h"

#include "tunnels/tunnel_factory.h"

class TrexAstfPort;
class CSyncBarrier;
class CRxAstfCore;
class TrexAstf;
class TrexCpToDpMsgBase;

#define MAC_ADDR_MAX 1000
#define IPV4_ADDR_MAX 1000
typedef std::unordered_map<uint8_t, TrexAstfPort*> astf_port_map_t;
typedef std::string cp_profile_id_t;


typedef enum trex_astf_hash {
    HASH_OK,
    HASH_ON_SAME_PROFILE,
    HASH_ON_OTHER_PROFILE,
} trex_astf_hash_e;

class AstfProfileState {
public:
    enum state_e {
        STATE_IDLE,
        STATE_LOADED,
        STATE_PARSE, // by DP core 0, not cancelable
        STATE_BUILD, // by DP cores, not cancelable
        STATE_TX,
        STATE_CLEANUP, // by DP cores, not cancelable
        STATE_DELETE,  // by DP core 0, not cancelable
        AMOUNT_OF_STATES,
    };
};

/***********************************************************
 * TRex ASTF each profile in interactive
 ***********************************************************/
class TrexAstfPerProfile : public AstfProfileState {
public:
    typedef std::vector<state_e> states_t;

    TrexAstfPerProfile(TrexAstf* astf_obj,
                       uint32_t dp_profile_id=0,
                       cp_profile_id_t cp_profile_id=DEFAULT_ASTF_PROFILE_ID);
    ~TrexAstfPerProfile();

    /**
     * Profile-related:
     * cmp - compare hash to loaded profile, true = matches (heavy profile will not be uploaded twice)
     * init - clears profile JSON string (if any)
     * append - appends fragment to profile JSON string
     * loaded - move state from idle to loaded
     */
    trex_astf_hash_e profile_cmp_hash(const std::string &hash);
    void profile_init();
    void profile_append(const std::string &fragment);
    void profile_set_loaded();

    /**
     * Handle profile status
     */
    void profile_change_state(state_e new_state);
    void profile_check_whitelist_states(const states_t &whitelist);
    bool is_profile_state_build();

    bool profile_needs_parsing();
    bool is_error() {
        return m_error.size() > 0;
    }

    /*
     * involved message between CP and DP
     */
    void parse();
    void build();
    void transmit();
    void cleanup();
    void all_dp_cores_finished(bool partial=false);
    void dp_core_finished();
    void dp_core_finished_partial();
    void dp_core_error(const std::string &err);

    void add_dp_profile_ctx(CPerProfileCtx* client, CPerProfileCtx* server);

    /*
     * publish event for each profile
     */
    void publish_astf_profile_state();
    void publish_astf_profile_clear();

    /*
     * For private variables
     */
    int32_t get_dp_profile_id() { return m_dp_profile_id; }
    state_e get_profile_state() { return m_profile_state; }
    bool    get_profile_stopping() { return m_profile_stopping; }
    bool    get_nc_flow_close() { return m_nc_flow_close; }
    double  get_duration() { return m_duration; }
    double  get_factor() { return m_factor; }
    CSTTCp* get_stt_cp() { return m_stt_cp; }

    void    set_profile_stopping(bool stopping) { m_profile_stopping = stopping; }
    void    set_nc_flow_close(bool nc) { m_nc_flow_close = nc; }
    void    set_duration(double duration) { m_duration = duration; }
    void    set_establish_timeout(double timeout) { m_establish_timeout = timeout; }
    void    set_terminate_duration(double duration) { m_terminate_duration = duration; }
    void    set_factor(double mult) { m_factor = mult; }

    /*
     * clear statistics counter
     */
    void    clear_counters(bool epoch_increment=true) { m_stt_cp->clear_counters(epoch_increment); }

private:
    state_e         m_profile_state;
    std::string     m_profile_buffer;
    std::string     m_profile_hash;
    bool            m_profile_parsed;
    bool            m_profile_stopping;

    int16_t         m_active_cores;
    int16_t         m_partial_cores;
    bool            m_nc_flow_close;
    double          m_duration;
    double          m_establish_timeout;
    double          m_terminate_duration;
    double          m_factor;
    std::string     m_error;

    int32_t         m_dp_profile_id;
    cp_profile_id_t m_cp_profile_id;

    CSTTCp*         m_stt_cp;
    TrexAstf*       m_astf_obj;
};


/***********************************************************
 * TRex ASTF dynamic multiple profiles in interactive
 ***********************************************************/
class TrexAstfProfile : public AstfProfileState {
public:
    typedef std::vector<state_e> states_t;
    Json::Value m_entry;

    TrexAstfProfile();
    ~TrexAstfProfile();

    /**
     * Handle dynamic multiple profiles
     */
    void add_profile(cp_profile_id_t profile_id);
    bool delete_profile(cp_profile_id_t profile_id);
    bool is_valid_profile(cp_profile_id_t profile_id);
    uint32_t get_num_profiles();
    std::unordered_map<std::string, TrexAstfPerProfile *> get_profile_list() {
        return m_profile_list;
    }

    TrexAstfPerProfile* get_profile(cp_profile_id_t profile_id);
    cp_profile_id_t get_profile_id(uint32_t dp_profile_id);

    std::vector<std::string> get_profile_id_list();
    std::vector<state_e>     get_profile_state_list();
    std::vector<CSTTCp *>    get_sttcp_list();
    std::vector<CSTTCp *>    get_sttcp_total_list();

    std::vector<std::string> m_states_names;
    std::vector<uint32_t>    m_states_cnt;

    bool is_another_profile_transmitting(cp_profile_id_t profile_id);
    bool is_safe_update_stats();

    void clear_stats();
    void Accumulate_stopped(bool clear, bool calculate, CSTTCp *lpstt);
    void Accumulate_total(bool clear, bool calculate,  CSTTCp *lpstt);
    CSTTCp* m_stt_stopped_cp;
    CSTTCp* m_stt_total_cp;
protected:
    std::unordered_map<std::string, TrexAstfPerProfile *> m_profile_list;
    std::unordered_map<uint32_t, cp_profile_id_t> m_profile_id_map;

private:
    uint32_t m_dp_profile_last_id;
};


/***********************************************************
 * TRex adavanced stateful interactive object
 ***********************************************************/
class TrexAstf : public TrexAstfProfile, public TrexSTX {
public:
    enum state_latency_e {
        STATE_L_IDLE,
        STATE_L_WORK,
        AMOUNT_OF_L_STATES,
    };

    /** 
     * create ASTF object 
     */
    TrexAstf(const TrexSTXCfg &cfg);
    virtual ~TrexAstf();

    /**
     * ASTF control plane
     * 
     */
    void launch_control_plane();

    /**
     * shutdown ASTF
     * 
     */
    void shutdown(bool post_shutdown);

    void dp_core_finished(int thread_id, uint32_t dp_profile_id);
    void dp_core_finished_partial(int thread_id, uint32_t dp_profile_id);

    void dp_core_state(int thread_id, int state);

    /**
     * async data sent for ASTF
     * 
     */
    void publish_async_data();

    /**
     * create a ASTF DP core
     * 
     */
    TrexDpCore *create_dp_core(uint32_t thread_id, CFlowGenListPerThread *core);

    /**
     * fetch astf port object by port ID
     *
     */
    TrexAstfPort *get_port_by_id(uint8_t port_id);

    /**
     * returns the port list as astf port objects
     */
    const astf_port_map_t &get_port_map() const {
        return *(astf_port_map_t *)&m_ports;
    }

    /**
     * Acquire context (and all ports)
     */
    void acquire_context(const std::string &user, bool force);

    /**
     * Release context (and all ports)
     */
    void release_context();

    /**
     * Topology-related:
     * cmp - compare hash to loaded topo, true = matches (heavy topo will not be uploaded twice)
     * clear - clears topo JSON string (if any)
     * append - appends fragment to topo JSON string
     * loaded - mark that topo JSON string was fully loaded
     */
    bool topo_cmp_hash(const std::string &hash);
    void topo_clear();
    void topo_append(const std::string &fragment);
    void topo_set_loaded();
    void topo_get(Json::Value &obj);

    bool tunnel_topo_cmp_hash(const std::string &hash);
    void tunnel_topo_clear();
    void tunnel_topo_append(const std::string &fragment);
    void tunnel_topo_set_loaded();
    void tunnel_topo_get(Json::Value &obj);
    bool tunnel_topo_needs_parsing();
    std::string* get_tunnel_topo_buffer() { return &m_tunnel_topo_buffer; }
    void         set_tunnel_topo_parsed(bool topo) { m_tunnel_topo_parsed = topo; }
    bool         is_tunnel_topo_parsed() { return m_tunnel_topo_parsed; }
    /**
     * Stats Related
     */
    bool is_state_build();

    /**
     * Start transmit
     */
    void start_transmit(cp_profile_id_t profile_id, const start_params_t &args);

    /**
     * Stop transmit
     */
    void stop_transmit(cp_profile_id_t profile_id);
    void stop_transmit_final(cp_profile_id_t profile_id);

    /**
     * Clear profile
     */
    void profile_clear(cp_profile_id_t profile_id);

    /**
     * Update traffic rate
     */
    void update_rate(cp_profile_id_t profile_id, double mult);

    TrexOwner& get_owner() {
        return m_owner;
    }

    /**
     * Start transmit latency streams only 
     */
    void start_transmit_latency(const lat_start_params_t &args, CTunnelsTopo* tunnel_topo=nullptr, CTunnelsDB* tunnel_db=nullptr);

    /**
     * Stop transmit latency streams only 
     */
    bool stop_transmit_latency();


    /**
     * Get latency stats
     */
    void get_latency_stats(Json::Value & obj);

    /**
     * update latency rate command
     */
    void update_latency_rate(double mult);

    /**
     * start latency traffic if latency option is available
     */
    std::string handle_start_latency(int32_t dp_profile_id);

    /**
     * stop latency traffic if latency option is available
     */
    void handle_stop_latency();

    CSyncBarrier* get_barrier() {
        return m_sync_b;
    }

    void dp_core_error(int thread_id, uint32_t dp_profile_id, const std::string &err);

    void add_dp_profile_ctx(uint32_t dp_profile_id, void* client, void* server) override;

    state_e get_state() {
        return m_state;
    }

    uint64_t get_epoch() {
        return m_epoch;
    }

    void inc_epoch(bool force = false);

    CRxAstfCore * get_rx(){
        assert(m_rx);
        return((CRxAstfCore *)m_rx);
    }

    bool topo_needs_parsing();

    bool topo_exists() {
        return  m_topo_parsed;
    }

    /**
     * update_astf_state          : update state for all profiles
     * publish_astf_state         : publish state for all profiles
     * get_profiles_status        : get JSON value list for all profiles id
     */
    void update_astf_state();
    void publish_astf_state();
    void get_profiles_status(Json::Value &result);

    void set_barrier(double timeout_sec);

    void set_service_mode(bool enabled, bool filtered, uint8_t mask);

    void send_message_to_dp(uint8_t core_id, TrexCpToDpMsgBase *msg, bool clone = false);
    void send_message_to_all_dp(TrexCpToDpMsgBase *msg, bool suspend = false);
    bool is_trans_state();

    std::string* get_topo_buffer() { return &m_topo_buffer; }
    void         set_topo_parsed(bool topo) { m_topo_parsed = topo; }

    void stop_dp_scheduler();
    bool is_dp_core_state(int state, bool any = false);

    ClientCfgDB * get_client_db() {
        return &m_fl->m_client_config_info;
    }

    void activate_tunnel_handler(bool activate, uint8_t type) {
        if (activate) {
            m_tunnel_handler = create_tunnel_handler(type, (uint8_t)(TUNNEL_MODE_CP));
        } else {
            delete m_tunnel_handler;
            m_tunnel_handler = nullptr;
        }
        tunnel_topo_clear();
    }

    void set_clients_info_ready(bool ready) {
        m_is_clients_info_ready = ready;
    }

    bool get_clients_info_ready() {
        return m_is_clients_info_ready;
    }

    void add_clients_info(Json::Value &clients_info) {
        m_clients_info.push_back(clients_info);
    }

    bool get_clients_info(Json::Value &clients_info);

    CTunnelHandler *get_tunnel_handler(){
        return m_tunnel_handler;
    }
    CCpDynStsInfra* get_cp_sts_infra();
    vector<CSTTCp *> get_dyn_sttcp_list();
    void insert_ignored_mac_addresses(std::vector<uint64_t>& mac_addresses);
    void get_ignored_mac_addresses(std::vector<uint64_t>& mac_addresses);
    int get_ignored_macs_size();
    void insert_ignored_ip_addresses(std::vector<uint32_t>& ip_addresses);
    void get_ignored_ip_addresses(std::vector<uint32_t>& ip_addresses);
    int get_ignored_ips_size();

protected:
    void change_state(state_e new_state);
    void check_whitelist_states(const states_t &whitelist);
    //void check_blacklist_states(const states_t &blacklist);

    void ports_report_state(state_e state);

    state_latency_e              m_l_state;
    uint32_t                     m_latency_pps;
    bool                         m_lat_with_traffic;
    int32_t                      m_lat_profile_id;
    TrexOwner                    m_owner;
    state_e                      m_state;
    std::vector<Json::Value>     m_clients_info;
    std::unordered_set<uint64_t> m_ignored_macs;
    std::unordered_set<uint32_t> m_ignored_ips;
    CSyncBarrier                *m_sync_b;
    CFlowGenList                *m_fl;
    CParserOption               *m_opts;
    std::string                  m_topo_buffer;
    std::string                  m_topo_hash;
    bool                         m_topo_parsed;
    std::string                  m_tunnel_topo_buffer;
    std::string                  m_tunnel_topo_hash;
    bool                         m_tunnel_topo_parsed;
    bool                         m_is_clients_info_ready;
    uint64_t                     m_epoch;
    CTunnelHandler              *m_tunnel_handler;
    CCpDynStsInfra              *m_cp_sts_infra;

public:
    bool                m_stopping_dp;
    std::vector<int>    m_dp_states;
    std::vector<TrexCpToDpMsgBase*> m_suspended_msgs;
};

static inline TrexAstf * get_astf_object() {
    return (TrexAstf *)get_stx();
}


#endif /* __TREX_STL_H__ */

