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
	int flags =
# ifdef MAP_NOCORE /* huh? */
		MAP_NOCORE |
# endif
		MAP_ANON | MAP_PRIVATE;
	uint8_t *base = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (base == MAP_FAILED)
		bb_die_memory_exhausted();
	//region->base = base;
	//region->base_size = size;
	region->aligned = base;
	region->aligned_size = size;
}

static void free_region(yescrypt_region_t *region)
{
	if (region->aligned)
		munmap(region->aligned, region->aligned_size);
	//region->base = NULL;
	//region->base_size = 0;
	region->aligned = NULL;
	region->aligned_size = 0;
}
