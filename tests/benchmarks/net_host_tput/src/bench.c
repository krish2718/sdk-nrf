/*
 * Copyright (c) 2026 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/clock_control/nrf_clock_control.h>

#include <stdlib.h>
#include <string.h>

#include <cmsis_core.h>

#define HAS_CPU_DVFS DT_NODE_EXISTS(DT_NODELABEL(cpuapp_hsfll))

#if HAS_CPU_DVFS
static const struct device *const cpu_clk = DEVICE_DT_GET(DT_NODELABEL(cpuapp_hsfll));
static struct nrf_clock_spec cur_spec;
static bool have_req;
#endif

static uint32_t measure_cpu_hz(void)
{
	uint64_t hw_hz = sys_clock_hw_cycles_per_sec();
	uint64_t cycles = 0;
	uint64_t t0;
	uint32_t prev, now;
	unsigned int key;

	/*
	 * DWT CYCCNT does not count on this part without debug authentication,
	 * so use SysTick on the core clock. It is free since GRTC is the system
	 * timer. 24-bit down-counter wraps every ~50 ms at 320 MHz, hence the
	 * tight polling loop.
	 */
	SysTick->LOAD = SysTick_LOAD_RELOAD_Msk;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

	key = irq_lock();
	prev = SysTick->VAL;
	t0 = k_cycle_get_64();
	while ((k_cycle_get_64() - t0) < hw_hz / 4) {
		now = SysTick->VAL;
		cycles += (prev - now) & SysTick_LOAD_RELOAD_Msk;
		prev = now;
	}
	irq_unlock(key);

	SysTick->CTRL = 0;

	return (uint32_t)(cycles * 4U);
}

static int cmd_clk(const struct shell *sh, size_t argc, char **argv)
{
#if !HAS_CPU_DVFS
	/* Fixed app core clock (e.g. nRF7120 HFPLL): report it so sweeps still log cpu_hz. */
	shell_print(sh, "cpu_hz=%u", measure_cpu_hz());

	return 0;
#else
	struct nrf_clock_spec spec = {0};
	struct onoff_client cli;
	int res = 0;
	int ret;

	spec.frequency = MHZ(strtoul(argv[1], NULL, 10));

	sys_notify_init_spinwait(&cli.notify);
	ret = nrf_clock_control_request(cpu_clk, &spec, &cli);
	if (ret < 0) {
		shell_error(sh, "request failed: %d", ret);
		return ret;
	}

	do {
		ret = sys_notify_fetch_result(&cli.notify, &res);
		k_yield();
	} while (ret == -EAGAIN);

	if (ret || res) {
		shell_error(sh, "clock change failed: %d/%d", ret, res);
		return -EIO;
	}

	if (have_req) {
		nrf_clock_control_release(cpu_clk, &cur_spec);
	}

	cur_spec = spec;
	have_req = true;

	shell_print(sh, "cpu_hz=%u", measure_cpu_hz());

	return 0;
#endif
}

static int cmd_freq(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "cpu_hz=%u", measure_cpu_hz());

	return 0;
}

/* Internal to subsys/net/ip (net_private.h); used here to price checksum offload. */
uint16_t calc_chksum(uint16_t sum_in, const uint8_t *data, size_t len);

static uint8_t src_buf[1536] __aligned(4);
static uint8_t dst_buf[1536] __aligned(4);
static volatile uint16_t sink;

static int cmd_perbyte(const struct shell *sh, size_t argc, char **argv, bool csum)
{
	size_t len = strtoul(argv[1], NULL, 10);
	uint32_t iters = strtoul(argv[2], NULL, 10);
	uint32_t hz = measure_cpu_hz();
	uint64_t t0, us;
	unsigned int key;

	if (len == 0 || len > sizeof(src_buf) || iters == 0) {
		return -EINVAL;
	}

	for (size_t i = 0; i < sizeof(src_buf); i++) {
		src_buf[i] = (uint8_t)i;
	}

	key = irq_lock();
	t0 = k_cycle_get_64();
	for (uint32_t i = 0; i < iters; i++) {
		if (csum) {
			sink = calc_chksum(0, src_buf, len);
		} else {
			memcpy(dst_buf, src_buf, len);
			sink = dst_buf[i % len];
		}
	}
	us = k_cyc_to_us_floor64(k_cycle_get_64() - t0);
	irq_unlock(key);

	/* Fixed-point x1000 to keep float printing out of the shell. */
	shell_print(sh, "%s len=%u cpu_hz=%u cycles_per_call=%llu milli_cycles_per_byte=%llu",
		    csum ? "csum" : "memcpy", len, hz,
		    us * hz / 1000000ULL / iters,
		    us * hz / 1000ULL / iters / len);

	return 0;
}

static int cmd_csum(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_perbyte(sh, argc, argv, true);
}

static int cmd_memcpy(const struct shell *sh, size_t argc, char **argv)
{
	return cmd_perbyte(sh, argc, argv, false);
}

