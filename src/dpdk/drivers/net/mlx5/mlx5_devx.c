/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_malloc.h>
#include <rte_common.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_common_devx.h>
#include <mlx5_malloc.h>

#include "mlx5.h"
#include "mlx5_common_os.h"
#include "mlx5_rxtx.h"
#include "mlx5_utils.h"
#include "mlx5_devx.h"
#include "mlx5_flow.h"
#include "mlx5_flow_os.h"

/**
 * Modify RQ vlan stripping offload
 *
 * @param rxq_obj
 *   Rx queue object.
 *
 * @return
 *   0 on success, non-0 otherwise
 */
static int
mlx5_rxq_obj_modify_rq_vlan_strip(struct mlx5_rxq_obj *rxq_obj, int on)
{
	struct mlx5_devx_modify_rq_attr rq_attr;

	memset(&rq_attr, 0, sizeof(rq_attr));
	rq_attr.rq_state = MLX5_RQC_STATE_RDY;
	rq_attr.state = MLX5_RQC_STATE_RDY;
	rq_attr.vsd = (on ? 0 : 1);
	rq_attr.modify_bitmask = MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD;
	return mlx5_devx_cmd_modify_rq(rxq_obj->rq_obj.rq, &rq_attr);
}

/**
 * Modify RQ using DevX API.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 * @param type
 *   Type of change queue state.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_modify_rq(struct mlx5_rxq_obj *rxq_obj, uint8_t type)
{
	struct mlx5_devx_modify_rq_attr rq_attr;

	memset(&rq_attr, 0, sizeof(rq_attr));
	switch (type) {
	case MLX5_RXQ_MOD_ERR2RST:
		rq_attr.rq_state = MLX5_RQC_STATE_ERR;
		rq_attr.state = MLX5_RQC_STATE_RST;
		break;
	case MLX5_RXQ_MOD_RST2RDY:
		rq_attr.rq_state = MLX5_RQC_STATE_RST;
		rq_attr.state = MLX5_RQC_STATE_RDY;
		break;
	case MLX5_RXQ_MOD_RDY2ERR:
		rq_attr.rq_state = MLX5_RQC_STATE_RDY;
		rq_attr.state = MLX5_RQC_STATE_ERR;
		break;
	case MLX5_RXQ_MOD_RDY2RST:
		rq_attr.rq_state = MLX5_RQC_STATE_RDY;
		rq_attr.state = MLX5_RQC_STATE_RST;
		break;
	default:
		break;
	}
	return mlx5_devx_cmd_modify_rq(rxq_obj->rq_obj.rq, &rq_attr);
}

/**
 * Modify SQ using DevX API.
 *
 * @param txq_obj
 *   DevX Tx queue object.
 * @param type
 *   Type of change queue state.
 * @param dev_port
 *   Unnecessary.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_modify_sq(struct mlx5_txq_obj *obj, enum mlx5_txq_modify_type type,
		    uint8_t dev_port)
{
	struct mlx5_devx_modify_sq_attr msq_attr = { 0 };
	int ret;

	if (type != MLX5_TXQ_MOD_RST2RDY) {
		/* Change queue state to reset. */
		if (type == MLX5_TXQ_MOD_ERR2RDY)
			msq_attr.sq_state = MLX5_SQC_STATE_ERR;
		else
			msq_attr.sq_state = MLX5_SQC_STATE_RDY;
		msq_attr.state = MLX5_SQC_STATE_RST;
		ret = mlx5_devx_cmd_modify_sq(obj->sq_obj.sq, &msq_attr);
		if (ret) {
			DRV_LOG(ERR, "Cannot change the Tx SQ state to RESET"
				" %s", strerror(errno));
			rte_errno = errno;
			return ret;
		}
	}
	if (type != MLX5_TXQ_MOD_RDY2RST) {
		/* Change queue state to ready. */
		msq_attr.sq_state = MLX5_SQC_STATE_RST;
		msq_attr.state = MLX5_SQC_STATE_RDY;
		ret = mlx5_devx_cmd_modify_sq(obj->sq_obj.sq, &msq_attr);
		if (ret) {
			DRV_LOG(ERR, "Cannot change the Tx SQ state to READY"
				" %s", strerror(errno));
			rte_errno = errno;
			return ret;
		}
	}
	/*
	 * The dev_port variable is relevant only in Verbs API, and there is a
	 * pointer that points to this function and a parallel function in verbs
	 * intermittently, so they should have the same parameters.
	 */
	(void)dev_port;
	return 0;
}

/**
 * Destroy the Rx queue DevX object.
 *
 * @param rxq_obj
 *   Rxq object to destroy.
 */
static void
mlx5_rxq_release_devx_resources(struct mlx5_rxq_obj *rxq_obj)
{
	mlx5_devx_rq_destroy(&rxq_obj->rq_obj);
	memset(&rxq_obj->rq_obj, 0, sizeof(rxq_obj->rq_obj));
	mlx5_devx_cq_destroy(&rxq_obj->cq_obj);
	memset(&rxq_obj->cq_obj, 0, sizeof(rxq_obj->cq_obj));
}

/**
 * Release an Rx DevX queue object.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 */
