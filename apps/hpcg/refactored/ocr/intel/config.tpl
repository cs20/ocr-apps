# ==========================================================================================================
#
# RMD Strawman ("Sunshine++") configuration file for F_SIM
#
# This file demonstrates how to set various parameters for the simulator to exercise with.
#
# Parameters have scoping rules - closer to actual specific agents in the block over-ride global settings.
#
# For example, the "verbose" flag may be over-ridden by later stages of configuration.  The global setting
# is least-binding, while the per-unit setting is most binding.  So if a specific agent setting says "be
# verbose", then it does not matter what other settings indicated.
#
# ==========================================================================================================
#
# Enumerate the list of machines to be used in distributed simulation runs
#

# ==========================================================================================================
# Set up the basic parameters for a top-level single board of Sunshine to simulate - counts of objects and sizes.

[environment]
   WORKLOAD_INSTALL = .
   WORKLOAD_EXEC = .

[TricksGlobal]
   cfg_area_size = 4                            # KB reserved by the ELF loader process
[MachineGlobal]
   rack_count    = 1                            # number of racks in the machine
   cube_count    = 1                            # number of hypercube 8-socket units per rack
   socket_count  = 1                            # number of populated sockets per hypercube
   trace         = 0                            # level and types of trace info to log in output (0 = off)
   verbose       = false                         # default don't speak about spurious things

[SocketGlobal]
   ipm_count     = 1                            # number of MCs to internal in-package memory per socket
   ipm_size      = 512                           # size in MB per MC for IPM

   dram_count    = 1                            # number of MCs to external DRAM banks per socket
   dram_size     = 2536                         # size in MB per MC for external DRAM

   nvm_count     = 1                            # number of MCs to external NVM banks per socket
   nvm_size      = 128                          # size in MB per MC for external NVM

   cluster_count = 1                            # number of on-die clusters per socket

   fw_img_fname  = $(TG_INSTALL)/lib/fw.img          # file to load
   ce_img_fname  = $(WORKLOAD_INSTALL)/tgkrnl        # file to load
   xe_img_fname  = $(WORKLOAD_INSTALL)/hpcg # file to load
   bin_img_fname = $(WORKLOAD_INSTALL)/hpcg.blob  # file to load

[ipmGlobal]
   verbose       = false
   trace         = 0                            # level and types of trace info to log in output (0 = off)

[dramGlobal]
   verbose       = false

[nvmGlobal]
   verbose       = false

[ClusterGlobal]
   sL3_count     = 1                            # how many logical sL3 units exist per cluster
   sL3_size      = 8                            # size in KB for each sL3 unit per cluster
   verbose       = false
   block_count = BLOCKCOUNT

[sl3Global]
   verbose       = false

[BlockGlobal]
   ce_count      = 1                           # ONLY one CE unit in a block will currently work due to QEMU internals
   xe_count      = 8                           # eight XE units in a block - each XE may have its own section defined below
   nlni_count    = 1                           # one next-level network-interface module per block is expected
   lsqdepth      = 16                          # How many buffer slots does the common network interface use for in/out queues (originating)
   lsqnetdepth   = 16                          # How many buffer slots does the common network interface use for in/out queues (receiving)
   sl2_count     = 1                           # how many logical sL2 entities per block
   sl2_size      = 2048                        # how many KB is each sL2 entity
   sl1_size      = 64                          # how many KB is each sL1 entity
   num_chains    = 8                           # how many concurrent hardware chains does each chain unit support
   num_dma_buffers = 8                         # how many active concurrent DMA operations can be in progress in the DMA unit
   num_dma_ops     = 16                        # how many DMA operations can be floating around per DMA unit
   num_qma_queues  = 8                         # how many QMA queues will each QMA unit offer
   num_qma_ops     = 16                        # how many QMA operations can be in flight around the QMA queues
   verbose       = false                        # global setting: should we be verbose or not? Can be over-ridden per unit below
   trace         = 0x0                    # per bit definitions in tg/common/include/tg-trace-values.h
   cache_defined = 1                           # create a cache module for this agent class
   cache_sizeKB    = 32                        # size in KB for cache module
   cache_assoc   = 4                           # associativity for the cache
   cache_lineSZ  = 64                          # line size in bytes for cache module
   cache_policy  = 0x5                         # policy for the cache; at this time, only mode "0x5" (wite-back, write-allocate) is supported!

   logfilebase   = hpcg.log                  # what is the BASE path+filename to write log files to, default is STDERR

[sl2Global]
   verbose       = false

[CEglobal]
   qemu_args     =                             # command-line to provide to QEMU when it is launched
#   qemu_args     = -s                          # command-line to provide to QEMU when it is launched
#   qemu_args     = -s -S                       # command-line to provide to QEMU when it is launched
   krnl_args     = -mbboot -mbsafepayload      # command-line to provide to XSTGKRNL
   logfile_redir = 0                           # ignore normal logfile routines; redirect all output for these to STDOUT if set to non-zero value
   cache_defined = 1                           # create a cache module for this agent class
   verbose       = false
   trace         = 0x210000000UL # 0x8 #0x320000ULL                  # per bit definitions in ss/common/include/tg-trace.h
#   trace         = 0x0 # 0x8 #0x320000ULL                  # per bit definitions in ss/common/include/tg-trace.h
   enabled       = 1

[XEglobal]
   cache_defined = 1                           # create a cache module for this agent class
   trace         = 0x210000000UL #0xE                  # per bit definitions in ss/common/include/tg-trace.h
#   trace         = 0x0 #0xE                  # per bit definitions in ss/common/include/tg-trace.h
   verbose       = false
   enabled       = 1

# ==========================================================================================================
# Configure the basic parameters of each next-level network-interface unit
#

[NLNIglobal]
   verbose       = false
   trace         = 0                            # level and types of trace info to log in output (0 = off)


