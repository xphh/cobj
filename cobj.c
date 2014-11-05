/*
Copyright (c) 2014 xphh

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#define _CRT_SECURE_NO_DEPRECATE
#include "cobj.h"
#include "cJSON/cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <string.h>

#ifdef WIN32
#define strdup _strdup
#endif

/************************************************************************/
/* const string                                                         */
/************************************************************************/
C_API const_string_t const_string_new(const char *str)
{
	const_string_t cs;
	cs.cstr = strdup(str);
	return cs;
}

C_API void const_string_clear(const_string_t cs)
{
	if (cs.cstr)
	{
		free(cs.cstr);
	}
}

/************************************************************************/
/* free buffer                                                          */
/************************************************************************/
#define BLOCK_SIZE	1024
#define CEIL(n)		(((n) + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE)

C_API void free_buffer_init(free_buffer_t *buf)
{
	memset(buf, 0, sizeof(free_buffer_t));
}

C_API void free_buffer_append(free_buffer_t *buf, void *data, size_t len)
{
	size_t need_len = buf->length + len + 1;
	if (need_len > buf->size)
	{
		size_t newlen = buf->length + len;
		size_t newsize = CEIL(need_len);

		char *newptr = (char *)malloc(newsize);
		memcpy(newptr, buf->ptr, buf->length);
		memcpy(newptr + buf->length, data, len);

		free(buf->ptr);
		buf->ptr = newptr;
		buf->length = newlen;
		buf->size = newsize;
	}
	else
	{
		memcpy(buf->ptr + buf->length, data, len);
		buf->length += len;
	}

	buf->ptr[buf->length] = 0;

	buf->count++;
}

C_API void free_buffer_append_empty(free_buffer_t *buf, size_t len)
{
	void *data = malloc(len);
	memset(data, 0, len);
	free_buffer_append(buf, data, len);
	free(data);
}

C_API void free_buffer_cleanup(free_buffer_t *buf)
{
	if (buf->ptr)
	{
		free(buf->ptr);
	}
	buf->ptr = NULL;
	buf->length = 0;
	buf->size = 0;
	buf->count = 0;
}

/************************************************************************/
/* metainfo                                                             */
/************************************************************************/
C_API metainfo_t *metainfo_create(int obj_size)
{
	metainfo_t *mi = (metainfo_t *)malloc(sizeof(metainfo_t));
	memset(mi, 0, sizeof(metainfo_t));
	mi->obj_size = obj_size;
	return mi;
}

C_API void metainfo_destroy(metainfo_t *mi)
{
	LIST_CLEAR(mi->fs);
	free(mi);
}

C_API void metainfo_add_member(metainfo_t *mi, int pos, int type, const char *name, int islist)
{
	field_t f;
	f.pos = pos;
	f.type = type;
	f.islist = islist;
	f.name = name;
	f.metainfo = NULL;
	LIST_ADD_OBJ(mi->fs, f);
}

C_API metainfo_t *metainfo_add_child(metainfo_t *mi, int pos, int obj_size, const char *name, int islist)
{
	metainfo_t *child = metainfo_create(obj_size);
	field_t f;
	f.pos = pos;
	f.type = FIELD_TYPE_STRUCT;
	f.islist = islist;
	f.name = name;
	f.metainfo = child;
	LIST_ADD_OBJ(mi->fs, f);
	return child;
}

/************************************************************************/
/* C Object                                                             */
/************************************************************************/
C_API void *cobject_new(metainfo_t *mi)
{
	void *ptr = malloc(mi->obj_size);
	memset(ptr, 0, mi->obj_size);
	return ptr;
}

C_API void cobject_init(void *obj, metainfo_t *mi)
{
	memset(obj, 0, mi->obj_size);
}

C_API void cobject_clear(void *obj, metainfo_t *mi)
{
	int i;

	for (i = 0; i < mi->fs.size; i++)
	{
		field_t *f = &mi->fs.array[i];
		void *ptr = ((char *)obj + f->pos);

		if (f->islist)
		{
			free_buffer_t *fb = (free_buffer_t *)ptr;
			int k;

			if (f->type == FIELD_TYPE_CSTR)
			{
				for (k = 0; k < fb->count; k++)
				{
					CSTR *p = (CSTR *)(fb->ptr + k * sizeof(CSTR));
					CS_CLEAR(*p);
				}
			} 
			else if (f->type == FIELD_TYPE_STRUCT)
			{
				for (k = 0; k < fb->count; k++)
				{
					metainfo_t *mi = (metainfo_t *)f->metainfo;
					void *obj = fb->ptr + k * mi->obj_size;
					cobject_clear(obj, mi);
				}
			}

			LIST_CLEAR(*fb);
		}
		else
		{
			if (f->type == FIELD_TYPE_INT)
			{
				int *p = (int *)ptr;
				*p = 0;
			}
			else if (f->type == FIELD_TYPE_BOOL)
			{
				BOOL *p = (BOOL *)ptr;
				*p = 0;
			}
			else if (f->type == FIELD_TYPE_CSTR)
			{
				CSTR *p = (CSTR *)ptr;
				CS_CLEAR(*p);
			} 
			else if (f->type == FIELD_TYPE_STRUCT)
			{
				metainfo_t *mi = (metainfo_t *)f->metainfo;
				void *obj = ptr;
				cobject_clear(obj, mi);
			}
		}
	}
}

C_API void cobject_delete(void *obj, metainfo_t *mi)
{
	cobject_clear(obj, mi);
	free(obj);
}

