/*
 * svector.c
 *
 * Vector handling.
 *
 * Copyright (c) 2015 F. Aragon. All rights reserved.
 */ 

#include "svector.h"
#include "scommon.h"

#ifndef SV_DEFAULT_SIGNED_VAL
#define SV_DEFAULT_SIGNED_VAL 0
#endif
#ifndef SV_DEFAULT_UNSIGNED_VAL
#define SV_DEFAULT_UNSIGNED_VAL 0
#endif

/*
 * Static functions forward declaration
 */

static sd_t *aux_dup_sd(const sd_t *d);
static sv_t *sv_check(sv_t **v);

/*
 * Constants
 */

static size_t svt_sizes[SV_LAST+1] = {	sizeof(char),
					sizeof(unsigned char),
					sizeof(short),
					sizeof(unsigned short),
					sizeof(int),
					sizeof(unsigned int),
					sizeof(sint_t),
					sizeof(suint_t),
					0
					};
static struct SVector sv_void0 = EMPTY_SV;
static sv_t *sv_void = (sv_t *)&sv_void0;
static struct sd_conf svf = {	(size_t (*)(const sd_t *))__sv_get_max_size,
				NULL,
				NULL,
				NULL,
				(sd_t *(*)(const sd_t *))aux_dup_sd,
				(sd_t *(*)(sd_t **))sv_check,
				(sd_t *)&sv_void0,
				0,
				0,
				0,
				0,
				0,
				sizeof(struct SVector),
				offsetof(struct SVector, elem_size),
				};

/*
 * Push-related function pointers
 */

#define SV_STx(f, TS, TL)		\
	static void f(void *st, TL *c)	\
	{				\
		*(TS *)st = (TS)*c;	\
	}

SV_STx(svsti8, char, const sint_t)
SV_STx(svstu8, unsigned char, const suint_t)
SV_STx(svsti16, short, const sint_t)
SV_STx(svstu16, unsigned short, const suint_t)
SV_STx(svsti32, sint32_t, const sint_t)
SV_STx(svstu32, suint32_t, const suint_t)
SV_STx(svsti64, sint_t, const sint_t)
SV_STx(svstu64, suint_t, const suint_t)

static void svstvoid(void *st, const sint_t *c)
{ /* do nothing */
}

typedef void (*T_SVSTX)(void *, const sint_t *);

static T_SVSTX svstx_f[SV_LAST + 1] = {	svsti8, (T_SVSTX)svstu8,
					svsti16, (T_SVSTX)svstu16,
					svsti32, (T_SVSTX)svstu32,
					svsti64, (T_SVSTX)svstu64,
					svstvoid
					};

/*
 * Pop-related function pointers
 */

#define SV_LDx(f, TL, TO)						\
	static TO *f(const void *ld, TO *out, const size_t index)	\
	{								\
		 *out = (TO)((TL *)ld)[index];				\
		return out;						\
	}

SV_LDx(svldi8, char, sint_t)
SV_LDx(svldu8, unsigned char, suint_t)
SV_LDx(svldi16, short, sint_t)
SV_LDx(svldu16, unsigned short, suint_t)
SV_LDx(svldi32, sint32_t, sint_t)
SV_LDx(svldu32, suint32_t, suint_t)
SV_LDx(svldi64, sint_t, sint_t)
SV_LDx(svldu64, suint_t, suint_t)

sint_t *svldvoid(const void *ld, sint_t *out, const size_t index)
{ /* dummy function */
	*out = SV_DEFAULT_SIGNED_VAL;
	return out;
}

typedef sint_t *(*T_SVLDX)(const void *, sint_t *, const size_t);

static T_SVLDX svldx_f[SV_LAST + 1] = {	svldi8, (T_SVLDX)svldu8,
					svldi16, (T_SVLDX)svldu16,
					svldi32, (T_SVLDX)svldu32,
					svldi64, (T_SVLDX)svldu64,
					svldvoid
					};

/*
 * Internal functions
 */

static size_t get_size(const sv_t *v)
{
	return ((struct SData_Full *)v)->size; /* faster than sd_get_size */
}