static void
mlx5_rxq_devx_obj_release(struct mlx5_rxq_obj *rxq_obj)
{
	MLX5_ASSERT(rxq_obj);
	if (rxq_obj->rxq_ctrl->type == MLX5_RXQ_TYPE_HAIRPIN) {
		MLX5_ASSERT(rxq_obj->rq);
		mlx5_devx_modify_rq(rxq_obj, MLX5_RXQ_MOD_RDY2RST);
		claim_zero(mlx5_devx_cmd_destroy(rxq_obj->rq));
	} else {
		MLX5_ASSERT(rxq_obj->cq_obj.cq);
		MLX5_ASSERT(rxq_obj->rq_obj.rq);
		mlx5_rxq_release_devx_resources(rxq_obj);
		if (rxq_obj->devx_channel)
			mlx5_os_devx_destroy_event_channel
							(rxq_obj->devx_channel);
	}
}

/**
 * Get event for an Rx DevX queue object.
 *
 * @param rxq_obj
 *   DevX Rx queue object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rx_devx_get_event(struct mlx5_rxq_obj *rxq_obj)
{
#ifdef HAVE_IBV_DEVX_EVENT
	union {
		struct mlx5dv_devx_async_event_hdr event_resp;
		uint8_t buf[sizeof(struct mlx5dv_devx_async_event_hdr) + 128];
	} out;
	int ret = mlx5_glue->devx_get_event(rxq_obj->devx_channel,
					    &out.event_resp,
					    sizeof(out.buf));

	if (ret < 0) {
		rte_errno = errno;
		return -rte_errno;
	}
	if (out.event_resp.cookie != (uint64_t)(uintptr_t)rxq_obj->cq_obj.cq) {
		rte_errno = EINVAL;
		return -rte_errno;
	}
	return 0;
#else
	(void)rxq_obj;
	rte_errno = ENOTSUP;
	return -rte_errno;
#endif /* HAVE_IBV_DEVX_EVENT */
}

/**
 * Create a RQ object using DevX.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_create_devx_rq_resources(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr rq_attr = { 0 };
	uint16_t log_desc_n = rxq_data->elts_n - rxq_data->sges_n;
	uint32_t wqe_size, log_wqe_size;

	/* Fill RQ attributes. */
	rq_attr.mem_rq_type = MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_INLINE;
	rq_attr.flush_in_error_en = 1;
	rq_attr.vsd = (rxq_data->vlan_strip) ? 0 : 1;
	rq_attr.cqn = rxq_ctrl->obj->cq_obj.cq->id;
	rq_attr.scatter_fcs = (rxq_data->crc_present) ? 1 : 0;
	rq_attr.ts_format = mlx5_ts_format_conv(priv->sh->rq_ts_format);
	/* Fill WQ attributes for this RQ. */
	if (mlx5_rxq_mprq_enabled(rxq_data)) {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC_STRIDING_RQ;
		/*
		 * Number of strides in each WQE:
		 * 512*2^single_wqe_log_num_of_strides.
		 */
		rq_attr.wq_attr.single_wqe_log_num_of_strides =
				rxq_data->strd_num_n -
				MLX5_MIN_SINGLE_WQE_LOG_NUM_STRIDES;
		/* Stride size = (2^single_stride_log_num_of_bytes)*64B. */
		rq_attr.wq_attr.single_stride_log_num_of_bytes =
				rxq_data->strd_sz_n -
				MLX5_MIN_SINGLE_STRIDE_LOG_NUM_BYTES;
		wqe_size = sizeof(struct mlx5_wqe_mprq);
	} else {
		rq_attr.wq_attr.wq_type = MLX5_WQ_TYPE_CYCLIC;
		wqe_size = sizeof(struct mlx5_wqe_data_seg);
	}
	log_wqe_size = log2above(wqe_size) + rxq_data->sges_n;
	wqe_size = 1 << log_wqe_size; /* round up power of two.*/
	rq_attr.wq_attr.log_wq_stride = log_wqe_size;
	rq_attr.wq_attr.log_wq_sz = log_desc_n;
	rq_attr.wq_attr.end_padding_mode = priv->config.hw_padding ?
						MLX5_WQ_END_PAD_MODE_ALIGN :
						MLX5_WQ_END_PAD_MODE_NONE;
	rq_attr.wq_attr.pd = priv->sh->pdn;
	rq_attr.counter_set_id = priv->counter_set_id;
	/* Create RQ using DevX API. */
	return mlx5_devx_rq_create(priv->sh->ctx, &rxq_ctrl->obj->rq_obj,
				   wqe_size, log_desc_n, &rq_attr,
				   rxq_ctrl->socket);
}