static cJSON *_to_jsonobject(void *obj, metainfo_t *mi)
{
	cJSON *root = cJSON_CreateObject();	
	int i;

	for (i = 0; i < mi->fs.size; i++)
	{
		field_t *f = &mi->fs.array[i];
		void *ptr = ((char *)obj + f->pos);

		if (f->islist)
		{
			free_buffer_t *fb = (free_buffer_t *)ptr;
			cJSON *array = NULL;

			if (f->type == FIELD_TYPE_INT)
			{
				array = cJSON_CreateIntArray((int *)fb->ptr, fb->count);
			}
			else if (f->type == FIELD_TYPE_CSTR)
			{
				array = cJSON_CreateStringArray((const char **)fb->ptr, fb->count);
			} 
			else if (f->type == FIELD_TYPE_STRUCT)
			{
				int k;
				array = cJSON_CreateArray();
				for (k = 0; k < fb->count; k++)
				{
					metainfo_t *mi = (metainfo_t *)f->metainfo;
					void *obj = fb->ptr + k * mi->obj_size;
					cJSON *item = _to_jsonobject(obj, mi);
					cJSON_AddItemToArray(array, item);
				}
			}

			if (array)
			{
				cJSON_AddItemToObject(root, f->name, array);
			}
		}
		else
		{
			if (f->type == FIELD_TYPE_INT)
			{
				int *p = (int *)ptr;
				cJSON_AddNumberToObject(root, f->name, *p);
			}
			else if (f->type == FIELD_TYPE_BOOL)
			{
				BOOL *p = (BOOL *)ptr;
				if (*p)
				{
					cJSON_AddTrueToObject(root, f->name);
				}
				else
				{
					cJSON_AddFalseToObject(root, f->name);
				}
			}
			else if (f->type == FIELD_TYPE_CSTR)
			{
				CSTR *p = (CSTR *)ptr;
				if (p->cstr)
				{
					cJSON_AddStringToObject(root, f->name, p->cstr);
				}
				else
				{
					cJSON_AddStringToObject(root, f->name, "");
				}
			} 
			else if (f->type == FIELD_TYPE_STRUCT)
			{
				cJSON * child = _to_jsonobject(ptr, f->metainfo);
				cJSON_AddItemToObject(root, f->name, child);
			}
		}
	}

	return root;
}

C_API CSTR cobject_to_json(void *obj, metainfo_t *mi)
{
	const_string_t cs = {NULL};
	cJSON *json;

	if (!obj || !mi)
	{
		return cs;
	}

	json = _to_jsonobject(obj, mi);

	cs.cstr = cJSON_Print(json);

	cJSON_Delete(json);

	return cs;
}

static void _from_jsonobject(void *obj, metainfo_t *mi, cJSON *json)
{
	int i;
	for (i = 0; i < mi->fs.size; i++)
	{
		field_t *f = &mi->fs.array[i];
		void *ptr = ((char *)obj + f->pos);

		cJSON *item = cJSON_GetObjectItem(json, f->name);
		if (!item)
		{
			continue;
		}

		if (f->islist)
		{
			free_buffer_t *fb = (free_buffer_t *)ptr;

			if (item->type == cJSON_Array)
			{
				cJSON *array = item;
				int size = cJSON_GetArraySize(array);
				int k;
				for (k = 0; k < size; k++)
				{
					cJSON *item = cJSON_GetArrayItem(array, k);
					if (f->type == FIELD_TYPE_INT)
					{
						if (item->type == cJSON_Number)
						{
							LIST_ADD_INT(*fb, item->valueint);
						}
					}
					else if (f->type == FIELD_TYPE_CSTR)
					{
						if (item->type == cJSON_String)
						{
							LIST_ADD_STR(*fb, item->valuestring);
						}
					} 
					else if (f->type == FIELD_TYPE_STRUCT)
					{
						if (item->type == cJSON_Object)
						{
							metainfo_t *mi = (metainfo_t *)f->metainfo;
							void *obj = cobject_new(mi);
							_from_jsonobject(obj, mi, item);
							free_buffer_append(fb, obj, mi->obj_size);
							free(obj);
						}
					}
				}
			}
		}
		else
		{
			if (f->type == FIELD_TYPE_INT)
			{
				int *p = (int *)ptr;
				if (item->type == cJSON_Number)
				{
					*p = item->valueint;
				}
			}
			else if (f->type == FIELD_TYPE_BOOL)
			{
				BOOL *p = (BOOL *)ptr;
				if (item->type == cJSON_True)
				{
					*p = 1;
				}
				else if (item->type == cJSON_False)
				{
					*p = 0;
				}
			}
			else if (f->type == FIELD_TYPE_CSTR)
			{
				CSTR *p = (CSTR *)ptr;
				if (item->type == cJSON_String)
				{
					*p = CS(item->valuestring);
				}
			} 
			else if (f->type == FIELD_TYPE_STRUCT)
			{
				if (item->type == cJSON_Object)
				{
					metainfo_t *mi = (metainfo_t *)f->metainfo;
					void *obj = ptr;
					_from_jsonobject(obj, mi, item);
				}
			}
		}
	}
}

C_API void *cobject_from_json(metainfo_t *mi, const char *str)
{
	void *obj = cobject_new(mi);
	cJSON *json;
	if (!mi || !str)
	{
		return NULL;
	}

	json = cJSON_Parse(str);
	if (json == NULL)
	{
		return NULL;
	}

	_from_jsonobject(obj, mi, json);

	cJSON_Delete(json);

	return obj;
}
