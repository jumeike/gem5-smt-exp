// This file containes the necessary routines to initialize
// and work with DPDK. It's written in C to be able to link with
// any application without issues and any extra steps.
//
// The implementation enables zero-copy networking.

#ifndef _DPDK_H_
#define _DPDP_H_

#include <assert.h>

#include <rte_config.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_mbuf.h>
#include <rte_pdump.h>


// AMIN
// #include <memcached_client.h>
#include <zipfian_int_distribution.h>
#include <helpers.h>
#include <csignal>
#include <iostream>
#include <map>
#include <random>
#include <tuple>
#include <vector>

typedef std::vector<std::vector<uint8_t>> DSet;

// Global DPDK configs.
static const char *kPacketMemPoolName = "dpdk_packet_mem_pool";

#define kRingN 1
#define kRingDescN 2048
#define kMTUStandardFrames 1500
#define kMTUJumboFrames 9000
#define kLinkTimeOut_ms 100
#define kMaxBurstSize 1

// Main DPDK struct.
struct DPDKObj {
  // Main mem pool for this DPDK object.
  struct rte_mempool *`mpool;

  // Some port configs and parameters.
  uint16_t pmd_port_cnt;
  uint16_t pmd_ports[RTE_MAX_ETHPORTS];
  struct rte_ether_addr pmd_eth_addrs[RTE_MAX_ETHPORTS];
  uint16_t pmd_port_to_use; // This port will be used throughout this object.

  // TX and RX buffers.
  uint16_t rx_burst_size;
  uint16_t rx_burst_ptr;
  struct rte_mbuf *rx_mbufs[kMaxBurstSize];
  uint16_t tx_burst_size;
  uint16_t tx_burst_ptr;
  struct rte_mbuf *tx_mbufs[kMaxBurstSize];
};

// Initialize DPDK; it returns pmd ports to be used for
// later communication.
static int InitDPDK(struct DPDKObj *dpdk_obj) {
  assert(dpdk_obj != NULL);

  const size_t kDpdkArgcMax = 16;
  int dargv_cnt = 0;
  char *dargv[kDpdkArgcMax];
  dargv[dargv_cnt++] = (char *)"-l";
  dargv[dargv_cnt++] = (char *)"0";
  dargv[dargv_cnt++] = (char *)"-n";
  dargv[dargv_cnt++] = (char *)"1";
  dargv[dargv_cnt++] = (char *)"--proc-type";
  dargv[dargv_cnt++] = (char *)"auto";

  int ret = rte_eal_init(dargv_cnt, dargv);
  if (ret < 0) {
    fprintf(stderr, "Failed to initialize DPDK.\n");
    return -1;
  }
  fprintf(stderr, "EAL is initialized!\n");

  
  ret = rte_pdump_init();
  if (ret) {
    fprintf(stderr, "Failed to initialize pdump.\n");
    return -1;
  }
  fprintf(stderr, "pdump initialized.\n");

  // Look-up NICs.
  int p_num = rte_eth_dev_count_avail();
  if (p_num == 0) {
    fprintf(stderr, "No suitable NICs found; check driver binding and DPDK "
                    "linking options\n");
    return -1;
  }

  // Print MAC address for each valid port.
  fprintf(stderr, "Found %d NIC ports:\n", p_num);
  dpdk_obj->pmd_port_cnt = 0;
  for (uint16_t i = 0; i < RTE_MAX_ETHPORTS; ++i) {
    if (rte_eth_dev_is_valid_port(i)) {
      dpdk_obj->pmd_ports[dpdk_obj->pmd_port_cnt] = i;
      rte_eth_macaddr_get(i,
                          &(dpdk_obj->pmd_eth_addrs[dpdk_obj->pmd_port_cnt]));
      fprintf(stderr, "    MAC address for port #%d:\n", i);
      fprintf(stderr, "        ");
      for (int j = 0; j < RTE_ETHER_ADDR_LEN; ++j) {
        fprintf(stderr, "%02X:",
                dpdk_obj->pmd_eth_addrs[dpdk_obj->pmd_port_cnt].addr_bytes[j]);
      }
      fprintf(stderr, "\n");
      ++dpdk_obj->pmd_port_cnt;
    }
  }

  // Init a PMD port with one of the available and valid ports.
  assert(dpdk_obj->pmd_port_cnt > 0);
  dpdk_obj->pmd_port_to_use = 0;
  uint16_t pmd_port_id = dpdk_obj->pmd_ports[dpdk_obj->pmd_port_to_use];
  struct rte_eth_dev_info dev_info;
  ret = rte_eth_dev_info_get(pmd_port_id, &dev_info);
  if (ret) {
    fprintf(stderr, "Failed to fetch device information.\n");
    return -1;
  }

  // Make minimal Ethernet port configuration:
  //  - no checksum offload
  //  - no RSS
  //  - standard frames
  struct rte_eth_conf port_conf;
  memset(&port_conf, 0, sizeof(port_conf));
  port_conf.link_speeds = ETH_LINK_SPEED_AUTONEG;
  port_conf.rxmode.max_rx_pkt_len = kMTUStandardFrames;
  ret = rte_eth_dev_configure(pmd_port_id, kRingN, kRingN, &port_conf);
  if (ret) {
    fprintf(stderr, "Failed to configure port.\n");
    return -1;
  }
  ret = rte_eth_dev_set_mtu(pmd_port_id, kMTUStandardFrames);
  if (ret) {
    fprintf(stderr, "Failed to configure MTU size.\n");
    return -1;
  }

  // Make packet pool.
  dpdk_obj->mpool = rte_pktmbuf_pool_create(
      kPacketMemPoolName, kRingN * kRingDescN * 2, 0, 0,
      kMTUStandardFrames + RTE_PKTMBUF_HEADROOM, SOCKET_ID_ANY);
  if (dpdk_obj->mpool == NULL) {
    fprintf(stderr, "Failed to create memory pool for packets.\n");
    return -1;
  }

  // Set-up RX/TX descs.
  uint16_t rx_ring_desc_N_actual = kRingDescN;
  uint16_t tx_ring_desc_N_actual = kRingDescN;
  ret = rte_eth_dev_adjust_nb_rx_tx_desc(pmd_port_id, &rx_ring_desc_N_actual,
                                         &tx_ring_desc_N_actual);
  if (ret) {
    fprintf(stderr, "Failed to adjust the number of RX descriptors.\n");
    return -1;
  }

  // Setup RX/TX rings (queues).
  for (int i = 0; i < kRingN; i++) {
    int ret = rte_eth_tx_queue_setup(pmd_port_id, i, tx_ring_desc_N_actual,
                                     (unsigned int)SOCKET_ID_ANY,
                                     &dev_info.default_txconf);
    if (ret) {
      fprintf(stderr, "Failed to setup TX queues for ring %d\n", i);
      return -1;
    }

    ret = rte_eth_rx_queue_setup(pmd_port_id, i, rx_ring_desc_N_actual,
                                 (unsigned int)SOCKET_ID_ANY,
                                 &dev_info.default_rxconf, dpdk_obj->mpool);
    if (ret) {
      fprintf(stderr, "Failed to setup RX queues for ring %d\n", i);
      return -1;
    }
  }

  // Start port.
  ret = rte_eth_dev_start(pmd_port_id);
  if (ret) {
    fprintf(stderr, "Failed to start port\n");
    return -1;
  }

  // Get link status.
  fprintf(stderr, "Port started, waiting for link to get up...\n");
  struct rte_eth_link link_status;
  memset(&link_status, 0, sizeof(link_status));
  size_t tout_cnt = 0;
  while (tout_cnt < kLinkTimeOut_ms &&
         link_status.link_status == ETH_LINK_DOWN) {
    memset(&link_status, 0, sizeof(link_status));
    rte_eth_link_get_nowait(pmd_port_id, &link_status);
    ++tout_cnt;

    const useconds_t ms = 1000;
    usleep(ms);
  }
  if (link_status.link_status == ETH_LINK_UP)
    fprintf(stderr, "Link is UP and is ready to do packet I/O.\n");
  else {
    fprintf(stderr, "Link is DOWN.\n");
    return -1;
  }

  return 0;
}

// Free rx buffers when data are not needed anymore.
static void FreeDPDKRxBuffers(struct DPDKObj *dpdk_obj) {
  rte_pktmbuf_free_bulk(dpdk_obj->rx_mbufs, dpdk_obj->rx_burst_size);
}

static void FreeDPDKPacket(struct rte_mbuf *packet) {
  rte_pktmbuf_free(packet);
}

// Allocate DPDK buffers for the transmission of batch_size packets.
static int AllocateDPDKTxBuffers(struct DPDKObj *dpdk_obj, size_t batch_size) {
  if (batch_size > kMaxBurstSize)
    return -1;

  int ret =
      rte_pktmbuf_alloc_bulk(dpdk_obj->mpool, dpdk_obj->tx_mbufs, batch_size);
  if (ret)
    return -1;
  dpdk_obj->tx_burst_size = batch_size;
  dpdk_obj->tx_burst_ptr = 0;

  return 0;
}

// Get pointer to the next tx buffer from the pre-allocated burst pool.
static struct rte_mbuf *GetNextDPDKTxBuffer(struct DPDKObj *dpdk_obj) {
  if (dpdk_obj->tx_burst_ptr >= dpdk_obj->tx_burst_size)
    return NULL;
  struct rte_mbuf *mbuf = dpdk_obj->tx_mbufs[dpdk_obj->tx_burst_ptr];
  ++dpdk_obj->tx_burst_ptr;
  return mbuf;
}

// Get pointer to the payload.
static struct rte_mbuf *GetNextDPDKRxBuffer(struct DPDKObj *dpdk_obj) {
  if (dpdk_obj->rx_burst_ptr >= dpdk_obj->rx_burst_size)
    return NULL;
  struct rte_mbuf *mbuf = dpdk_obj->rx_mbufs[dpdk_obj->rx_burst_ptr];
  ++dpdk_obj->rx_burst_ptr;
  return mbuf;
}

// amin
static uint8_t *ExtractPacketPayload(uint8_t *pckt) {
  // returing the pointer to the beginning of the packet data after the header
  return pckt + sizeof(struct rte_ether_hdr);
}
// static uint8_t *ExtractPacketPayload(struct rte_mbuf *pckt) {
//   return rte_pktmbuf_mtod(pckt, uint8_t *) + sizeof(struct rte_ether_hdr);
// }

static void AppendPacketHeader(struct DPDKObj *dpdk_obj, struct rte_mbuf *pckt,
                               const struct rte_ether_addr *dst_mac,
                               size_t length) {
  // Packet header.
  size_t pkt_size = sizeof(struct rte_ether_hdr) + length;
  pckt->data_len = pkt_size;
  pckt->pkt_len = pkt_size;

  // Ethernet header.
  struct rte_ether_hdr *eth_hdr =
      rte_pktmbuf_mtod(pckt, struct rte_ether_hdr *);
  rte_ether_addr_copy(&dpdk_obj->pmd_eth_addrs[dpdk_obj->pmd_port_to_use],
                      &eth_hdr->s_addr);
  rte_ether_addr_copy(dst_mac, &eth_hdr->d_addr);
  // We use will RTE_ETHER_TYPE_IPV4 header type to avoid any issues on the
  // switch, but we won't actually use IP.
  eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
}

// Send a batch of packets sitting so far in tx_mbuf's.
static int SendBatch(struct DPDKObj *dpdk_obj) {
  // Send packet.
  const uint16_t burst_size = dpdk_obj->tx_burst_size;
  const uint16_t ring_id = 0;
  uint16_t pckt_sent =
      rte_eth_tx_burst(dpdk_obj->pmd_ports[dpdk_obj->pmd_port_to_use], ring_id,
                       dpdk_obj->tx_mbufs, burst_size);
  if (pckt_sent != burst_size) {
    fprintf(stderr, "Failed to send all %d packets, only %d was sent.\n",
            burst_size, pckt_sent);
    rte_pktmbuf_free_bulk(dpdk_obj->tx_mbufs + pckt_sent,
                          (unsigned int)(burst_size - pckt_sent));
    return -1;
  }

  dpdk_obj->tx_burst_ptr = 0;
  return 0;
}

// Receive one or many packets and store them in pckts.
// This is a non-blocking call.
static uint8_t *RecvOverDPDK(struct DPDKObj *dpdk_obj) {
// Generate dataset.
  // size_t ds_size = FLAGS_dataset_size;
  size_t ds_size = 2000;
  long unsigned int ksize_min, ksize_max, vsize_min, vsize_max;
  float ksize_skew, vsize_skew;

  // hard coding these values for now
  long unsigned int ksize_min = 10, ksize_max = 100, vsize_min = 100, vsize_max = 1000;
  float ksize_skew = 0.9, vsize_skew = 0.9;

  size_t kMaxValSize = 1024;


  std::cout << "Generating dataset: #items: " << ds_size
            << ", key distribution: " << ksize_min << "|" << ksize_max << "|"
            << ksize_skew << ", value distribution: " << vsize_min << "|"
            << vsize_max << "|" << vsize_skew << "\n";


  DSet dset_keys;
  DSet dset_vals;

  std::default_random_engine generator;
  zipfian_int_distribution<size_t> k_distribution(ksize_min, ksize_max,
                                                  ksize_skew);
  zipfian_int_distribution<size_t> v_distribution(vsize_min, vsize_max,
                                                  vsize_skew);
  for (size_t i = 0; i < ds_size; ++i) {
    size_t key_len = k_distribution(generator);
    size_t val_len = v_distribution(generator);
    dset_keys.push_back(std::vector<uint8_t>());
    dset_vals.push_back(std::vector<uint8_t>());
    dset_keys.back().reserve(key_len);
    dset_vals.back().reserve(val_len);
    for (size_t j = 0; j < key_len; ++j) {
      dset_keys.back().push_back(std::rand() % 256);
    }
    for (size_t j = 0; j < val_len; ++j) {
      dset_vals.back().push_back(std::rand() % 256);
    }
  }
  std::cout << "Dataset generated.\n";

  // Populate memcached server with the dataset.
  // hardcode this value for now - populate_workload_size = 1000
  size_t populate_ds_size = 1000;
  // size_t populate_ds_size = FLAGS_populate_workload_size;

  if (populate_ds_size > ds_size) {
    std::cout << "Population dataset is bigger than the main dataset.\n";
    // return -1;
  } else {
    std::cout << "Populating memcached server with " << populate_ds_size
              << " first elements from the generated dataset.\n";
  }
  
  int res = 0;
  // size_t ok_responses_recved = 0;
  size_t batch_cnt = 0;

  for (size_t i = 0; i < populate_ds_size; ++i) {

    uint8_t *tx_buff_ptr = (uint8_t *)malloc(kMaxPacketSize);
    if (tx_buff_ptr == NULL) {
      std::cerr << "Failed to allocate memory for the packet\n";
      // return -1;
    }
    size_t h_size =
        HelperFormUdpHeader(reinterpret_cast<MemcacheUdpHeader *>(tx_buff_ptr), i, 0);
    tx_buff_ptr += sizeof(MemcacheUdpHeader);

    // Form request header.
    size_t rh_size = HelperFormSetReqHeader(
        reinterpret_cast<ReqHdr *>(tx_buff_ptr), dset_keys[i].size(), dset_vals[i].size());
    tx_buff_ptr += sizeof(ReqHdr);

    //start of the packet data
    uint8_t *data_ptr = tx_buff_ptr;

    // Fill packet: extra, unlimited storage time.
    uint32_t extra[2] = {0x00, 0x00};
    std::memcpy(tx_buff_ptr, extra, kExtraSizeForSet);
    tx_buff_ptr += kExtraSizeForSet;

    // Fill packet: key.
    std::memcpy(tx_buff_ptr, dset_keys[i].data(), dset_keys[i].size());
    tx_buff_ptr += dset_keys[i].size();

    // Fill packet: value.
    std::memcpy(tx_buff_ptr, dset_vals[i].data(), dset_vals[i].size());

    // Check total packet size.
    uint32_t total_length = h_size + rh_size;
    if (total_length > kMaxPacketSize) {
      std::cerr << "Packet size of " << total_length << " is too large\n";
      // return -1;
    }

    return tx_buff_ptr;

  } // end of for loop


  std::cout << "Server populated with " << populate_ds_size
            << " key-value pairs, "
            << " OK response count: " << "\n";


  /* If we are in simulation, take checkpoint here. */
  #ifdef _GEM5_
      fprintf(stderr, "Taking post-warmup checkpoint.\n");
      system("m5 checkpoint");
  #endif
}
  
  // // Execute the load.
  // std::cout << "If you want a separate trace for the workoad benchark, now it's a good time to start capturing it.\n";
  // std::cout << "Press <Ctrl-C> to execute the workload...\n";
  // while (!kCtlzArmed) {
  //   sleep(1);
  // }
  // // De-register the signal.
  // signal(SIGINT, SIG_DFL);

  // size_t wrkl_size;
  // float wrkl_get_frac;
  // sscanf(FLAGS_workload_config.c_str(), "%lu-%f", &wrkl_size, &wrkl_get_frac);
  // size_t num_of_unique_sets = ds_size - populate_ds_size;
  // std::cout << "Executing workload of #queries: " << wrkl_size
  //           << ", GET/SET= " << wrkl_get_frac
  //           << ", unique SET keys: " << num_of_unique_sets << "\n";

  // size_t ok_set_responses_recved = 0;
  // size_t ok_get_responses_recved = 0;
  // batch_cnt = 0;
  // std::map<uint16_t, size_t> sent_get_idxs;
  // struct timespec wrkl_start, wrkl_end;
  // size_t set_cnt = 0;
  // clock_gettime(CLOCK_MONOTONIC, &wrkl_start);
  // for (size_t i = 0; i < wrkl_size; ++i) {
  //   float get_set = rand() / (float)RAND_MAX;
  //   if (get_set < wrkl_get_frac) {
  //     // Execute GET.
  //     // Always hit in the cache, i.e. use a populated key.
  //     size_t random_key_idx = static_cast<size_t>(rand()) % populate_ds_size;
  //     if (FLAGS_check_get_correctness)
  //       sent_get_idxs[i] = random_key_idx;
  //     auto &key = dset_keys[random_key_idx];
  //     client.Get(i, 0, key.data(), key.size());
  //   } else {
  //     // Execute SET.
  //     // Always miss in the cache, i.e. use an unpopulated key.
  //     size_t key_idx = populate_ds_size + (set_cnt % num_of_unique_sets);
  //     client.Set(i, 0, dset_keys[key_idx].data(), dset_keys[key_idx].size(),
  //                dset_vals[key_idx].data(), dset_vals[key_idx].size());
  //   }
  //   ++batch_cnt;

  //   // Check correctness for batch.
  //   if (batch_cnt == FLAGS_batching) {
  //     std::vector<std::pair<uint16_t, MemcachedClient::Status>> set_statuses;
  //     std::vector<std::pair<uint16_t, std::vector<uint8_t>>> get_statuses;
  //     client.RecvResponses(&set_statuses, &get_statuses);

  //     for (auto &s : set_statuses) {
  //       // Just ckeck ret. status.
  //       if (s.second == MemcachedClient::kOK)
  //         ++ok_set_responses_recved;
  //     }
  //     for (auto &g : get_statuses) {
  //       if (g.second.size() != 0) {
  //         // Check returned data.
  //         if (FLAGS_check_get_correctness) {
  //           size_t ds_idx = sent_get_idxs[g.first];
  //           if (std::memcmp(dset_vals[ds_idx].data(), g.second.data(),
  //                           g.second.size()) == 0)
  //             ++ok_get_responses_recved;
  //         } else {
  //           ++ok_get_responses_recved;
  //         }
  //       }
  //     }

  //     if (FLAGS_check_get_correctness)
  //       sent_get_idxs.clear();
  //     batch_cnt = 0;
  //   }
  // }
  // clock_gettime(CLOCK_MONOTONIC, &wrkl_end);
  // static constexpr long int kBillion = 1000000000L;
  // long int wrkl_diff = kBillion * (wrkl_end.tv_sec - wrkl_start.tv_sec) +
  //                      wrkl_end.tv_nsec - wrkl_start.tv_nsec;
  // double wrkl_ns = wrkl_diff / (double)wrkl_size;
  // double wrkl_avg_thr = kBillion * (1 / wrkl_ns); // qps

  // std::cout << "Workload executed, some statistics: \n";
  // std::cout << "   * total requests sent: " << wrkl_size << "\n";
  // std::cout << "   * average sending throughput: " << wrkl_avg_thr << " qps\n";
  // std::cout << "   * OK SET responses: " << ok_set_responses_recved << "\n";
  // std::cout << "   * OK GET responses: " << ok_get_responses_recved << "\n";
  // std::cout << "   * OK total responses: "
  //           << ok_set_responses_recved + ok_get_responses_recved << "\n";
}


// Receive one or many packets and store them in pckts.
// Returns the number of packets received.
// This is a non-blocking call.
// static int RecvOverDPDK(struct DPDKObj *dpdk_obj) {
//   struct rte_mbuf *packets[kMaxBurstSize];
//   const uint16_t ring_id = 0;
//   uint16_t received_pckt_cnt = rte_eth_rx_burst(dpdk_obj->pmd_ports[dpdk_obj->pmd_port_to_use], ring_id, packets, kMaxBurstSize);

//   dpdk_obj->rx_burst_size = 0;
//   for (int i = 0; i < received_pckt_cnt; ++i) {
//     struct rte_ether_hdr *eth_hdr =
//         rte_pktmbuf_mtod(packets[i], struct rte_ether_hdr *);
//     // Skip not our packets.
//     if (rte_be_to_cpu_16(eth_hdr->ether_type) != RTE_ETHER_TYPE_IPV4) {
//       FreeDPDKPacket(packets[i]);
//       continue;
//     }

//     // Store the payload pointers.
//     *(dpdk_obj->rx_mbufs + dpdk_obj->rx_burst_size) = packets[i];
//     ++dpdk_obj->rx_burst_size;
//   }

//   dpdk_obj->rx_burst_ptr = 0;
//   return dpdk_obj->rx_burst_size;
// }

#endif