/**
 * Create a DevX CQ object for an Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_create_devx_cq_resources(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_devx_cq *cq_obj = 0;
	struct mlx5_devx_cq_attr cq_attr = { 0 };
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	unsigned int cqe_n = mlx5_rxq_cqe_num(rxq_data);
	uint32_t log_cqe_n;
	uint16_t event_nums[1] = { 0 };
	int ret = 0;

	if (priv->config.cqe_comp && !rxq_data->hw_timestamp &&
	    !rxq_data->lro) {
		cq_attr.cqe_comp_en = 1u;
		rxq_data->mcqe_format = priv->config.cqe_comp_fmt;
		rxq_data->byte_mask = UINT32_MAX;
		switch (priv->config.cqe_comp_fmt) {
		case MLX5_CQE_RESP_FORMAT_HASH:
			/* fallthrough */
		case MLX5_CQE_RESP_FORMAT_CSUM:
			/*
			 * Select CSUM miniCQE format only for non-vectorized
			 * MPRQ Rx burst, use HASH miniCQE format for others.
			 */
			if (mlx5_rxq_check_vec_support(rxq_data) < 0 &&
			    mlx5_rxq_mprq_enabled(rxq_data))
				cq_attr.mini_cqe_res_format =
					MLX5_CQE_RESP_FORMAT_CSUM_STRIDX;
			else
				cq_attr.mini_cqe_res_format =
					MLX5_CQE_RESP_FORMAT_HASH;
			rxq_data->mcqe_format = cq_attr.mini_cqe_res_format;
			break;
		case MLX5_CQE_RESP_FORMAT_FTAG_STRIDX:
			rxq_data->byte_mask = MLX5_LEN_WITH_MARK_MASK;
			/* fallthrough */
		case MLX5_CQE_RESP_FORMAT_CSUM_STRIDX:
			cq_attr.mini_cqe_res_format = priv->config.cqe_comp_fmt;
			break;
		case MLX5_CQE_RESP_FORMAT_L34H_STRIDX:
			cq_attr.mini_cqe_res_format = 0;
			cq_attr.mini_cqe_res_format_ext = 1;
			break;
		}
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is enabled, format %d.",
			dev->data->port_id, priv->config.cqe_comp_fmt);
		/*
		 * For vectorized Rx, it must not be doubled in order to
		 * make cq_ci and rq_ci aligned.
		 */
		if (mlx5_rxq_check_vec_support(rxq_data) < 0)
			cqe_n *= 2;
	} else if (priv->config.cqe_comp && rxq_data->hw_timestamp) {
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is disabled for HW"
			" timestamp.",
			dev->data->port_id);
	} else if (priv->config.cqe_comp && rxq_data->lro) {
		DRV_LOG(DEBUG,
			"Port %u Rx CQE compression is disabled for LRO.",
			dev->data->port_id);
	}
	cq_attr.uar_page_id = mlx5_os_get_devx_uar_page_id(sh->devx_rx_uar);
	log_cqe_n = log2above(cqe_n);
	/* Create CQ using DevX API. */
	ret = mlx5_devx_cq_create(sh->ctx, &rxq_ctrl->obj->cq_obj, log_cqe_n,
				  &cq_attr, sh->numa_node);
	if (ret)
		return ret;
	cq_obj = &rxq_ctrl->obj->cq_obj;
	rxq_data->cqes = (volatile struct mlx5_cqe (*)[])
							(uintptr_t)cq_obj->cqes;
	rxq_data->cq_db = cq_obj->db_rec;
	rxq_data->cq_uar = mlx5_os_get_devx_uar_base_addr(sh->devx_rx_uar);
	rxq_data->cqe_n = log_cqe_n;
	rxq_data->cqn = cq_obj->cq->id;
	if (rxq_ctrl->obj->devx_channel) {
		ret = mlx5_os_devx_subscribe_devx_event
					      (rxq_ctrl->obj->devx_channel,
					       cq_obj->cq->obj,
					       sizeof(event_nums),
					       event_nums,
					       (uint64_t)(uintptr_t)cq_obj->cq);
		if (ret) {
			DRV_LOG(ERR, "Fail to subscribe CQ to event channel.");
			ret = errno;
			mlx5_devx_cq_destroy(cq_obj);
			memset(cq_obj, 0, sizeof(*cq_obj));
			rte_errno = ret;
			return -ret;
		}
	}
	return 0;
}

/**
 * Create the Rx hairpin queue object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_obj_hairpin_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_devx_create_rq_attr attr = { 0 };
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	uint32_t max_wq_data;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	tmpl->rxq_ctrl = rxq_ctrl;
	attr.hairpin = 1;
	max_wq_data = priv->config.hca_attr.log_max_hairpin_wq_data_sz;
	/* Jumbo frames > 9KB should be supported, and more packets. */
	if (priv->config.log_hp_size != (uint32_t)MLX5_ARG_UNSET) {
		if (priv->config.log_hp_size > max_wq_data) {
			DRV_LOG(ERR, "Total data size %u power of 2 is "
				"too large for hairpin.",
				priv->config.log_hp_size);
			rte_errno = ERANGE;
			return -rte_errno;
		}
		attr.wq_attr.log_hairpin_data_sz = priv->config.log_hp_size;
	} else {
		attr.wq_attr.log_hairpin_data_sz =
				(max_wq_data < MLX5_HAIRPIN_JUMBO_LOG_SIZE) ?
				 max_wq_data : MLX5_HAIRPIN_JUMBO_LOG_SIZE;
	}
	/* Set the packets number to the maximum value for performance. */
	attr.wq_attr.log_hairpin_num_packets =
			attr.wq_attr.log_hairpin_data_sz -
			MLX5_HAIRPIN_QUEUE_STRIDE;
	attr.counter_set_id = priv->counter_set_id;
	tmpl->rq = mlx5_devx_cmd_create_rq(priv->sh->ctx, &attr,
					   rxq_ctrl->socket);
	if (!tmpl->rq) {
		DRV_LOG(ERR,
			"Port %u Rx hairpin queue %u can't create rq object.",
			dev->data->port_id, idx);
		rte_errno = errno;
		return -rte_errno;
	}
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_HAIRPIN;
	return 0;
}

