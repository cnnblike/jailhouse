/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Configuration for RK3308.
 *
 * Copyright (c) 2024 Ke Li
 *
 * Authors:
 *  Ke Li <cnnblike#gmail.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.
 * See the COPYING file in the top-level directory.
 *
 * Reservation via device tree: reg = <0x0 0x1f400000 0x0 0x00c00000>
 *
 * total reserved 0x1f400000 - 0x1fffffff = 12M
 * hypervisor using 0x1f400000 - 0x1fc00000 = 8M 
 * communication region 0x1fc00000 - 0x1fe00000 = 2M 
 * inmate cell region 0x1fe00000 - 0x1fffffff = 2M
 * 
 * The RK3308 doesn't provide an SMMU, therefore we cannot use cache coloring
 * for DMA memory.
 *
 * The last used interrupt number in the TRM is 148 (SPI 116), so for Jailhouse
 * we use interrupts from 150 (SPI 118). Each vPCI controller uses 4 interrupts.
 * The root cell uses interrupts 150..153 (SPIs 118..121).
 * The first guest cell uses interrupts 153..156 (SPIs 122..125).
 * The GIC supports 128 hardware interrupts (128 SPIs) overall. (RK3308 TRM-Part 1 page 251).
 * 
 * If we want to have an extra one guest cell, consider re-use 147 and 148 which was reserved - then the interrupt used by vPCI should be like [116, 119], [120, 123], [124, 127].
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[4 + 4 + 5];
	struct jailhouse_irqchip irqchips[1];
	struct jailhouse_pci_device pci_devices[2];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.architecture = JAILHOUSE_ARM64,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x1f400000,
			.size = 0x00800000,
		},
		.debug_console = {
			/* uart0 */
			.address = 0xff0a0000,
			.size = 0x1000,
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			// reusing reserved memory region from 0xff840000 - 0xfff80000 in Address Mapping
			.pci_mmconfig_base = 0xff840000,
			.pci_mmconfig_end_bus = 0,
			.pci_is_virtual = 1,
			.pci_domain = -1,
			.color = {
				/* autodetected if not specified */
				/* .way_size = 0x10000, */
				.root_map_offset = 0x0C000000000,
			},
			.arm = {
				// Virtual Maintenance Interrupt for GIC v2 is 25
				.maintenance_irq = 25, 
				.gic_version = 2,
				.gicd_base = 0xff581000,
				.gicc_base = 0xff582000,
				.gich_base = 0xff584000,
				.gicv_base = 0xff586000,
			},
			.memguard = {
				/* num_irqs are just used for output debug info for memguard, 
				   For RK3308, we have following:
				   - 16 SGI, 
				   - 4 PPI
				   - 89 SPI and lots of Interrupt IDs are reserved. 
				   Using 149 below as we have 0 - 148 for Interrupt ID.*/
				.num_irqs = 149,
				// interrupt Id for nCNTHPIRQ is 26
				.hv_timer = 26,
				.irq_prio_min = 0xf0,
				.irq_prio_max = 0x00,
				.irq_prio_step = 0x10,
				.irq_prio_threshold = 0x10,
				.num_pmu_irq = 4,
				/* One PMU irq per CPU */
				// interrupt id for npmuirq is 115, 116, 117, 118
				.pmu_cpu_irq = {
					115, 116, 117, 118,
				},
			},
		},
		.root_cell = {
			.name = "rk3308",
			.num_pci_devices = ARRAY_SIZE(config.pci_devices),
			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			// the max interrupt id is 148
			.vpci_irq_base = 149 - 32,
		},
	},

	.cpus = {
		0b1111,
	},

	.mem_regions = {
		// hypervisor reserved 0x1f400000 - 0x1fc00000

		/* 2 MB memory region from 0x1fc00000 to 0x1fe00000 reserved for communication */
		/* IVSHMEM shared memory regions for 00:00.0 (demo) */
		// 4 regions for 2 peers, bare communication
		// but it's defined using networking macro do for ease of maintain.
		JAILHOUSE_SHMEM_NET_REGIONS(0x1fc00000, 0),
		// The following communication region macro isn't for actual use
		/* IVSHMEM shared memory regions for 00:01.0 (networking) */
		JAILHOUSE_SHMEM_NET_REGIONS(0x1fd00000, 0), /* four regions, size 1MB */

		/* 2 MB memory region from 0x1fe00000 to 0x20000000 reserved for cells */ 
		{
			.phys_start = 0x1fe00000,
			.virt_start = 0x1fe00000,
			.size = 0x00200000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},

		/* normal memory before the Jailhouse reserved memory region */ 
		{
			.phys_start = 0x00000000,
			.virt_start = 0x00000000,
			.size = 0x1f400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
			         JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA,
		},
		
		/* 0xff000000 - 0xff580000 pre gic I/O memory region*/
		{
			.phys_start = 0xff000000,
			.virt_start = 0xff000000,
			.size = 0x00580000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
			         JAILHOUSE_MEM_IO ,
		},
		// leave out gic region
		/* 0xff590000 - 0xff840000 I/O memory region */ 
		{
			.phys_start = 0xff590000,
			.virt_start = 0xff590000,
			.size = 0x002b0000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
			         JAILHOUSE_MEM_IO ,
		},
		// leave out virtual PCI device 0xff840000 - 0xfff80000
		/*{
			.phys_start = 0xff840000,
			.virt_start = 0xff840000,
			.size = 0x00740000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
			         JAILHOUSE_MEM_IO | JAILHOUSE_MEM_DMA,
		},*/
		{
			.phys_start = 0xfff80000,
			.virt_start = 0xfff80000,
			.size = 0x00080000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
			         JAILHOUSE_MEM_IO | JAILHOUSE_MEM_DMA,
		}, 
	},

	.irqchips = {
		/* GIC */ {
			.address = 0xff581000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

	.pci_devices = {
		/* 0001:00:01.0 (demo) */ 
		{
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1,
			// .bdf is in format BBBB:DD:FF
			.bdf = 1 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 0,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,
		},
		/* 0001:00:02.0 (networking) */ 
		{
			.type = JAILHOUSE_PCI_TYPE_IVSHMEM,
			.domain = 1, 
			.bdf = 2 << 3,
			.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,
			.shmem_regions_start = 4,
			.shmem_dev_id = 0,
			.shmem_peers = 2,
			.shmem_protocol = JAILHOUSE_SHMEM_PROTO_VETH,
		},
	},
};