static void set_size(sv_t *v, const size_t size)
{
	if (v)
		((struct SData_Full *)v)->size = size; /* faster than sd_set_size */
}

static size_t get_alloc_size(const sv_t *v)
{
	return !v ? 0 : SDV_HEADER_SIZE + v->elem_size * get_size(v);
}

static sv_t *sv_alloc_base(const enum eSV_Type t, const size_t elem_size,
			   const size_t initial_num_elems_reserve)
{
	const size_t alloc_size = SDV_HEADER_SIZE + elem_size *
						    initial_num_elems_reserve;
	return sv_alloc_raw(t, elem_size, S_FALSE, malloc(alloc_size), alloc_size);
}

static void sv_copy_elems(sv_t *v, const size_t v_off, const sv_t *src,
		     const size_t src_off, const size_t n)
{
	s_copy_elems(__sv_get_buffer(v), v_off, __sv_get_buffer_r(src),
		     src_off, n, v->elem_size);
}

static void sv_move_elems(sv_t *v, const size_t v_off, const sv_t *src,
		     const size_t src_off, const size_t n)
{
	s_move_elems(__sv_get_buffer(v), v_off, __sv_get_buffer_r(src),
		     src_off, n, v->elem_size);
}

static sv_t *sv_check(sv_t **v)
{
	ASSERT_RETURN_IF(!v || !*v, sv_void);
	return *v;
}

static sv_t *sv_clear(sv_t **v)
{
	ASSERT_RETURN_IF(!v, sv_void);
	if (!*v)
		*v = sv_void;
	else
		set_size(*v, 0);
	return *v;
}

static sv_t *aux_dup(const sv_t *src, const size_t n_elems)
{
	const size_t ss = get_size(src),
		     size = n_elems < ss ? n_elems : ss;
	sv_t *v = src->sv_type == SV_GEN ?
			sv_alloc(src->elem_size, ss) :
			sv_alloc_t((enum eSV_Type)src->sv_type, ss);
	if (v) {
		sv_copy_elems(v, 0, src, 0, size);
		set_size(v, size);
	} else {
		v = sv_void;	/* alloc error */
	}
	return v;
}

static sd_t *aux_dup_sd(const sd_t *d)
{
	return (sd_t *)aux_dup((const sv_t *)d, sd_get_size(d));
}

static sv_t *aux_cat(sv_t **v, const sbool_t cat, const sv_t *src, const size_t ss)
{
	ASSERT_RETURN_IF(!v, sv_void);
	if (!*v)  /* duplicate source */
		return *v = (sv_t *)aux_dup_sd((const sd_t *)src);
	if (src && (*v)->sv_type == src->sv_type) {
		const sbool_t aliasing = *v == src;
		const size_t at = (cat && *v) ? get_size(*v) : 0,
			     out_size = at + ss;
		if (sv_reserve(v, out_size) >= out_size) {
			if (!aliasing)
				sv_copy_elems(*v, at, src, 0, ss);
			else
				if (at > 0)
					sv_move_elems(*v, at, *v, 0, ss);
			set_size(*v, at + ss);
		}
	} else {
		return cat ? sv_check(v) : sv_clear(v);
	}
	return *v;
}

static size_t aux_reserve(sv_t **v, const sv_t *src, const size_t max_elems)
{
	ASSERT_RETURN_IF(!v, 0);
	const sv_t *s = *v && *v != sv_void ? *v : src;
	ASSERT_RETURN_IF(!s, 0);
	if (s == src) { /* reserve from NULL/sv_void */
		*v = src->sv_type == SV_GEN ?
			sv_alloc(src->elem_size, max_elems) :
			sv_alloc_t((const enum eSV_Type)src->sv_type, max_elems);
		return __sv_get_max_size(*v);
	}
	return sd_reserve((sd_t **)v, max_elems, &svf);
}

static size_t aux_grow(sv_t **v, const sv_t *src, const size_t extra_elems)
{
	ASSERT_RETURN_IF(!v, 0);
	ASSERT_RETURN_IF(!*v || *v == sv_void, aux_reserve(v, src, extra_elems));
	return sv_grow(v, extra_elems);
}

