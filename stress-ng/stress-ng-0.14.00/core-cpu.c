/*
 * Copyright (C) 2014-2020 Canonical, Ltd.
 * Copyright (C) 2021-2022 Colin Ian King.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-arch.h"
#include "core-cpu.h"

	/* Name + dest reg */			/* Input -> Output */
#define CPUID_sse3_ECX		(1U << 0)	/* EAX=0x1 -> ECX */
#define CPUID_pclmulqdq_ECX	(1U << 1)	/* EAX=0x1 -> ECX */
#define CPUID_dtes64_ECX	(1U << 2)	/* EAX=0x1 -> ECX */
#define CPUID_monitor_EC	(1U << 3)	/* EAX=0x1 -> ECX */
#define CPUID_ds_cpl_ECX	(1U << 4)	/* EAX=0x1 -> ECX */
#define CPUID_vmx_ECX		(1U << 5)	/* EAX=0x1 -> ECX */
#define CPUID_smx_ECX		(1U << 6)	/* EAX=0x1 -> ECX */
#define CPUID_est_ECX		(1U << 7)	/* EAX=0x1 -> ECX */
#define CPUID_tm2_ECX		(1U << 8)	/* EAX=0x1 -> ECX */
#define CPUID_ssse3_ECX		(1U << 9)	/* EAX=0x1 -> ECX */
#define CPUID_cnxt_id_ECX	(1U << 10)	/* EAX=0x1 -> ECX */
#define CPUID_sdbg_ECX		(1U << 11)	/* EAX=0x1 -> ECX */
#define CPUID_fma_ECX		(1U << 12)	/* EAX=0x1 -> ECX */
#define CPUID_cx16_ECX		(1U << 13)	/* EAX=0x1 -> ECX */
#define CPUID_xtpr_ECX		(1U << 14)	/* EAX=0x1 -> ECX */
#define CPUID_pdcm_ECX		(1U << 15)	/* EAX=0x1 -> ECX */
#define CPUID_pcid_ECX		(1U << 17)	/* EAX=0x1 -> ECX */
#define CPUID_dca_ECX		(1U << 18)	/* EAX=0x1 -> ECX */
#define CPUID_sse4_1_ECX	(1U << 19)	/* EAX=0x1 -> ECX */
#define CPUID_sse4_2_ECX	(1U << 20)	/* EAX=0x1 -> ECX */
#define CPUID_x2apic_ECX	(1U << 21)	/* EAX=0x1 -> ECX */
#define CPUID_movbe_ECX		(1U << 22)	/* EAX=0x1 -> ECX */
#define CPUID_popcnt_ECX	(1U << 23)	/* EAX=0x1 -> ECX */
#define CPUID_tsc_deadline_ECX	(1U << 24)	/* EAX=0x1 -> ECX */
#define CPUID_aes_ECX		(1U << 25)	/* EAX=0x1 -> ECX */
#define CPUID_xsave_ECX		(1U << 26)	/* EAX=0x1 -> ECX */
#define CPUID_osxsave_ECX	(1U << 27)	/* EAX=0x1 -> ECX */
#define CPUID_avx_ECX		(1U << 28)	/* EAX=0x1 -> ECX */
#define CPUID_f16c_ECX		(1U << 29)	/* EAX=0x1 -> ECX */
#define CPUID_rdrnd_ECX		(1U << 30)	/* EAX=0x1 -> ECX */
#define CPUID_hypervisor_ECX	(1U << 26)	/* EAX=0x1 -> ECX */

#define CPUID_fpu_EDX		(1U << 0)	/* EAX=0x1 -> EDX */
#define CPUID_vme_EDX		(1U << 1)	/* EAX=0x1 -> EDX */
#define CPUID_de_EDX		(1U << 2)	/* EAX=0x1 -> EDX */
#define CPUID_pse_EDX		(1U << 3)	/* EAX=0x1 -> EDX */
#define CPUID_tsc_EDX		(1U << 4)	/* EAX=0x1 -> EDX */
#define CPUID_msr_EDX		(1U << 5)	/* EAX=0x1 -> EDX */
#define CPUID_pae_EDX		(1U << 6)	/* EAX=0x1 -> EDX */
#define CPUID_mce_EDX		(1U << 7)	/* EAX=0x1 -> EDX */
#define CPUID_cx8_EDX		(1U << 8)	/* EAX=0x1 -> EDX */
#define CPUID_apic_EDX		(1U << 9)	/* EAX=0x1 -> EDX */
#define CPUID_sep_EDX		(1U << 11)	/* EAX=0x1 -> EDX */
#define CPUID_mtrr_EDX		(1U << 12)	/* EAX=0x1 -> EDX */
#define CPUID_pge_EDX		(1U << 13)	/* EAX=0x1 -> EDX */
#define CPUID_mca_EDX		(1U << 14)	/* EAX=0x1 -> EDX */
#define CPUID_cmov_EDX		(1U << 15)	/* EAX=0x1 -> EDX */
#define CPUID_pat_EDX		(1U << 16)	/* EAX=0x1 -> EDX */
#define CPUID_pse_36_EDX	(1U << 17)	/* EAX=0x1 -> EDX */
#define CPUID_psn_EDX		(1U << 18)	/* EAX=0x1 -> EDX */
#define CPUID_clfsh_EDX		(1U << 19)	/* EAX=0x1 -> EDX */
#define CPUID_ds_EDX		(1U << 21)	/* EAX=0x1 -> EDX */
#define CPUID_acpi_EDX		(1U << 22)	/* EAX=0x1 -> EDX */
#define CPUID_mmx_EDX		(1U << 23)	/* EAX=0x1 -> EDX */
#define CPUID_fxsr_EDX		(1U << 24)	/* EAX=0x1 -> EDX */
#define CPUID_sse_EDX		(1U << 25)	/* EAX=0x1 -> EDX */
#define CPUID_sse2_EDX		(1U << 26)	/* EAX=0x1 -> EDX */
#define CPUID_ss_EDX		(1U << 27)	/* EAX=0x1 -> EDX */
#define CPUID_htt_EDX		(1U << 28)	/* EAX=0x1 -> EDX */
#define CPUID_tm_EDX		(1U << 29)	/* EAX=0x1 -> EDX */
#define CPUID_ia64_EDX		(1U << 30)	/* EAX=0x1 -> EDX */
#define CPUID_pbe_EDX		(1U << 31)	/* EAX=0x1 -> EDX */

