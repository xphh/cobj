#ifndef _GITHUB_XPHH_COBJ_H_
#define _GITHUB_XPHH_COBJ_H_

#include <stddef.h>

#ifdef __cplusplus
#define C_API extern "C"
#else
#define C_API
#endif

/************************************************************************/
/* for boolean type                                                     */
/************************************************************************/
typedef int BOOL;

/************************************************************************/
/* we use CSTR for string type                                          */
/************************************************************************/
typedef struct
{
	char *cstr;
} const_string_t;

C_API const_string_t const_string_new(const char *str);
C_API void const_string_clear(const_string_t cs);

#define CSTR				const_string_t
#define CS(str)				const_string_new(str)
#define CS_CLEAR(cs)		const_string_clear(cs), (cs).cstr = NULL

/************************************************************************/
/* free buffer which is easy to use                                     */
/************************************************************************/
typedef struct
{
	char *ptr;
	size_t length;
	size_t size;
	int count;
} free_buffer_t;

C_API void free_buffer_init(free_buffer_t *buf);
C_API void free_buffer_append(free_buffer_t *buf, void *data, size_t len);
C_API void free_buffer_append_empty(free_buffer_t *buf, size_t len);
C_API void free_buffer_cleanup(free_buffer_t *buf);

/************************************************************************/
/* self-define List type with generic type supported                    */
/************************************************************************/
#define DECLARE_LIST(TYPE) \
struct { \
	TYPE *array; \
	size_t _buffer_length; \
	size_t _buffer_size; \
	int size; \
}

#define LIST_INIT(list)				free_buffer_init((free_buffer_t *)&list)
#define LIST_ADD(list, obj)			free_buffer_append((free_buffer_t *)&list, &obj, sizeof(obj))
#define LIST_ADD_EMPTY(list, s)		free_buffer_append_empty((free_buffer_t *)&list, sizeof(struct s))
#define LIST_CLEAR(list)			free_buffer_cleanup((free_buffer_t *)&list)

#define LIST_ADD_INT(list, s)		{ int i = s; free_buffer_append((free_buffer_t *)&list, &i, sizeof(i)); }
#define LIST_ADD_BOOL(list, s)		{ BOOL b = s; free_buffer_append((free_buffer_t *)&list, &b, sizeof(b)); }
#define LIST_ADD_STR(list, s)		{ CSTR cstr = CS(s); free_buffer_append((free_buffer_t *)&list, &cstr, sizeof(cstr)); }
#define LIST_ADD_OBJ(list, s)		LIST_ADD(list, s)

/************************************************************************/
/* metainfo                                                             */
/************************************************************************/
enum
{
	/* Note that now supported types as below */
	FIELD_TYPE_INT = 0,
	FIELD_TYPE_BOOL,
	FIELD_TYPE_CSTR,
	FIELD_TYPE_STRUCT = 100,
};

typedef struct
{
	int pos;
	int type;
	int islist;
	const char *name;
	void *metainfo;
} field_t;

typedef struct  
{
	int obj_size;
	DECLARE_LIST(field_t) fs;
} metainfo_t;

C_API metainfo_t *metainfo_create(int obj_size);
C_API void metainfo_destroy(metainfo_t *mi);
C_API void metainfo_add_member(metainfo_t *mi, int pos, int type, const char *name, int islist);
C_API metainfo_t *metainfo_add_child(metainfo_t *mi, int pos, int obj_size, const char *name, int islist);

#define METAINFO(s)							_metainfo_##s
#define REGISTER_METAINFO(s)				_register_metainfo_##s()
#define DEFINE_METAINFO(s)					metainfo_t *METAINFO(s); REGISTER_METAINFO(s)

#define METAINFO_CREATE(s)					METAINFO(s) = metainfo_create((int)sizeof(struct s))
#define METAINFO_DESTROY(s)					metainfo_destroy(METAINFO(s))

#define METAINFO_ADD_MEMBER(s, t, n)		metainfo_add_member(METAINFO(s), offsetof(struct s, n), t, #n, 0)
#define METAINFO_ADD_MEMBER_LIST(s, t, n)	metainfo_add_member(METAINFO(s), offsetof(struct s, n), t, #n, 1)
#define METAINFO_ADD_CHILD(s, t, n)			metainfo_add_child(METAINFO(s), offsetof(struct s, n), (int)sizeof(struct t), #n, 0)
#define METAINFO_ADD_CHILD_LIST(s, t, n)	metainfo_add_child(METAINFO(s), offsetof(struct s, n), (int)sizeof(struct t), #n, 1)

#define METAINFO_CHILD_BEGIN(s, t, n)		{ metainfo_t *METAINFO(t) = METAINFO_ADD_CHILD(s, t, n);
#define METAINFO_CHILD_LIST_BEGIN(s, t, n)	{ metainfo_t *METAINFO(t) = METAINFO_ADD_CHILD_LIST(s, t, n);
#define METAINFO_CHILD_END()				}

/************************************************************************/
/* C Object <===> Json String                                           */
/************************************************************************/
C_API void *cobject_new(metainfo_t *mi);
#define OBJECT_NEW(s)					(struct s*)cobject_new(METAINFO(s))

C_API void cobject_clear(void *obj, metainfo_t *mi);
#define OBJECT_CLEAR(ptr, s)			cobject_clear(ptr, METAINFO(s))

C_API void cobject_delete(void *obj, metainfo_t *mi);
#define OBJECT_DELETE(ptr, s)			cobject_delete(ptr, METAINFO(s))

C_API CSTR cobject_to_json(void *obj, metainfo_t *mi);
#define OBJECT_TO_JSON(ptr, s)			cobject_to_json(ptr, METAINFO(s))

C_API void *cobject_from_json(metainfo_t *mi, const char *str);
#define OBJECT_FROM_JSON(s, str)		cobject_from_json(METAINFO(s), str)


#endif