static sv_t *aux_erase(sv_t **v, const sbool_t cat, const sv_t *src,
                       const size_t off, const size_t n)
{
	ASSERT_RETURN_IF(!v, sv_void);
	if (!src)
		src = sv_void;
	const size_t ss0 = get_size(src),
	at = (cat && *v) ? get_size(*v) : 0;
	const sbool_t overflow = off + n > ss0;
	const size_t src_size = overflow ? ss0 - off : n,
	copy_size = ss0 - off - src_size;
	if (*v == src) { /* BEHAVIOR: aliasing: copy-only */
		if (off + n >= ss0) { /* tail clean cut */
			set_size(*v, off);
		} else {
			sv_move_elems(*v, off, *v, off + n, n);
			set_size(*v, ss0 - n);
		}
	} else { /* copy or cat */
		const size_t out_size = at + off + copy_size;
		if (aux_reserve(v, src, out_size) >= out_size) {
			sv_copy_elems(*v, at, src, 0, off);
			sv_copy_elems(*v, at + off, src, off + n, copy_size);
			set_size(*v, out_size);
		} else {
		}
	}
	return sv_check(v);
}

static sv_t *aux_resize(sv_t **v, const sbool_t cat, const sv_t *src,
			const size_t n0)
{
	ASSERT_RETURN_IF(!v, sv_void);
	ASSERT_RETURN_IF(!src, (cat ? sv_check(v) : sv_clear(v)));
	const size_t src_size = get_size(src),
		     at = (cat && *v) ? get_size(*v) : 0;
	const size_t n = n0 < src_size ? n0 : src_size;
	if (!s_size_t_overflow(at, n)) { /* overflow check */
		const size_t out_size = at + n;
		const sbool_t aliasing = *v == src;
		if (sv_reserve(v, out_size) >= out_size) {
			if (!aliasing) {
				void *po = __sv_get_buffer(*v);
				const void *psrc = __sv_get_buffer_r(src);
				const size_t elem_size = src->elem_size;
				s_copy_elems(po, at, psrc, 0, n, elem_size);
			}
			set_size(*v, out_size);
		}
	}
	return *v;
}

static char *ptr_to_elem(sv_t *v, const size_t i)
{
	return (char *)__sv_get_buffer(v) + i * v->elem_size;
}

static const char *ptr_to_elem_r(const sv_t *v, const size_t i)
{
	return (const char *)__sv_get_buffer_r(v) + i * v->elem_size;
}

/*
 * Allocation
 */

/*
#API: |Allocate typed vector in the stack|Vector type: SV_I8/SV_U8/SV_I16/SV_U16/SV_I32/SV_U32/SV_I64/SV_U64; space preallocated to store n elements|vector|O(1)|
sv_t *sv_alloca_t(const enum eSV_Type t, const size_t initial_num_elems_reserve)

#API: |Allocate generic vector in the stack|element size; space preallocated to store n elements|vector|O(1)|
sv_t *sv_alloca(const size_t elem_size, const size_t initial_num_elems_reserve)
*/

sv_t *sv_alloc_raw(const enum eSV_Type t, const size_t elem_size,
		   const sbool_t ext_buf, void *buffer,
		   const size_t buffer_size)
{
	RETURN_IF(!elem_size || !buffer, sv_void);
        sv_t *v = (sv_t *)buffer;
	sd_reset((sd_t *)v, S_TRUE, buffer_size, ext_buf);
	v->sv_type = t;
	v->elem_size = elem_size;
	v->aux = v->aux2 = 0;
	return v;
}

/* #API: |Allocate generic vector in the heap|element size; space preallocated to store n elements|vector|O(1)| */

sv_t *sv_alloc(const size_t elem_size, const size_t initial_num_elems_reserve)
{
	return sv_alloc_base(SV_GEN, elem_size, initial_num_elems_reserve);
}

/* #API: |Allocate typed vector in the heap|Vector type: SV_I8/SV_U8/SV_I16/SV_U16/SV_I32/SV_U32/SV_I64/SV_U64; space preallocated to store n elements|vector|O(1)| */

sv_t *sv_alloc_t(const enum eSV_Type t, const size_t initial_num_elems_reserve)
{
	return sv_alloc_base(t, sv_elem_size(t), initial_num_elems_reserve);
}