/**
 * Create the Rx queue DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Rx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_rxq_devx_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[idx];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	struct mlx5_rxq_obj *tmpl = rxq_ctrl->obj;
	int ret = 0;

	MLX5_ASSERT(rxq_data);
	MLX5_ASSERT(tmpl);
	if (rxq_ctrl->type == MLX5_RXQ_TYPE_HAIRPIN)
		return mlx5_rxq_obj_hairpin_new(dev, idx);
	tmpl->rxq_ctrl = rxq_ctrl;
	if (rxq_ctrl->irq) {
		int devx_ev_flag =
			  MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA;

		tmpl->devx_channel = mlx5_os_devx_create_event_channel
								(priv->sh->ctx,
								 devx_ev_flag);
		if (!tmpl->devx_channel) {
			rte_errno = errno;
			DRV_LOG(ERR, "Failed to create event channel %d.",
				rte_errno);
			goto error;
		}
		tmpl->fd = mlx5_os_get_devx_channel_fd(tmpl->devx_channel);
	}
	/* Create CQ using DevX API. */
	ret = mlx5_rxq_create_devx_cq_resources(dev, idx);
	if (ret) {
		DRV_LOG(ERR, "Failed to create CQ.");
		goto error;
	}
	/* Create RQ using DevX API. */
	ret = mlx5_rxq_create_devx_rq_resources(dev, idx);
	if (ret) {
		DRV_LOG(ERR, "Port %u Rx queue %u RQ creation failure.",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	/* Change queue state to ready. */
	ret = mlx5_devx_modify_rq(tmpl, MLX5_RXQ_MOD_RST2RDY);
	if (ret)
		goto error;
	rxq_data->wqes = (void *)(uintptr_t)tmpl->rq_obj.umem_buf;
	rxq_data->rq_db = (uint32_t *)(uintptr_t)tmpl->rq_obj.db_rec;
	rxq_data->cq_arm_sn = 0;
	rxq_data->cq_ci = 0;
	mlx5_rxq_initialize(rxq_data);
	dev->data->rx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	rxq_ctrl->wqn = tmpl->rq_obj.rq->id;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_rxq_devx_obj_release(tmpl);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Prepare RQT attribute structure for DevX RQT API.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param log_n
 *   Log of number of queues in the array.
 * @param ind_tbl
 *   DevX indirection table object.
 *
 * @return
 *   The RQT attr object initialized, NULL otherwise and rte_errno is set.
 */
static struct mlx5_devx_rqt_attr *
mlx5_devx_ind_table_create_rqt_attr(struct rte_eth_dev *dev,
				     const unsigned int log_n,
				     const uint16_t *queues,
				     const uint32_t queues_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_rqt_attr *rqt_attr = NULL;
	const unsigned int rqt_n = 1 << log_n;
	unsigned int i, j;

	rqt_attr = mlx5_malloc(MLX5_MEM_ZERO, sizeof(*rqt_attr) +
			      rqt_n * sizeof(uint32_t), 0, SOCKET_ID_ANY);
	if (!rqt_attr) {
		DRV_LOG(ERR, "Port %u cannot allocate RQT resources.",
			dev->data->port_id);
		rte_errno = ENOMEM;
		return NULL;
	}
	rqt_attr->rqt_max_size = priv->config.ind_table_max_size;
	rqt_attr->rqt_actual_size = rqt_n;
	for (i = 0; i != queues_n; ++i) {
		struct mlx5_rxq_data *rxq = (*priv->rxqs)[queues[i]];
		struct mlx5_rxq_ctrl *rxq_ctrl =
				container_of(rxq, struct mlx5_rxq_ctrl, rxq);

		rqt_attr->rq_list[i] = rxq_ctrl->obj->rq_obj.rq->id;
	}
	MLX5_ASSERT(i > 0);
	for (j = 0; i != rqt_n; ++j, ++i)
		rqt_attr->rq_list[i] = rqt_attr->rq_list[j];
	return rqt_attr;
}

/**
 * Create RQT using DevX API as a filed of indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param log_n
 *   Log of number of queues in the array.
 * @param ind_tbl
 *   DevX indirection table object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_ind_table_new(struct rte_eth_dev *dev, const unsigned int log_n,
			struct mlx5_ind_table_obj *ind_tbl)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_rqt_attr *rqt_attr = NULL;

	MLX5_ASSERT(ind_tbl);
	rqt_attr = mlx5_devx_ind_table_create_rqt_attr(dev, log_n,
							ind_tbl->queues,
							ind_tbl->queues_n);
	if (!rqt_attr)
		return -rte_errno;
	ind_tbl->rqt = mlx5_devx_cmd_create_rqt(priv->sh->ctx, rqt_attr);
	mlx5_free(rqt_attr);
	if (!ind_tbl->rqt) {
		DRV_LOG(ERR, "Port %u cannot create DevX RQT.",
			dev->data->port_id);
		rte_errno = errno;
		return -rte_errno;
	}
	return 0;
}

/**
 * Modify RQT using DevX API as a filed of indirection table.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param log_n
 *   Log of number of queues in the array.
 * @param ind_tbl
 *   DevX indirection table object.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_ind_table_modify(struct rte_eth_dev *dev, const unsigned int log_n,
			   const uint16_t *queues, const uint32_t queues_n,
			   struct mlx5_ind_table_obj *ind_tbl)
{
	int ret = 0;
	struct mlx5_devx_rqt_attr *rqt_attr = NULL;

	MLX5_ASSERT(ind_tbl);
	rqt_attr = mlx5_devx_ind_table_create_rqt_attr(dev, log_n,
							queues,
							queues_n);
	if (!rqt_attr)
		return -rte_errno;
	ret = mlx5_devx_cmd_modify_rqt(ind_tbl->rqt, rqt_attr);
	mlx5_free(rqt_attr);
	if (ret)
		DRV_LOG(ERR, "Port %u cannot modify DevX RQT.",
			dev->data->port_id);
	return ret;
}

/**
 * Destroy the DevX RQT object.
 *
 * @param ind_table
 *   Indirection table to release.
 */
static void
mlx5_devx_ind_table_destroy(struct mlx5_ind_table_obj *ind_tbl)
{
	claim_zero(mlx5_devx_cmd_destroy(ind_tbl->rqt));
}

/**
 * Set TIR attribute struct with relevant input values.
 *
 * @param[in] dev
 *   Pointer to Ethernet device.
 * @param[in] rss_key
 *   RSS key for the Rx hash queue.
 * @param[in] hash_fields
 *   Verbs protocol hash field to make the RSS on.
 * @param[in] ind_tbl
 *   Indirection table for TIR.
 * @param[in] tunnel
 *   Tunnel type.
 * @param[out] tir_attr
 *   Parameters structure for TIR creation/modification.
 *
 * @return
 *   The Verbs/DevX object initialised index, 0 otherwise and rte_errno is set.
 */
static void
mlx5_devx_tir_attr_set(struct rte_eth_dev *dev, const uint8_t *rss_key,
		       uint64_t hash_fields,
		       const struct mlx5_ind_table_obj *ind_tbl,
		       int tunnel, struct mlx5_devx_tir_attr *tir_attr)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_rxq_data *rxq_data = (*priv->rxqs)[ind_tbl->queues[0]];
	struct mlx5_rxq_ctrl *rxq_ctrl =
		container_of(rxq_data, struct mlx5_rxq_ctrl, rxq);
	enum mlx5_rxq_type rxq_obj_type = rxq_ctrl->type;
	bool lro = true;
	uint32_t i;

	/* Enable TIR LRO only if all the queues were configured for. */
	for (i = 0; i < ind_tbl->queues_n; ++i) {
		if (!(*priv->rxqs)[ind_tbl->queues[i]]->lro) {
			lro = false;
			break;
		}
	}
	memset(tir_attr, 0, sizeof(*tir_attr));
	tir_attr->disp_type = MLX5_TIRC_DISP_TYPE_INDIRECT;
	tir_attr->rx_hash_fn = MLX5_RX_HASH_FN_TOEPLITZ;
	tir_attr->tunneled_offload_en = !!tunnel;
	/* If needed, translate hash_fields bitmap to PRM format. */
	if (hash_fields) {
		struct mlx5_rx_hash_field_select *rx_hash_field_select =
#ifdef HAVE_IBV_DEVICE_TUNNEL_SUPPORT
			hash_fields & IBV_RX_HASH_INNER ?
				&tir_attr->rx_hash_field_selector_inner :
#endif
				&tir_attr->rx_hash_field_selector_outer;
		/* 1 bit: 0: IPv4, 1: IPv6. */
		rx_hash_field_select->l3_prot_type =
					!!(hash_fields & MLX5_IPV6_IBV_RX_HASH);
		/* 1 bit: 0: TCP, 1: UDP. */
		rx_hash_field_select->l4_prot_type =
					!!(hash_fields & MLX5_UDP_IBV_RX_HASH);
		/* Bitmask which sets which fields to use in RX Hash. */
		rx_hash_field_select->selected_fields =
			((!!(hash_fields & MLX5_L3_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP) |
			(!!(hash_fields & MLX5_L3_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP |
			(!!(hash_fields & MLX5_L4_SRC_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT |
			(!!(hash_fields & MLX5_L4_DST_IBV_RX_HASH)) <<
			 MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT;
	}
	if (rxq_obj_type == MLX5_RXQ_TYPE_HAIRPIN)
		tir_attr->transport_domain = priv->sh->td->id;
	else
		tir_attr->transport_domain = priv->sh->tdn;
	memcpy(tir_attr->rx_hash_toeplitz_key, rss_key, MLX5_RSS_HASH_KEY_LEN);
	tir_attr->indirect_table = ind_tbl->rqt->id;
	if (dev->data->dev_conf.lpbk_mode)
		tir_attr->self_lb_block =
					MLX5_TIRC_SELF_LB_BLOCK_BLOCK_UNICAST;
	if (lro) {
		tir_attr->lro_timeout_period_usecs = priv->config.lro.timeout;
		tir_attr->lro_max_msg_sz = priv->max_lro_msg_size;
		tir_attr->lro_enable_mask =
				MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO |
				MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO;
	}
}

/**
 * Create an Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param hrxq
 *   Pointer to Rx Hash queue.
 * @param tunnel
 *   Tunnel type.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_hrxq_new(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
		   int tunnel __rte_unused)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_devx_tir_attr tir_attr = {0};
	int err;

	mlx5_devx_tir_attr_set(dev, hrxq->rss_key, hrxq->hash_fields,
			       hrxq->ind_table, tunnel, &tir_attr);
	hrxq->tir = mlx5_devx_cmd_create_tir(priv->sh->ctx, &tir_attr);
	if (!hrxq->tir) {
		DRV_LOG(ERR, "Port %u cannot create DevX TIR.",
			dev->data->port_id);
		rte_errno = errno;
		goto error;
	}
#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)
	if (mlx5_flow_os_create_flow_action_dest_devx_tir(hrxq->tir,
							  &hrxq->action)) {
		rte_errno = errno;
		goto error;
	}
#endif
	return 0;
error:
	err = rte_errno; /* Save rte_errno before cleanup. */
	if (hrxq->tir)
		claim_zero(mlx5_devx_cmd_destroy(hrxq->tir));
	rte_errno = err; /* Restore rte_errno. */
	return -rte_errno;
}

/**
 * Destroy a DevX TIR object.
 *
 * @param hrxq
 *   Hash Rx queue to release its tir.
 */
static void
mlx5_devx_tir_destroy(struct mlx5_hrxq *hrxq)
{
	claim_zero(mlx5_devx_cmd_destroy(hrxq->tir));
}

/**
 * Modify an Rx Hash queue configuration.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param hrxq
 *   Hash Rx queue to modify.
 * @param rss_key
 *   RSS key for the Rx hash queue.
 * @param hash_fields
 *   Verbs protocol hash field to make the RSS on.
 * @param[in] ind_tbl
 *   Indirection table for TIR.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_hrxq_modify(struct rte_eth_dev *dev, struct mlx5_hrxq *hrxq,
		       const uint8_t *rss_key,
		       uint64_t hash_fields,
		       const struct mlx5_ind_table_obj *ind_tbl)
{
	struct mlx5_devx_modify_tir_attr modify_tir = {0};

	/*
	 * untested for modification fields:
	 * - rx_hash_symmetric not set in hrxq_new(),
	 * - rx_hash_fn set hard-coded in hrxq_new(),
	 * - lro_xxx not set after rxq setup
	 */
	if (ind_tbl != hrxq->ind_table)
		modify_tir.modify_bitmask |=
			MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_INDIRECT_TABLE;
	if (hash_fields != hrxq->hash_fields ||
			memcmp(hrxq->rss_key, rss_key, MLX5_RSS_HASH_KEY_LEN))
		modify_tir.modify_bitmask |=
			MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_HASH;
	mlx5_devx_tir_attr_set(dev, rss_key, hash_fields, ind_tbl,
			       0, /* N/A - tunnel modification unsupported */
			       &modify_tir.tir);
	modify_tir.tirn = hrxq->tir->id;
	if (mlx5_devx_cmd_modify_tir(hrxq->tir, &modify_tir)) {
		DRV_LOG(ERR, "port %u cannot modify DevX TIR",
			dev->data->port_id);
		rte_errno = errno;
		return -rte_errno;
	}
	return 0;
}

/**
 * Create a DevX drop action for Rx Hash queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_devx_drop_action_create(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(ERR, "DevX drop action is not supported yet.");
	rte_errno = ENOTSUP;
	return -rte_errno;
}

/**
 * Release a drop hash Rx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
static void
mlx5_devx_drop_action_destroy(struct rte_eth_dev *dev)
{
	(void)dev;
	DRV_LOG(ERR, "DevX drop action is not supported yet.");
	rte_errno = ENOTSUP;
}

/**
 * Create the Tx hairpin queue object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_txq_obj_hairpin_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_devx_create_sq_attr attr = { 0 };
	struct mlx5_txq_obj *tmpl = txq_ctrl->obj;
	uint32_t max_wq_data;

	MLX5_ASSERT(txq_data);
	MLX5_ASSERT(tmpl);
	tmpl->txq_ctrl = txq_ctrl;
	attr.hairpin = 1;
	attr.tis_lst_sz = 1;
	max_wq_data = priv->config.hca_attr.log_max_hairpin_wq_data_sz;
	/* Jumbo frames > 9KB should be supported, and more packets. */
	if (priv->config.log_hp_size != (uint32_t)MLX5_ARG_UNSET) {
		if (priv->config.log_hp_size > max_wq_data) {
			DRV_LOG(ERR, "Total data size %u power of 2 is "
				"too large for hairpin.",
				priv->config.log_hp_size);
			rte_errno = ERANGE;
			return -rte_errno;
		}
		attr.wq_attr.log_hairpin_data_sz = priv->config.log_hp_size;
	} else {
		attr.wq_attr.log_hairpin_data_sz =
				(max_wq_data < MLX5_HAIRPIN_JUMBO_LOG_SIZE) ?
				 max_wq_data : MLX5_HAIRPIN_JUMBO_LOG_SIZE;
	}
	/* Set the packets number to the maximum value for performance. */
	attr.wq_attr.log_hairpin_num_packets =
			attr.wq_attr.log_hairpin_data_sz -
			MLX5_HAIRPIN_QUEUE_STRIDE;
	attr.tis_num = priv->sh->tis->id;
	tmpl->sq = mlx5_devx_cmd_create_sq(priv->sh->ctx, &attr);
	if (!tmpl->sq) {
		DRV_LOG(ERR,
			"Port %u tx hairpin queue %u can't create SQ object.",
			dev->data->port_id, idx);
		rte_errno = errno;
		return -rte_errno;
	}
	return 0;
}

#if defined(HAVE_MLX5DV_DEVX_UAR_OFFSET) || !defined(HAVE_INFINIBAND_VERBS_H)
/**
 * Destroy the Tx queue DevX object.
 *
 * @param txq_obj
 *   Txq object to destroy.
 */
static void
mlx5_txq_release_devx_resources(struct mlx5_txq_obj *txq_obj)
{
	mlx5_devx_sq_destroy(&txq_obj->sq_obj);
	memset(&txq_obj->sq_obj, 0, sizeof(txq_obj->sq_obj));
	mlx5_devx_cq_destroy(&txq_obj->cq_obj);
	memset(&txq_obj->cq_obj, 0, sizeof(txq_obj->cq_obj));
}

/**
 * Create a SQ object and its resources using DevX.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 * @param[in] log_desc_n
 *   Log of number of descriptors in queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_txq_create_devx_sq_resources(struct rte_eth_dev *dev, uint16_t idx,
				  uint16_t log_desc_n)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_txq_obj *txq_obj = txq_ctrl->obj;
	struct mlx5_devx_create_sq_attr sq_attr = {
		.flush_in_error_en = 1,
		.allow_multi_pkt_send_wqe = !!priv->config.mps,
		.min_wqe_inline_mode = priv->config.hca_attr.vport_inline_mode,
		.allow_swp = !!priv->config.swp,
		.cqn = txq_obj->cq_obj.cq->id,
		.tis_lst_sz = 1,
		.tis_num = priv->sh->tis->id,
		.wq_attr = (struct mlx5_devx_wq_attr){
			.pd = priv->sh->pdn,
			.uar_page =
				 mlx5_os_get_devx_uar_page_id(priv->sh->tx_uar),
		},
		.ts_format = mlx5_ts_format_conv(priv->sh->sq_ts_format),
	};
	/* Create Send Queue object with DevX. */
	return mlx5_devx_sq_create(priv->sh->ctx, &txq_obj->sq_obj, log_desc_n,
				   &sq_attr, priv->sh->numa_node);
}
#endif

/**
 * Create the Tx queue DevX object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_txq_devx_obj_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq_data, struct mlx5_txq_ctrl, txq);

	if (txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN)
		return mlx5_txq_obj_hairpin_new(dev, idx);
#if !defined(HAVE_MLX5DV_DEVX_UAR_OFFSET) && defined(HAVE_INFINIBAND_VERBS_H)
	DRV_LOG(ERR, "Port %u Tx queue %u cannot create with DevX, no UAR.",
		     dev->data->port_id, idx);
	rte_errno = ENOMEM;
	return -rte_errno;
#else
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_txq_obj *txq_obj = txq_ctrl->obj;
	struct mlx5_devx_cq_attr cq_attr = {
		.uar_page_id = mlx5_os_get_devx_uar_page_id(sh->tx_uar),
	};
	void *reg_addr;
	uint32_t cqe_n, log_desc_n;
	uint32_t wqe_n, wqe_size;
	int ret = 0;

	MLX5_ASSERT(txq_data);
	MLX5_ASSERT(txq_obj);
	txq_obj->txq_ctrl = txq_ctrl;
	txq_obj->dev = dev;
	cqe_n = (1UL << txq_data->elts_n) / MLX5_TX_COMP_THRESH +
		1 + MLX5_TX_COMP_THRESH_INLINE_DIV;
	log_desc_n = log2above(cqe_n);
	cqe_n = 1UL << log_desc_n;
	if (cqe_n > UINT16_MAX) {
		DRV_LOG(ERR, "Port %u Tx queue %u requests to many CQEs %u.",
			dev->data->port_id, txq_data->idx, cqe_n);
		rte_errno = EINVAL;
		return 0;
	}
	/* Create completion queue object with DevX. */
	ret = mlx5_devx_cq_create(sh->ctx, &txq_obj->cq_obj, log_desc_n,
				  &cq_attr, priv->sh->numa_node);
	if (ret) {
		DRV_LOG(ERR, "Port %u Tx queue %u CQ creation failure.",
			dev->data->port_id, idx);
		goto error;
	}
	txq_data->cqe_n = log_desc_n;
	txq_data->cqe_s = cqe_n;
	txq_data->cqe_m = txq_data->cqe_s - 1;
	txq_data->cqes = txq_obj->cq_obj.cqes;
	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->cq_db = txq_obj->cq_obj.db_rec;
	*txq_data->cq_db = 0;
	/*
	 * Adjust the amount of WQEs depending on inline settings.
	 * The number of descriptors should be enough to handle
	 * the specified number of packets. If queue is being created
	 * with Verbs the rdma-core does queue size adjustment
	 * internally in the mlx5_calc_sq_size(), we do the same
	 * for the queue being created with DevX at this point.
	 */
	wqe_size = txq_data->tso_en ?
		   RTE_ALIGN(txq_ctrl->max_tso_header, MLX5_WSEG_SIZE) : 0;
	wqe_size += sizeof(struct mlx5_wqe_cseg) +
		    sizeof(struct mlx5_wqe_eseg) +
		    sizeof(struct mlx5_wqe_dseg);
	if (txq_data->inlen_send)
		wqe_size = RTE_MAX(wqe_size, sizeof(struct mlx5_wqe_cseg) +
					     sizeof(struct mlx5_wqe_eseg) +
					     RTE_ALIGN(txq_data->inlen_send +
						       sizeof(uint32_t),
						       MLX5_WSEG_SIZE));
	wqe_size = RTE_ALIGN(wqe_size, MLX5_WQE_SIZE) / MLX5_WQE_SIZE;
	/* Create Send Queue object with DevX. */
	wqe_n = RTE_MIN((1UL << txq_data->elts_n) * wqe_size,
			(uint32_t)priv->sh->device_attr.max_qp_wr);
	log_desc_n = log2above(wqe_n);
	ret = mlx5_txq_create_devx_sq_resources(dev, idx, log_desc_n);
	if (ret) {
		DRV_LOG(ERR, "Port %u Tx queue %u SQ creation failure.",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	/* Create the Work Queue. */
	txq_data->wqe_n = log_desc_n;
	txq_data->wqe_s = 1 << txq_data->wqe_n;
	txq_data->wqe_m = txq_data->wqe_s - 1;
	txq_data->wqes = (struct mlx5_wqe *)(uintptr_t)txq_obj->sq_obj.wqes;
	txq_data->wqes_end = txq_data->wqes + txq_data->wqe_s;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_data->wqe_comp = 0;
	txq_data->wqe_thres = txq_data->wqe_s / MLX5_TX_COMP_THRESH_INLINE_DIV;
	txq_data->qp_db = txq_obj->sq_obj.db_rec;
	*txq_data->qp_db = 0;
	txq_data->qp_num_8s = txq_obj->sq_obj.sq->id << 8;
	/* Change Send Queue state to Ready-to-Send. */
	ret = mlx5_devx_modify_sq(txq_obj, MLX5_TXQ_MOD_RST2RDY, 0);
	if (ret) {
		rte_errno = errno;
		DRV_LOG(ERR,
			"Port %u Tx queue %u SQ state to SQC_STATE_RDY failed.",
			dev->data->port_id, idx);
		goto error;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/*
	 * If using DevX need to query and store TIS transport domain value.
	 * This is done once per port.
	 * Will use this value on Rx, when creating matching TIR.
	 */
	if (!priv->sh->tdn)
		priv->sh->tdn = priv->sh->td->id;
#endif
	MLX5_ASSERT(sh->tx_uar);
	reg_addr = mlx5_os_get_devx_uar_reg_addr(sh->tx_uar);
	MLX5_ASSERT(reg_addr);
	txq_ctrl->bf_reg = reg_addr;
	txq_ctrl->uar_mmap_offset =
				mlx5_os_get_devx_uar_mmap_offset(sh->tx_uar);
	txq_uar_init(txq_ctrl);
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	mlx5_txq_release_devx_resources(txq_obj);
	rte_errno = ret; /* Restore rte_errno. */
	return -rte_errno;
#endif
}

/**
 * Release an Tx DevX queue object.
 *
 * @param txq_obj
 *   DevX Tx queue object.
 */
void
mlx5_txq_devx_obj_release(struct mlx5_txq_obj *txq_obj)
{
	MLX5_ASSERT(txq_obj);
	if (txq_obj->txq_ctrl->type == MLX5_TXQ_TYPE_HAIRPIN) {
		if (txq_obj->tis)
			claim_zero(mlx5_devx_cmd_destroy(txq_obj->tis));
#if defined(HAVE_MLX5DV_DEVX_UAR_OFFSET) || !defined(HAVE_INFINIBAND_VERBS_H)
	} else {
		mlx5_txq_release_devx_resources(txq_obj);
#endif
	}
}

struct mlx5_obj_ops devx_obj_ops = {
	.rxq_obj_modify_vlan_strip = mlx5_rxq_obj_modify_rq_vlan_strip,
	.rxq_obj_new = mlx5_rxq_devx_obj_new,
	.rxq_event_get = mlx5_rx_devx_get_event,
	.rxq_obj_modify = mlx5_devx_modify_rq,
	.rxq_obj_release = mlx5_rxq_devx_obj_release,
	.ind_table_new = mlx5_devx_ind_table_new,
	.ind_table_modify = mlx5_devx_ind_table_modify,
	.ind_table_destroy = mlx5_devx_ind_table_destroy,
	.hrxq_new = mlx5_devx_hrxq_new,
	.hrxq_destroy = mlx5_devx_tir_destroy,
	.hrxq_modify = mlx5_devx_hrxq_modify,
	.drop_action_create = mlx5_devx_drop_action_create,
	.drop_action_destroy = mlx5_devx_drop_action_destroy,
	.txq_obj_new = mlx5_txq_devx_obj_new,
	.txq_obj_modify = mlx5_devx_modify_sq,
	.txq_obj_release = mlx5_txq_devx_obj_release,
};
