# Experiment config

See `config_doc.txt` for detail description of each configuration. 

We introduce the detailed configuration of the important scenarios in the paper.

## Traffic Generation

### Incastmix

This is the main traffic pattern that we used in paper.

We run experiments when `FLOW_CDF = 4/13/7/8` , corresponding to `Memcached/Web Server/Hadoop/Web Search` workloads in paper respectively.

The configures related to traffic generation are set as following (note: remove the description when copy into config file):

```
FLOW_FROM_FILE 0
FLOW_CDF 4
FLOW_NUM 50000 {#poisson flows}
LOAD 0.8
INCAST_MIX 720 {#incast flows of one time incast}
INCAST_LOAD 0.5
INCAST_CDF 9
```

You can see `config-dcqcn(-ideal/floodagte).ini, config-dctcp(-ideal/floodagte).ini, config-hpcc(-ideal/floodagte).ini, config-timely(-ideal/floodagte).ini` for detail.

> Note that it is FCT statistics of incast and poisson flows in output files. Therefore, we add some bash scripts (e.g., `getStatisticsAll.sh`, `getBuffer.sh`, `getQueuingDelay.sh`) to get more metrics that we used in paper, e.g., poisson/incast FCT, buffer occupancy, and queuing delay.
> You can type '-h' for help when use these scripts.

### Pure poisson

Again, we run experiments when `FLOW_CDF = 4/13/7/8`. The configures related to traffic generation are set as following (note: remove the description when copy into config file):

```
FLOW_FROM_FILE 0
FLOW_CDF 4
FLOW_NUM 50000
LOAD 0.8
INCAST_MIX 0 {no incast flows}
INCAST_LOAD 0.5
INCAST_CDF 9
```

### Pure incast

Several incast flows arrive once (or many times). The configures related to traffic generation are set as following (note: remove the description when copy into config file):

```
FLOW_FROM_FILE 0
FLOW_CDF 99
FLOW_NUM 1120 {#incast flows}
INCAST_MIX 0
INCAST_CDF 0
INCAST_INTERVAL 30
INCAST_TIME 1
INCAST_SCALE 1
INCAST_FLOW_SIZE 1
```

## Differrent Algorithms

### w/o floodgate

Set `USE_FLOODGATE = 0` to turn off Floodgate.

You can see `config-dcqcn.ini, config-dctcp.ini, config-hpcc.ini, config-timely.ini` for detail.

### w/ ideal

The configures related to Floodgate are set as following:

```
USE_DYNAMIC_PFC_THRESHOLD 0
SRCTOR_DST_PFC 1

USE_FLOODGATE 1
SWTICH_ACK_MODE 1
SWITCH_WIN_M 1.5
SWITCH_ACK_TH_M 1.5
RESET_ONLY_TOR_SWITCH_WIN 0
SWTICH_BYTE_COUNTER 1
SWTICH_CREDIT_INTERVAL 0
SWITCH_ABSOLUTE_PSN 0
SWITCH_SYN_TIMEOUT_US 0
```

You can see `config-dcqcn-ideal.ini, config-dctcp-ideal.ini, config-hpcc-ideal.ini, config-timely-ideal.ini` for detail.

### w/ floodgate

The configures related to Floodgate are set as following:

```
USE_DYNAMIC_PFC_THRESHOLD 1
SRCTOR_DST_PFC 0

USE_FLOODGATE 1
SWTICH_ACK_MODE 2
SWITCH_WIN_M 1.5
SWITCH_ACK_TH_M 1.5
RESET_ONLY_TOR_SWITCH_WIN 0
SWTICH_BYTE_COUNTER 10240
SWTICH_CREDIT_INTERVAL 10
SWITCH_ABSOLUTE_PSN 0
SWITCH_SYN_TIMEOUT_US 0
```

You can see `config-dcqcn-floodgate.ini, config-dctcp-floodgate.ini, config-hpcc-floodgate.ini, config-timely-floodgate.ini` for detail.