SD_BUILDFUNCS(sv)

/*
#API: |Free one or more vectors|vector;more vectors (optional)||O(1)|
void sv_free(sv_t **c, ...)

#API: |Ensure space for extra elements|vector;number of extra eelements|extra size allocated|O(1)|
size_t sv_grow(sv_t **c, const size_t extra_elems)

#API: |Ensure space for elements|vector;absolute element reserve|reserved elements|O(1)|
size_t sv_reserve(sv_t **c, const size_t max_elems)

#API: |Free unused space|vector|same vector (optional usage)|O(1)|
sv_t *sv_shrink(sv_t **c)

#API: |Get vector size|vector|vector bytes used in UTF8 format|O(1)|
size_t sv_get_size(const sv_t *c)

#API: |Set vector size (bytes used in UTF8 format)|vector;set vector number of elements||O(1)|
void sv_set_size(sv_t *c, const size_t s)

#API: |Equivalent to sv_get_size|vector|Number of bytes (UTF-8 vector length)|O(1)|
size_t sv_get_len(const sv_t *c)

#API: |Allocate vector (stack)|space preAllocated to store n elements|allocated vector|O(1)|
sv_t *sv_alloca(const size_t initial_reserve)
*/

/*
 * Accessors
 */

/* #API: |Allocated space|vector|current allocated space (vector elements)|O(1)| */

size_t sv_capacity(const sv_t *v)
{
	return !v || !v->elem_size ?
		0 : (v->df.alloc_size - SDV_HEADER_SIZE) / v->elem_size;
}

/* #API: |Preallocated space left|vector|allocated space left|O(1)| */

size_t sv_len_left(const sv_t *v)
{
	return !v? 0 : sv_capacity(v) - sv_len(v);
}

/* #API: |Explicit set length (intended for external I/O raw acccess)|vector;new length||O(1)| */

sbool_t sv_set_len(sv_t *v, const size_t elems)
{
	const size_t max_size = __sv_get_max_size(v);
	const sbool_t resize_ok = elems <= max_size ? S_TRUE : S_FALSE;
	sd_set_size((sd_t *)v, S_MIN(elems, max_size));
	return resize_ok;
}

/* #API: |Get string buffer read-only access|string|pointer to the insternal string buffer (UTF-8 or raw data)|O(1)| */

const void *sv_get_buffer_r(const sv_t *v)
{
	return !v ? 0 : __sv_get_buffer_r(v);
}

/* #API: |Get string buffer access|string|pointer to the insternal string buffer (UTF-8 or raw data)|O(1)| */

void *sv_get_buffer(sv_t *v)
{
	return !v ? 0 : __sv_get_buffer(v);
}

/* #API: |Get buffer size|vector|Number of bytes in use for current vector elements|O(1)| */

size_t sv_get_buffer_size(const sv_t *v)
{
	return sv_len(v) * SDV_HEADER_SIZE;
}

/* #API: |Get vector type element size|vector type|Element size (bytes)|O(1)| */

size_t sv_elem_size(const enum eSV_Type t)
{
	return t >= SV_I8 && t <= SV_GEN ? svt_sizes[t] : 0;
}

/*
 * Allocation from other sources: "dup"
 */

/* #API: |Duplicate vector|vector|output vector|O(n)| */

sv_t *sv_dup(const sv_t *src)
{
	sv_t *v = NULL;
	return sv_cpy(&v, src);
}

/* #API: |Duplicate vector portion|vector; offset start; number of elements to take|output vector|O(n)| */

sv_t *sv_dup_erase(const sv_t *src, const size_t off, const size_t n)
{
	sv_t *v = NULL;
	return aux_erase(&v, S_FALSE, src, off, n);
}

/* #API: |Duplicate vector with resize operation|vector; size for the output|output vector|O(n)| */

sv_t *sv_dup_resize(const sv_t *src, const size_t n)
{
	sv_t *v = NULL;
	return aux_resize(&v, S_FALSE, src, n);
}

/*
 * Assignment
 */

/* #API: |Overwrite vector with a vector copy|output vector; input vector|output vector reference (optional usage)|O(n)| */