#define CPUID_fsgsbase_EBX	(1U << 0)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_sgx_EBX 		(1U << 2)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_bmi1_EBX		(1U << 3)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_hle_EBX 		(1U << 4)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx2_EBX		(1U << 5)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_smep_EBX 		(1U << 7)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_bmi2_EBX 		(1U << 8)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_erms_EBX 		(1U << 9)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_invpcid_EBX 	(1U << 10)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_rtm_EBX 		(1U << 11)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_pqm_EBX 		(1U << 12)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_mpx_EBX 		(1U << 14)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_pqe_EBX 		(1U << 15)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_f_EBX	(1U << 16)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_dq_EBX 	(1U << 17)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_rdseed_EBX	(1U << 18)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_adx_EBX 		(1U << 19)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_smap_EBX 		(1U << 20)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_ifma_EBX	(1U << 21)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_pcommit_EBX 	(1U << 22)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_clflushopt_EBX 	(1U << 23)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_clwb_EBX		(1U << 24)	/* EAX=0x7, ECX=0x0 -> EBX */
#define CPUID_intel_pt_EBX	(1U << 25)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_pf_EBX	(1U << 26)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_er_EBX	(1U << 27)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_cd_EBX	(1U << 28)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_sha_EBX		(1U << 29)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_bw_EBX	(1U << 30)	/* EAX=0x7, EXC=0x0 -> EBX */
#define CPUID_avx512_vl_EBX	(1U << 31)	/* EAX=0x7, EXC=0x0 -> EBX */

#define CPUID_prefetchwt1_ECX	(1U << 0)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_avx512_vbmi_ECX	(1U << 1)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_umip_ECX		(1U << 2)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_pku_ECX		(1U << 3)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_ospke_ECX		(1U << 4)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_waitpkg_ECX	(1U << 5)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_avx512_vbmi2_ECX	(1U << 6)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_cet_ss_ECX	(1U << 7)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_gfni_ECX		(1U << 8)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_vaes_ECX		(1U << 9)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_vclmulqdq_ECX	(1U << 10)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_avx512_vnni_ECX	(1U << 11)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_avx512_bitalg_ECX	(1U << 12)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_tme_en_ECX	(1U << 13)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_avx512_vpopcntdq_ECX (1U << 14)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_rdpid_ECX		(1U << 22)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_kl_ECX		(1U << 23)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_cldemote_ECX	(1U << 25)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_movdiri_ECX	(1U << 27)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_movdir64b_ECX	(1U << 28)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_enqcmd_ECX	(1U << 29)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_sgx_lc_ECX	(1U << 30)	/* EAX=0x7, ECX=0x0, -> ECX */
#define CPUID_pks_ECX		(1U << 31)	/* EAX=0x7, ECX=0x0, -> ECX */

#define CPUID_avx512_4vnniw_EDX	(1U << 2)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_avx512_4fmaps_EDX	(1U << 3)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_fsrm_EDX		(1U << 4)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_avx512_vp2intersect_EDX (1U << 8)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_srbds_ctrl_edx	(1U << 9)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_md_clear_EDX	(1U << 10)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_tsx_force_abort_EDX (1U << 13)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_serialize_EDX	(1U << 14)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_hybrid_EDX	(1U << 15)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_tsxldtrk_EDX	(1U << 16)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_pconfig_EDX	(1U << 18)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_lbr_EDX		(1U << 19)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_cet_ibt_EDX	(1U << 20)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_amx_bf16_EDX	(1U << 22)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_avx512_fp16_EDX	(1U << 23)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_amx_tile_EDX	(1U << 24)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_amx_int8_EDX	(1U << 25)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_ibrs_ibpb_EDX	(1U << 26)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_stip_EDX		(1U << 27)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_l1d_flush_EDX	(1U << 28)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_ia32_arch_cap_EDX	(1U << 29)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_ia32_core_cap_EDX	(1U << 30)	/* EAX=0x7, ECX=0x0, -> EDX */
#define CPUID_ssbd_EDX		(1U << 31)	/* EAX=0x7, ECX=0x0, -> EDX */

