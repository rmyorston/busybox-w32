/*-
 * Copyright 2013-2018,2022 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

static void alloc_region(yescrypt_region_t *region, size_t size)
{
	size_t base_size = size;
	uint8_t *base, *aligned;

#if 0 //def MAP_ANON - use mmap, possibly
//(if defined(MAP_HUGETLB) && defined(MAP_HUGE_2MB)) using 2MB pages
#else /* mmap not available */
	base = aligned = NULL;
	if (size + 63 < size)
		bb_die_memory_exhausted();
	base = malloc(size + 63);
	if (base) {
		aligned = base + 63;
		aligned -= (uintptr_t)aligned & 63;
	}
#endif
	region->base = base;
	region->aligned = aligned;
	region->base_size = base ? base_size : 0;
	region->aligned_size = base ? size : 0;
}

static void free_region(yescrypt_region_t *region)
{
#if 0 //def MAP_ANON
	if (region->base) {
		if (munmap(region->base, region->base_size))
			return -1;
	}
#else
	free(region->base);
#endif
	region->base = NULL;
	region->aligned = NULL;
	region->base_size = 0;
	region->aligned_size = 0;
}