#if defined(NRF_MRAMC) && defined(MRAMC_WAITSTATES_WAITSTATENUM_Msk)
#define HAS_MRAMC_WS 1
#else
#define HAS_MRAMC_WS 0
#endif

#if defined(NRF_ICACHE) && defined(CACHE_PROFILING_ENABLE_ENABLE_Msk)
#define HAS_ICACHE_PROF 1
#else
#define HAS_ICACHE_PROF 0
#endif

#if HAS_MRAMC_WS
static uint32_t mram_ws_get(void)
{
	return (NRF_MRAMC->WAITSTATES & MRAMC_WAITSTATES_WAITSTATENUM_Msk) >>
	       MRAMC_WAITSTATES_WAITSTATENUM_Pos;
}
#endif

static int cmd_mem(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "cpu_hz=%u", measure_cpu_hz());
#if HAS_MRAMC_WS
	shell_print(sh, "mramc_waitstates=0x%08x mram_ws=%u mramc_config=0x%08x",
		    NRF_MRAMC->WAITSTATES, mram_ws_get(), NRF_MRAMC->CONFIG);
#endif
#if HAS_ICACHE_PROF
	shell_print(sh, "icache_enable=%u icache_status=0x%08x prof_enable=%u",
		    NRF_ICACHE->ENABLE, NRF_ICACHE->STATUS, NRF_ICACHE->PROFILING.ENABLE);
#endif
	return 0;
}

static int cmd_ws(const struct shell *sh, size_t argc, char **argv)
{
#if HAS_MRAMC_WS
	uint32_t ws = strtoul(argv[1], NULL, 10);
	uint32_t reg;

	if (ws < MRAMC_WAITSTATES_WAITSTATENUM_Min || ws > MRAMC_WAITSTATES_WAITSTATENUM_Max) {
		shell_error(sh, "ws out of range %lu..%lu", MRAMC_WAITSTATES_WAITSTATENUM_Min,
			    MRAMC_WAITSTATES_WAITSTATENUM_Max);
		return -EINVAL;
	}

	/* KEY is write-only; the other control bits must be written back unchanged. */
	reg = NRF_MRAMC->WAITSTATES & ~(MRAMC_WAITSTATES_WAITSTATENUM_Msk |
					MRAMC_WAITSTATES_KEY_Msk);
	reg |= (ws << MRAMC_WAITSTATES_WAITSTATENUM_Pos) |
	       (MRAMC_WAITSTATES_KEY_Enable << MRAMC_WAITSTATES_KEY_Pos);
	NRF_MRAMC->WAITSTATES = reg;
	__DSB();
	__ISB();

	shell_print(sh, "mram_ws=%u", mram_ws_get());
	return 0;
#else
	return -ENOTSUP;
#endif
}

static int cmd_icache(const struct shell *sh, size_t argc, char **argv)
{
#if HAS_ICACHE_PROF
	bool on = strcmp(argv[1], "on") == 0;

	if (on) {
		NRF_ICACHE->TASKS_INVALIDATECACHE = 1;
		while (NRF_ICACHE->STATUS & CACHE_STATUS_READY_Msk) {
		}
	}
	NRF_ICACHE->ENABLE = on;
	__DSB();
	__ISB();

	shell_print(sh, "icache_enable=%u", NRF_ICACHE->ENABLE);
	return 0;
#else
	return -ENOTSUP;
#endif
}

static int cmd_prof(const struct shell *sh, size_t argc, char **argv)
{
#if HAS_ICACHE_PROF
	if (strcmp(argv[1], "clear") == 0) {
		NRF_ICACHE->PROFILING.ENABLE = 1;
		NRF_ICACHE->PROFILING.CLEAR = 1;
		return 0;
	}

	shell_print(sh, "icache_hit=%u icache_miss=%u", NRF_ICACHE->PROFILING.HIT,
		    NRF_ICACHE->PROFILING.MISS);
	return 0;
#else
	return -ENOTSUP;
#endif
}

SHELL_STATIC_SUBCMD_SET_CREATE(bench_cmds,
	SHELL_CMD_ARG(clk, NULL, "Request app core clock: clk <MHz>", cmd_clk, 2, 0),
	SHELL_CMD(freq, NULL, "Measure app core clock", cmd_freq),
	SHELL_CMD_ARG(csum, NULL, "Time calc_chksum: csum <len> <iters>", cmd_csum, 3, 0),
	SHELL_CMD_ARG(memcpy, NULL, "Time memcpy: memcpy <len> <iters>", cmd_memcpy, 3, 0),
	SHELL_CMD(mem, NULL, "Dump MRAM wait states and I-cache state", cmd_mem),
	SHELL_CMD_ARG(ws, NULL, "Set MRAM read wait states: ws <n>", cmd_ws, 2, 0),
	SHELL_CMD_ARG(icache, NULL, "I-cache: icache <on|off>", cmd_icache, 2, 0),
	SHELL_CMD_ARG(prof, NULL, "I-cache profiling: prof <clear|show>", cmd_prof, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(bench, &bench_cmds, "Throughput benchmark helpers", NULL);