#define CPUID_syscall_EDX	(1U << 11)	/* EAX=0x80000001  -> EDX */

/*
 *  stress_x86_cpuid()
 *	cpuid for x86
 */
void stress_x86_cpuid(
	uint32_t *eax,
	uint32_t *ebx,
	uint32_t *ecx,
	uint32_t *edx)
{
#if defined(STRESS_ARCH_X86)
        asm volatile("cpuid"
            : "=a" (*eax),
              "=b" (*ebx),
              "=c" (*ecx),
              "=d" (*edx)
            : "0" (*eax), "2" (*ecx)
            : "memory");
#else
	*eax = 0;
	*ebx = 0;
	*ecx = 0;
	*edx = 0;
#endif
}

/*
 *  stress_cpu_is_x86()
 *	Intel x86 test
 */
bool stress_cpu_is_x86(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0, ebx = 0, ecx = 0, edx = 0;

	/* Intel CPU? */
	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);
	if ((memcmp(&ebx, "Genu", 4) == 0) &&
	    (memcmp(&edx, "ineI", 4) == 0) &&
	    (memcmp(&ecx, "ntel", 4) == 0))
		return true;
	else
		return false;
#else
	return false;
#endif
}

#if defined(STRESS_ARCH_X86)
/*
 *  stress_cpu_x86_extended_features
 *	cpuid EAX=7, ECX=0: Extended Features
 */
static void stress_cpu_x86_extended_features(
	uint32_t *ebx,
	uint32_t *ecx,
	uint32_t *edx)
{
	uint32_t eax = 7;

	stress_x86_cpuid(&eax, ebx, ecx, edx);
}
#endif

/*
 *  stress_cpu_x86_has_clflushopt()
 *	does x86 cpu support clflushopt?
 */
bool stress_cpu_x86_has_clflushopt(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_cpu_x86_extended_features(&ebx, &ecx, &edx);

	return !!(ebx & CPUID_clflushopt_EBX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_clwb()
 *	does x86 cpu support clwb?
 */
bool stress_cpu_x86_has_clwb(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_cpu_x86_extended_features(&ebx, &ecx, &edx);

	return !!(ebx & CPUID_clwb_EBX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_cldemote()
 *	does x86 cpu support cldemote?
 */
bool stress_cpu_x86_has_cldemote(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_cpu_x86_extended_features(&ebx, &ecx, &edx);

	return !!(ecx & CPUID_cldemote_ECX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_waitpkg()
 *	does x86 cpu support waitpkg?
 */
bool stress_cpu_x86_has_waitpkg(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_cpu_x86_extended_features(&ebx, &ecx, &edx);

	return !!(ecx & CPUID_waitpkg_ECX);
#else
	return false;
#endif
}


/*
 *  stress_cpu_x86_has_rdseed()
 *	does x86 cpu support rdseed?
 */
bool stress_cpu_x86_has_rdseed(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_cpu_x86_extended_features(&ebx, &ecx, &edx);

	return !!(ebx & CPUID_rdseed_EBX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_syscall()
 *	does x86 cpu support syscall?
 */
bool stress_cpu_x86_has_syscall(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x80000001, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_syscall_EDX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_rdrand()
 *	does x86 cpu support rdrand?
 */
bool stress_cpu_x86_has_rdrand(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(ecx & CPUID_rdrnd_ECX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_tsc()
 *	does x86 cpu support tsc?
 */
bool stress_cpu_x86_has_tsc(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_tsc_EDX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_msr()
 *	does x86 cpu support MSRs?
 */
bool stress_cpu_x86_has_msr(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_msr_EDX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_clfsh()
 *	does x86 cpu support clflush?
 */
bool stress_cpu_x86_has_clfsh(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_clfsh_EDX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_sse()
 *	does x86 cpu support sse?
 */
bool stress_cpu_x86_has_sse(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_sse_EDX);
#else
	return false;
#endif
}

/*
 *  stress_cpu_x86_has_sse2()
 *	does x86 cpu support sse?
 */
bool stress_cpu_x86_has_sse2(void)
{
#if defined(STRESS_ARCH_X86)
	uint32_t eax = 0x1, ebx = 0, ecx = 0, edx = 0;

	if (!stress_cpu_is_x86())
		return false;

	stress_x86_cpuid(&eax, &ebx, &ecx, &edx);

	return !!(edx & CPUID_sse2_EDX);
#else
	return false;
#endif
}