sv_t *sv_cpy(sv_t **v, const sv_t *src)
{
	return aux_cat(v, S_FALSE, src, sv_len(src));
}

/* #API: |Overwrite vector with input vector copy applying a erase operation|output vector; input vector; input vector erase start offset; number of elements to erase|output vector reference (optional usage)|O(n)| */

sv_t *sv_cpy_erase(sv_t **v, const sv_t *src, const size_t off, const size_t n)
{
	return aux_erase(v, S_FALSE, src, off, n);
}

/* #API: |Overwrite vector with input vector copy plus resize operation|output vector; input vector; number of elements of input vector|output vector reference (optional usage)|O(n)| */

sv_t *sv_cpy_resize(sv_t **v, const sv_t *src, const size_t n)
{
	return aux_resize(v, S_FALSE, src, n);
}

/*
 * Append
 */

/*
#API: |Concatenate vectors to vector|vector; first vector to be concatenated; optional: N additional vectors to be concatenated|output vector reference (optional usage)|O(n)|
sv_t *sv_cat(sv_t **v, const sv_t *v1, ...)
*/

sv_t *sv_cat_aux(sv_t **v, const size_t nargs, const sv_t *v1, ...)
{
	ASSERT_RETURN_IF(!v, sv_void);
	const sv_t *v0 = *v;
	const size_t v0s = v0 ? get_size(v0) : 0;
	if (aux_grow(v, v1, nargs)) {
		const sv_t *v1a = v1 == v0 ? *v : v1;
		const size_t v1as = v1 == v0 ? v0s : sv_len(v1);
		if (nargs == 1)
			return aux_cat(v, S_TRUE, v1a, v1as);
		size_t i = 1;
		va_list ap;
		va_start(ap, v1);
		aux_cat(v, S_TRUE, v1a, v1as);
		for (; i < nargs; i++) {
			const sv_t *vnext = va_arg(ap, const sv_t *);
			const sv_t *vnexta = vnext == v0 ? *v : vnext;
			const size_t vns = vnext == v0 ? v0s : sv_len(vnext);
			aux_cat(v, S_TRUE, vnexta, vns);
		}
		va_end(ap);
	}
	return sv_check(v);
}

/* #API: |Concatenate vector with erase operation|output vector; input vector; input vector offset for erase start; erase element count|output vector reference (optional usage)|O(n)| */

sv_t *sv_cat_erase(sv_t **v, const sv_t *src, const size_t off, const size_t n)
{
	return aux_erase(v, S_TRUE, src, off, n);
}

/* #API: |Concatenate vector with input vector copy plus resize operation|output vector; input vector; number of elements of input vector|output vector reference (optional usage)|O(n)| */

sv_t *sv_cat_resize(sv_t **v, const sv_t *src, const size_t n)
{
	return aux_resize(v, S_TRUE, src, n);
}

/*
 * Transformation
 */

/* #API: |Erase portion of a vector|input/output vector; element offset where to start the cut; number of elements to be cut|output vector reference (optional usage)|O(n)| */

sv_t *sv_erase(sv_t **v, const size_t off, const size_t n)
{
	return aux_erase(v, S_FALSE, *v, off, n);
}

/* #API: |Resize vector|input/output vector; new size|output vector reference (optional usage)|O(n)| */

sv_t *sv_resize(sv_t **v, const size_t n)
{
	return aux_resize(v, S_FALSE, *v, n);
}

/*
 * Search
 */

/* #API: |Find value in vector (generic data)|vector; search offset start; target to be located|offset: >=0 found; S_NPOS: not found|O(n)| */

size_t sv_find(const sv_t *v, const size_t off, const void *target)
{
	ASSERT_RETURN_IF(!v || v->sv_type < SV_I8 || v->sv_type > SV_GEN, S_NPOS);
	size_t pos = S_NPOS;
	const size_t size = get_size(v);
	const void *p = __sv_get_buffer_r(v); /* in order to do pointer arithmetic */
	const size_t elem_size = v->elem_size;
	const size_t off_max = size * v->elem_size;
	size_t i = off * elem_size;
	for (; i < off_max; i += elem_size)
		if (!memcmp((const char *)p + i, target, elem_size))
			return i / elem_size;	/* found */
	return pos;
}

