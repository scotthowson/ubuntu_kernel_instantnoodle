/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __VDSO_BITS_H
#define __VDSO_BITS_H

<<<<<<< Updated upstream
#define BIT(nr)			(1UL << (nr))
=======
#ifdef __GENKSYMS__
/*
 * Old version of this macro to preserve the CRC signatures of some drm symbols.
 * Crazy but true...
 */
#define BIT(nr)			(1UL << (nr))
#else
#include <vdso/const.h>

#define BIT(nr)			(UL(1) << (nr))
#endif
>>>>>>> Stashed changes

#endif	/* __VDSO_BITS_H */