#define SV_FIND_iu(v, off, target)						\
	ASSERT_RETURN_IF(!v || v->sv_type < SV_I8 || v->sv_type > SV_U64, S_NPOS);	\
	char i8; unsigned char u8; short i16; unsigned short u16;		\
	int i32; unsigned u32; sint_t i64; suint_t u64;				\
	void *src;								\
	switch (v->sv_type) {							\
	case SV_I8: i8 = (char)target; src = &i8; break;			\
	case SV_U8: u8 = (unsigned char)target; src = &u8; break;		\
	case SV_I16: i16 = (short)target; src = &i16; break;			\
	case SV_U16: u16 = (unsigned short)target; src = &u16; break;		\
	case SV_I32: i32 = (int)target; src = &i32; break;			\
	case SV_U32: u32 = (unsigned)target; src = &u32; break;			\
	case SV_I64: i64 = (sint_t)target; src = &i64; break;			\
	case SV_U64: u64 = (suint_t)target; src = &u64; break;			\
	default: src = NULL;											\
	}

/* #API: |Find value in vector (integer)|vector; search offset start; target to be located|offset: >=0 found; S_NPOS: not found|O(n)| */

size_t sv_find_i(const sv_t *v, const size_t off, const sint_t target)
{
	SV_FIND_iu(v, off, target);
	return sv_find(v, off, src);
}

/* #API: |Find value in vector (unsigned integer)|vector; search offset start; target to be located|offset: >=0 found; S_NPOS: not found|O(n)| */

size_t sv_find_u(const sv_t *v, const size_t off, const suint_t target)
{
	SV_FIND_iu(v, off, target);
	return sv_find(v, off, src);
}

#undef SV_FIND_iu

/*
 * Compare
 */

/* #API: |Compare two vectors|vector #1; vector #1 offset start; vector #2; vector #2 start; compare size|0: equals; < 0 if a < b; > 0 if a > b|O(n)| */

int sv_ncmp(const sv_t *v1, const size_t v1off, const sv_t *v2, const size_t v2off, const size_t n)
{
	ASSERT_RETURN_IF(!v1 && !v2, 0);
	ASSERT_RETURN_IF(!v1, -1);
	ASSERT_RETURN_IF(!v2, 1);
	const size_t sv1 = get_size(v1), sv2 = get_size(v2),
		     sv1_left = v1off < sv1 ? sv1 - v1off : 0,
		     sv2_left = v2off < sv2 ? sv2 - v2off : 0,
		     cmp_bytes = v1->elem_size * S_MIN3(n, sv1_left, sv2_left);
	const char *v1p = ptr_to_elem_r(v1, v1off),
		   *v2p = ptr_to_elem_r(v2, v2off);
	return memcmp(v1p, v2p, cmp_bytes);
}

/*
 * Vector "at": element access to given position
 */

/* #API: |Vector random access (generic data)|vector; location|NULL: not found; != NULL: element reference|O(1)| */

const void *sv_at(const sv_t *v, const size_t index)
{
	RETURN_IF(!v, sv_void);
	return (const void *)ptr_to_elem_r(v, index);
}

#define SV_IU_AT(T, def_val)					\
	ASSERT_RETURN_IF(!v, def_val);				\
	const size_t size = get_size(v);			\
	RETURN_IF(index >= size, def_val);			\
	const void *p = __sv_get_buffer_r(v);			\
	sint_t tmp;						\
	return *(T *)(svldx_f[v->sv_type](p, &tmp, index));

/* #API: |Vector random access (integer)|vector; location|Element value|O(1)| */

sint_t sv_i_at(const sv_t *v, const size_t index)
{
	SV_IU_AT(sint_t, SV_DEFAULT_SIGNED_VAL);
}

/* #API: |Vector random access (unsigned integer)|vector; location|Element value|O(1)| */

suint_t sv_u_at(const sv_t *v, const size_t index)
{
	SV_IU_AT(suint_t, SV_DEFAULT_UNSIGNED_VAL);
}

#undef SV_IU_AT

/*
 * Vector "push": add element in the last position
 */

#define SV_PUSH_START(n)					\
	ASSERT_RETURN_IF(!v || !sv_grow(v, n) || !*v, S_FALSE); \
	const size_t sz = get_size(*v);				\
	const size_t elem_size = (*v)->elem_size;		\
	char *p = ptr_to_elem(*v, sz);

#define SV_PUSH_END(n)	\
	set_size(*v, sz + n);

/* #API: |Push/add element (generic data)|vector; data source; number of elements|S_TRUE: added OK; S_FALSE: not enough memory|O(1)| */

sbool_t sv_push_raw(sv_t **v, const void *src, const size_t n)
{
	ASSERT_RETURN_IF(!src || !n, S_FALSE);
	SV_PUSH_START(n);
	SV_PUSH_END(n);
	memcpy(p, src, elem_size * n);
	return S_TRUE;
}

/*
#API: |Push/add multiple elements (generic data)|vector; new element to be added; more elements to be added (optional)|S_TRUE: added OK; S_FALSE: not enough memory|O(1) for one element; O(n) generic case|
sbool_t sv_push(sv_t **v, const void *c1, ...)
*/

size_t sv_push_aux(sv_t **v, const size_t nargs, const void *c1, ...)
{
	ASSERT_RETURN_IF(!v || !*v || !nargs, S_FALSE);
	size_t op_cnt = 0;
	va_list ap;
	if (sv_grow(v, nargs) == 0)
		return op_cnt;
	op_cnt += (sv_push_raw(v, c1, 1) ? 1 : 0);
	if (nargs == 1) /* just one elem: avoid loop */	
		return op_cnt;
	va_start(ap, c1);
	size_t i = 1;
	for (; i < nargs; i++) {
		const void *next = (const void *)va_arg(ap, const void *);
		op_cnt += (sv_push_raw(v, next, 1) ? 1 : 0);
	}
	va_end(ap);
	return op_cnt;
}

/* #API: |Push/add element (integer)|vector; data source|S_TRUE: added OK; S_FALSE: not enough memory|O(1)| */

sbool_t sv_push_i(sv_t **v, const sint_t c)
{
	SV_PUSH_START(1);
	SV_PUSH_END(1);
	svstx_f[(*v)->sv_type](p, &c);
	return S_TRUE;
}

/* #API: |Push/add element (unsigned integer)|vector; data source|S_TRUE: added OK; S_FALSE: not enough memory|O(1)| */

sbool_t sv_push_u(sv_t **v, const suint_t c)
{
	SV_PUSH_START(1);
	SV_PUSH_END(1);
	svstx_f[(*v)->sv_type](p, (sint_t *)&c);
	return S_TRUE;
}

#undef SV_PUSH_START
#undef SV_PUSH_END
#undef SV_PUSH_IU

/*
 * Vector "pop": extract element from last position
 */

#define SV_POP_START(def_val)			\
	ASSERT_RETURN_IF(!v, def_val);		\
	const size_t sz = get_size(v);		\
	ASSERT_RETURN_IF(!sz, def_val);		\
	const size_t elem_size = v->elem_size;	\
	char *p = ptr_to_elem(v, sz - 1);

#define SV_POP_END			\
	set_size(v, sz - 1);

#define SV_POP_IU(T)					\
	sint_t tmp;					\
	return *(T *)(svldx_f[v->sv_type](p, &tmp, 0));

/* #API: |Pop/extract element (generic data)|vector|Element reference|O(i1)| */

void *sv_pop(sv_t *v)
{
	SV_POP_START(NULL);
	SV_POP_END;
	return p;
}

/* #API: |Pop/extract element (integer)|vector|Integer element|O(i1)| */

sint_t sv_pop_i(sv_t *v)
{
	SV_POP_START(SV_DEFAULT_SIGNED_VAL);
	SV_POP_END;
	SV_POP_IU(sint_t);
}

/* #API: |Pop/extract element (unsigned integer)|vector|Integer element|O(i1)| */

suint_t sv_pop_u(sv_t *v)
{
	SV_POP_START(SV_DEFAULT_UNSIGNED_VAL);
	SV_POP_END;
	SV_POP_IU(suint_t);
}

