/* Generated by re2c 0.16 */
#line 1 "ext/standard/var_unserializer.re"
/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2017 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Sascha Schumann <sascha@schumann.cx>                         |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "php.h"
#include "ext/standard/php_var.h"
#include "php_incomplete_class.h"
#include "zend_portability.h"

struct php_unserialize_data {
	void *first;
	void *last;
	void *first_dtor;
	void *last_dtor;
	HashTable *allowed_classes;
};

PHPAPI php_unserialize_data_t php_var_unserialize_init() {
	php_unserialize_data_t d;
	/* fprintf(stderr, "UNSERIALIZE_INIT    == lock: %u, level: %u\n", BG(serialize_lock), BG(unserialize).level); */
	if (BG(serialize_lock) || !BG(unserialize).level) {
		d = ecalloc(1, sizeof(struct php_unserialize_data));
		if (!BG(serialize_lock)) {
			BG(unserialize).data = d;
			BG(unserialize).level = 1;
		}
	} else {
		d = BG(unserialize).data;
		++BG(unserialize).level;
	}
	return d;
}

PHPAPI void php_var_unserialize_destroy(php_unserialize_data_t d) {
	/* fprintf(stderr, "UNSERIALIZE_DESTROY == lock: %u, level: %u\n", BG(serialize_lock), BG(unserialize).level); */
	if (BG(serialize_lock) || BG(unserialize).level == 1) {
		var_destroy(&d);
		efree(d);
	}
	if (!BG(serialize_lock) && !--BG(unserialize).level) {
		BG(unserialize).data = NULL;
	}
}

PHPAPI HashTable *php_var_unserialize_get_allowed_classes(php_unserialize_data_t d) {
	return d->allowed_classes;
}
PHPAPI void php_var_unserialize_set_allowed_classes(php_unserialize_data_t d, HashTable *classes) {
	d->allowed_classes = classes;
}


/* {{{ reference-handling for unserializer: var_* */
#define VAR_ENTRIES_MAX 1024
#define VAR_ENTRIES_DBG 0

/* VAR_FLAG used in var_dtor entries to signify an entry on which __wakeup should be called */
#define VAR_WAKEUP_FLAG 1

typedef struct {
	zval *data[VAR_ENTRIES_MAX];
	zend_long used_slots;
	void *next;
} var_entries;

typedef struct {
	zval data[VAR_ENTRIES_MAX];
	zend_long used_slots;
	void *next;
} var_dtor_entries;

static inline void var_push(php_unserialize_data_t *var_hashx, zval *rval)
{
	var_entries *var_hash = (*var_hashx)->last;
#if VAR_ENTRIES_DBG
	fprintf(stderr, "var_push(%ld): %d\n", var_hash?var_hash->used_slots:-1L, Z_TYPE_P(rval));
#endif

	if (!var_hash || var_hash->used_slots == VAR_ENTRIES_MAX) {
		var_hash = emalloc(sizeof(var_entries));
		var_hash->used_slots = 0;
		var_hash->next = 0;

		if (!(*var_hashx)->first) {
			(*var_hashx)->first = var_hash;
		} else {
			((var_entries *) (*var_hashx)->last)->next = var_hash;
		}

		(*var_hashx)->last = var_hash;
	}

	var_hash->data[var_hash->used_slots++] = rval;
}

PHPAPI void var_push_dtor(php_unserialize_data_t *var_hashx, zval *rval)
{
	zval *tmp_var = var_tmp_var(var_hashx);
    if (!tmp_var) {
        return;
    }
	ZVAL_COPY(tmp_var, rval);
}

PHPAPI zval *var_tmp_var(php_unserialize_data_t *var_hashx)
{
    var_dtor_entries *var_hash;

    if (!var_hashx || !*var_hashx) {
        return NULL;
    }

    var_hash = (*var_hashx)->last_dtor;
    if (!var_hash || var_hash->used_slots == VAR_ENTRIES_MAX) {
        var_hash = emalloc(sizeof(var_dtor_entries));
        var_hash->used_slots = 0;
        var_hash->next = 0;

        if (!(*var_hashx)->first_dtor) {
            (*var_hashx)->first_dtor = var_hash;
        } else {
            ((var_dtor_entries *) (*var_hashx)->last_dtor)->next = var_hash;
        }

        (*var_hashx)->last_dtor = var_hash;
    }
    ZVAL_UNDEF(&var_hash->data[var_hash->used_slots]);
	Z_EXTRA(var_hash->data[var_hash->used_slots]) = 0;
    return &var_hash->data[var_hash->used_slots++];
}

PHPAPI void var_replace(php_unserialize_data_t *var_hashx, zval *ozval, zval *nzval)
{
	zend_long i;
	var_entries *var_hash = (*var_hashx)->first;
#if VAR_ENTRIES_DBG
	fprintf(stderr, "var_replace(%ld): %d\n", var_hash?var_hash->used_slots:-1L, Z_TYPE_P(nzval));
#endif

	while (var_hash) {
		for (i = 0; i < var_hash->used_slots; i++) {
			if (var_hash->data[i] == ozval) {
				var_hash->data[i] = nzval;
				/* do not break here */
			}
		}
		var_hash = var_hash->next;
	}
}

static zval *var_access(php_unserialize_data_t *var_hashx, zend_long id)
{
	var_entries *var_hash = (*var_hashx)->first;
#if VAR_ENTRIES_DBG
	fprintf(stderr, "var_access(%ld): %ld\n", var_hash?var_hash->used_slots:-1L, id);
#endif

	while (id >= VAR_ENTRIES_MAX && var_hash && var_hash->used_slots == VAR_ENTRIES_MAX) {
		var_hash = var_hash->next;
		id -= VAR_ENTRIES_MAX;
	}

	if (!var_hash) return NULL;

	if (id < 0 || id >= var_hash->used_slots) return NULL;

	return var_hash->data[id];
}

PHPAPI void var_destroy(php_unserialize_data_t *var_hashx)
{
	void *next;
	zend_long i;
	var_entries *var_hash = (*var_hashx)->first;
	var_dtor_entries *var_dtor_hash = (*var_hashx)->first_dtor;
	zend_bool wakeup_failed = 0;
	zval wakeup_name;
	ZVAL_UNDEF(&wakeup_name);

#if VAR_ENTRIES_DBG
	fprintf(stderr, "var_destroy(%ld)\n", var_hash?var_hash->used_slots:-1L);
#endif

	while (var_hash) {
		next = var_hash->next;
		efree_size(var_hash, sizeof(var_entries));
		var_hash = next;
	}

	while (var_dtor_hash) {
		for (i = 0; i < var_dtor_hash->used_slots; i++) {
			zval *zv = &var_dtor_hash->data[i];
#if VAR_ENTRIES_DBG
			fprintf(stderr, "var_destroy dtor(%p, %ld)\n", var_dtor_hash->data[i], Z_REFCOUNT_P(var_dtor_hash->data[i]));
#endif

			/* Perform delayed __wakeup calls */
			if (Z_EXTRA_P(zv) == VAR_WAKEUP_FLAG) {
				if (!wakeup_failed) {
					zval retval;
					if (Z_ISUNDEF(wakeup_name)) {
						ZVAL_STRINGL(&wakeup_name, "__wakeup", sizeof("__wakeup") - 1);
					}

					BG(serialize_lock)++;
					if (call_user_function_ex(CG(function_table), zv, &wakeup_name, &retval, 0, 0, 1, NULL) == FAILURE || Z_ISUNDEF(retval)) {
						wakeup_failed = 1;
						GC_FLAGS(Z_OBJ_P(zv)) |= IS_OBJ_DESTRUCTOR_CALLED;
					}
					BG(serialize_lock)--;

					zval_ptr_dtor(&retval);
				} else {
					GC_FLAGS(Z_OBJ_P(zv)) |= IS_OBJ_DESTRUCTOR_CALLED;
				}
			}

			zval_ptr_dtor(zv);
		}
		next = var_dtor_hash->next;
		efree_size(var_dtor_hash, sizeof(var_dtor_entries));
		var_dtor_hash = next;
	}

	zval_ptr_dtor(&wakeup_name);
}

/* }}} */

static zend_string *unserialize_str(const unsigned char **p, size_t len, size_t maxlen)
{
	size_t i, j;
	zend_string *str = zend_string_safe_alloc(1, len, 0, 0);
	unsigned char *end = *(unsigned char **)p+maxlen;

	if (end < *p) {
		zend_string_free(str);
		return NULL;
	}

	for (i = 0; i < len; i++) {
		if (*p >= end) {
			zend_string_free(str);
			return NULL;
		}
		if (**p != '\\') {
			ZSTR_VAL(str)[i] = (char)**p;
		} else {
			unsigned char ch = 0;

			for (j = 0; j < 2; j++) {
				(*p)++;
				if (**p >= '0' && **p <= '9') {
					ch = (ch << 4) + (**p -'0');
				} else if (**p >= 'a' && **p <= 'f') {
					ch = (ch << 4) + (**p -'a'+10);
				} else if (**p >= 'A' && **p <= 'F') {
					ch = (ch << 4) + (**p -'A'+10);
				} else {
					zend_string_free(str);
					return NULL;
				}
			}
			ZSTR_VAL(str)[i] = (char)ch;
		}
		(*p)++;
	}
	ZSTR_VAL(str)[i] = 0;
	ZSTR_LEN(str) = i;
	return str;
}

static inline int unserialize_allowed_class(
		zend_string *class_name, php_unserialize_data_t *var_hashx)
{
	HashTable *classes = (*var_hashx)->allowed_classes;
	zend_string *lcname;
	int res;
	ALLOCA_FLAG(use_heap)

	if(classes == NULL) {
		return 1;
	}
	if(!zend_hash_num_elements(classes)) {
		return 0;
	}

	ZSTR_ALLOCA_ALLOC(lcname, ZSTR_LEN(class_name), use_heap);
	zend_str_tolower_copy(ZSTR_VAL(lcname), ZSTR_VAL(class_name), ZSTR_LEN(class_name));
	res = zend_hash_exists(classes, lcname);
	ZSTR_ALLOCA_FREE(lcname, use_heap);
	return res;
}

#define YYFILL(n) do { } while (0)
#define YYCTYPE unsigned char
#define YYCURSOR cursor
#define YYLIMIT limit
#define YYMARKER marker


#line 325 "ext/standard/var_unserializer.re"




static inline zend_long parse_iv2(const unsigned char *p, const unsigned char **q)
{
	char cursor;
	zend_long result = 0;
	int neg = 0;

	switch (*p) {
		case '-':
			neg++;
			/* fall-through */
		case '+':
			p++;
	}

	while (1) {
		cursor = (char)*p;
		if (cursor >= '0' && cursor <= '9') {
			result = result * 10 + (size_t)(cursor - (unsigned char)'0');
		} else {
			break;
		}
		p++;
	}
	if (q) *q = p;
	if (neg) return -result;
	return result;
}

static inline zend_long parse_iv(const unsigned char *p)
{
	return parse_iv2(p, NULL);
}

/* no need to check for length - re2c already did */
static inline size_t parse_uiv(const unsigned char *p)
{
	unsigned char cursor;
	size_t result = 0;

	if (*p == '+') {
		p++;
	}

	while (1) {
		cursor = *p;
		if (cursor >= '0' && cursor <= '9') {
			result = result * 10 + (size_t)(cursor - (unsigned char)'0');
		} else {
			break;
		}
		p++;
	}
	return result;
}

#define UNSERIALIZE_PARAMETER zval *rval, const unsigned char **p, const unsigned char *max, php_unserialize_data_t *var_hash
#define UNSERIALIZE_PASSTHRU rval, p, max, var_hash

static int php_var_unserialize_internal(UNSERIALIZE_PARAMETER);

static zend_always_inline int process_nested_data(UNSERIALIZE_PARAMETER, HashTable *ht, zend_long elements, int objprops)
{
	while (elements-- > 0) {
		zval key, *data, d, *old_data;
		zend_ulong idx;

		ZVAL_UNDEF(&key);

		if (!php_var_unserialize_internal(&key, p, max, NULL)) {
			zval_dtor(&key);
			return 0;
		}

		data = NULL;
		ZVAL_UNDEF(&d);

		if (!objprops) {
			if (Z_TYPE(key) == IS_LONG) {
				idx = Z_LVAL(key);
numeric_key:
				if (UNEXPECTED((old_data = zend_hash_index_find(ht, idx)) != NULL)) {
					//??? update hash
					var_push_dtor(var_hash, old_data);
					data = zend_hash_index_update(ht, idx, &d);
				} else {
					data = zend_hash_index_add_new(ht, idx, &d);
				}
			} else if (Z_TYPE(key) == IS_STRING) {
				if (UNEXPECTED(ZEND_HANDLE_NUMERIC(Z_STR(key), idx))) {
					goto numeric_key;
				}
				if (UNEXPECTED((old_data = zend_hash_find(ht, Z_STR(key))) != NULL)) {
					//??? update hash
					var_push_dtor(var_hash, old_data);
					data = zend_hash_update(ht, Z_STR(key), &d);
				} else {
					data = zend_hash_add_new(ht, Z_STR(key), &d);
				}
			} else {
				zval_dtor(&key);
				return 0;
			}
		} else {
			if (EXPECTED(Z_TYPE(key) == IS_STRING)) {
string_key:
				if ((old_data = zend_hash_find(ht, Z_STR(key))) != NULL) {
					if (Z_TYPE_P(old_data) == IS_INDIRECT) {
						old_data = Z_INDIRECT_P(old_data);
					}
					var_push_dtor(var_hash, old_data);
					data = zend_hash_update_ind(ht, Z_STR(key), &d);
				} else {
					data = zend_hash_add_new(ht, Z_STR(key), &d);
				}
			} else if (Z_TYPE(key) == IS_LONG) {
				/* object properties should include no integers */
				convert_to_string(&key);
				goto string_key;
			} else {
				zval_dtor(&key);
				return 0;
			}
		}

		if (!php_var_unserialize_internal(data, p, max, var_hash)) {
			zval_dtor(&key);
			return 0;
		}

		if (UNEXPECTED(Z_ISUNDEF_P(data))) {
			if (Z_TYPE(key) == IS_LONG) {
				zend_hash_index_del(ht, Z_LVAL(key));
			} else {
				zend_hash_del_ind(ht, Z_STR(key));
			}
		} else {
			var_push_dtor(var_hash, data);
		}

		zval_dtor(&key);

		if (elements && *(*p-1) != ';' && *(*p-1) != '}') {
			(*p)--;
			return 0;
		}
	}

	return 1;
}

static inline int finish_nested_data(UNSERIALIZE_PARAMETER)
{
	if (*((*p)++) == '}')
		return 1;

#if SOMETHING_NEW_MIGHT_LEAD_TO_CRASH_ENABLE_IF_YOU_ARE_BRAVE
	zval_ptr_dtor(rval);
#endif
	return 0;
}

static inline int object_custom(UNSERIALIZE_PARAMETER, zend_class_entry *ce)
{
	zend_long datalen;

	datalen = parse_iv2((*p) + 2, p);

	(*p) += 2;

	if (datalen < 0 || (max - (*p)) <= datalen) {
		zend_error(E_WARNING, "Insufficient data for unserializing - " ZEND_LONG_FMT " required, " ZEND_LONG_FMT " present", datalen, (zend_long)(max - (*p)));
		return 0;
	}

	if (ce->unserialize == NULL) {
		zend_error(E_WARNING, "Class %s has no unserializer", ZSTR_VAL(ce->name));
		object_init_ex(rval, ce);
	} else if (ce->unserialize(rval, ce, (const unsigned char*)*p, datalen, (zend_unserialize_data *)var_hash) != SUCCESS) {
		return 0;
	}

	(*p) += datalen;

	return finish_nested_data(UNSERIALIZE_PASSTHRU);
}

static inline zend_long object_common1(UNSERIALIZE_PARAMETER, zend_class_entry *ce)
{
	zend_long elements;

	if( *p >= max - 2) {
		zend_error(E_WARNING, "Bad unserialize data");
		return -1;
	}

	elements = parse_iv2((*p) + 2, p);

	(*p) += 2;

	if (ce->serialize == NULL) {
		object_init_ex(rval, ce);
	} else {
		/* If this class implements Serializable, it should not land here but in object_custom(). The passed string
		obviously doesn't descend from the regular serializer. */
		zend_error(E_WARNING, "Erroneous data format for unserializing '%s'", ZSTR_VAL(ce->name));
		return -1;
	}

	return elements;
}

#ifdef PHP_WIN32
# pragma optimize("", off)
#endif
static inline int object_common2(UNSERIALIZE_PARAMETER, zend_long elements)
{
	HashTable *ht;
	zend_bool has_wakeup;

	if (Z_TYPE_P(rval) != IS_OBJECT) {
		return 0;
	}

	has_wakeup = Z_OBJCE_P(rval) != PHP_IC_ENTRY
		&& zend_hash_str_exists(&Z_OBJCE_P(rval)->function_table, "__wakeup", sizeof("__wakeup")-1);

	ht = Z_OBJPROP_P(rval);
	zend_hash_extend(ht, zend_hash_num_elements(ht) + elements, (ht->u.flags & HASH_FLAG_PACKED));
	if (!process_nested_data(UNSERIALIZE_PASSTHRU, ht, elements, 1)) {
		if (has_wakeup) {
			ZVAL_DEREF(rval);
			GC_FLAGS(Z_OBJ_P(rval)) |= IS_OBJ_DESTRUCTOR_CALLED;
		}
		return 0;
	}

	ZVAL_DEREF(rval);
	if (has_wakeup) {
		/* Delay __wakeup call until end of serialization */
		zval *wakeup_var = var_tmp_var(var_hash);
		ZVAL_COPY(wakeup_var, rval);
		Z_EXTRA_P(wakeup_var) = VAR_WAKEUP_FLAG;
	}

	return finish_nested_data(UNSERIALIZE_PASSTHRU);
}
#ifdef PHP_WIN32
# pragma optimize("", on)
#endif

PHPAPI int php_var_unserialize(UNSERIALIZE_PARAMETER)
{
	var_entries *orig_var_entries = (*var_hash)->last;
	zend_long orig_used_slots = orig_var_entries ? orig_var_entries->used_slots : 0;
	int result;

	result = php_var_unserialize_internal(UNSERIALIZE_PASSTHRU);

	if (!result) {
		/* If the unserialization failed, mark all elements that have been added to var_hash
		 * as NULL. This will forbid their use by other unserialize() calls in the same
		 * unserialization context. */
		var_entries *e = orig_var_entries;
		zend_long s = orig_used_slots;
		while (e) {
			for (; s < e->used_slots; s++) {
				e->data[s] = NULL;
			}

			e = e->next;
			s = 0;
		}
	}

	return result;
}

static int php_var_unserialize_internal(UNSERIALIZE_PARAMETER)
{
	const unsigned char *cursor, *limit, *marker, *start;
	zval *rval_ref;

	limit = max;
	cursor = *p;

	if (YYCURSOR >= YYLIMIT) {
		return 0;
	}

	if (var_hash && (*p)[0] != 'R') {
		var_push(var_hash, rval);
	}

	start = cursor;


#line 622 "ext/standard/var_unserializer.c"
{
	YYCTYPE yych;
	static const unsigned char yybm[] = {
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		128, 128, 128, 128, 128, 128, 128, 128, 
		128, 128,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
		  0,   0,   0,   0,   0,   0,   0,   0, 
	};
	if ((YYLIMIT - YYCURSOR) < 7) YYFILL(7);
	yych = *YYCURSOR;
	switch (yych) {
	case 'C':
	case 'O':	goto yy4;
	case 'N':	goto yy5;
	case 'R':	goto yy6;
	case 'S':	goto yy7;
	case 'a':	goto yy8;
	case 'b':	goto yy9;
	case 'd':	goto yy10;
	case 'i':	goto yy11;
	case 'o':	goto yy12;
	case 'r':	goto yy13;
	case 's':	goto yy14;
	case '}':	goto yy15;
	default:	goto yy2;
	}
yy2:
	++YYCURSOR;
yy3:
#line 1005 "ext/standard/var_unserializer.re"
	{ return 0; }
#line 682 "ext/standard/var_unserializer.c"
yy4:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy17;
	goto yy3;
yy5:
	yych = *++YYCURSOR;
	if (yych == ';') goto yy19;
	goto yy3;
yy6:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy21;
	goto yy3;
yy7:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy22;
	goto yy3;
yy8:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy23;
	goto yy3;
yy9:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy24;
	goto yy3;
yy10:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy25;
	goto yy3;
yy11:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy26;
	goto yy3;
yy12:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy27;
	goto yy3;
yy13:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy28;
	goto yy3;
yy14:
	yych = *(YYMARKER = ++YYCURSOR);
	if (yych == ':') goto yy29;
	goto yy3;
yy15:
	++YYCURSOR;
#line 999 "ext/standard/var_unserializer.re"
	{
	/* this is the case where we have less data than planned */
	php_error_docref(NULL, E_NOTICE, "Unexpected end of serialized data");
	return 0; /* not sure if it should be 0 or 1 here? */
}
#line 735 "ext/standard/var_unserializer.c"
yy17:
	yych = *++YYCURSOR;
	if (yybm[0+yych] & 128) {
		goto yy31;
	}
	if (yych == '+') goto yy30;
yy18:
	YYCURSOR = YYMARKER;
	goto yy3;
yy19:
	++YYCURSOR;
#line 677 "ext/standard/var_unserializer.re"
	{
	*p = YYCURSOR;
	ZVAL_NULL(rval);
	return 1;
}
#line 753 "ext/standard/var_unserializer.c"
yy21:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy33;
		goto yy18;
	} else {
		if (yych <= '-') goto yy33;
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy34;
		goto yy18;
	}
yy22:
	yych = *++YYCURSOR;
	if (yych == '+') goto yy36;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy37;
	goto yy18;
yy23:
	yych = *++YYCURSOR;
	if (yych == '+') goto yy39;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy40;
	goto yy18;
yy24:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '1') goto yy42;
	goto yy18;
yy25:
	yych = *++YYCURSOR;
	if (yych <= '/') {
		if (yych <= ',') {
			if (yych == '+') goto yy43;
			goto yy18;
		} else {
			if (yych <= '-') goto yy44;
			if (yych <= '.') goto yy45;
			goto yy18;
		}
	} else {
		if (yych <= 'I') {
			if (yych <= '9') goto yy46;
			if (yych <= 'H') goto yy18;
			goto yy48;
		} else {
			if (yych == 'N') goto yy49;
			goto yy18;
		}
	}
yy26:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy50;
		goto yy18;
	} else {
		if (yych <= '-') goto yy50;
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy51;
		goto yy18;
	}
yy27:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy53;
		goto yy18;
	} else {
		if (yych <= '-') goto yy53;
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy54;
		goto yy18;
	}
yy28:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy56;
		goto yy18;
	} else {
		if (yych <= '-') goto yy56;
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy57;
		goto yy18;
	}
yy29:
	yych = *++YYCURSOR;
	if (yych == '+') goto yy59;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy60;
	goto yy18;
yy30:
	yych = *++YYCURSOR;
	if (yybm[0+yych] & 128) {
		goto yy31;
	}
	goto yy18;
yy31:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yybm[0+yych] & 128) {
		goto yy31;
	}
	if (yych <= '/') goto yy18;
	if (yych <= ':') goto yy62;
	goto yy18;
yy33:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy34:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy34;
	if (yych == ';') goto yy63;
	goto yy18;
yy36:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy37:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy37;
	if (yych <= ':') goto yy65;
	goto yy18;
yy39:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy40:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy40;
	if (yych <= ':') goto yy66;
	goto yy18;
yy42:
	yych = *++YYCURSOR;
	if (yych == ';') goto yy67;
	goto yy18;
yy43:
	yych = *++YYCURSOR;
	if (yych == '.') goto yy45;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy46;
	goto yy18;
yy44:
	yych = *++YYCURSOR;
	if (yych <= '/') {
		if (yych != '.') goto yy18;
	} else {
		if (yych <= '9') goto yy46;
		if (yych == 'I') goto yy48;
		goto yy18;
	}
yy45:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy69;
	goto yy18;
yy46:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	if (yych <= ':') {
		if (yych <= '.') {
			if (yych <= '-') goto yy18;
			goto yy69;
		} else {
			if (yych <= '/') goto yy18;
			if (yych <= '9') goto yy46;
			goto yy18;
		}
	} else {
		if (yych <= 'E') {
			if (yych <= ';') goto yy71;
			if (yych <= 'D') goto yy18;
			goto yy73;
		} else {
			if (yych == 'e') goto yy73;
			goto yy18;
		}
	}
yy48:
	yych = *++YYCURSOR;
	if (yych == 'N') goto yy74;
	goto yy18;
yy49:
	yych = *++YYCURSOR;
	if (yych == 'A') goto yy75;
	goto yy18;
yy50:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy51:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy51;
	if (yych == ';') goto yy76;
	goto yy18;
yy53:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy54:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy54;
	if (yych <= ':') goto yy78;
	goto yy18;
yy56:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy57:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy57;
	if (yych == ';') goto yy79;
	goto yy18;
yy59:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy60:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 2) YYFILL(2);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy60;
	if (yych <= ':') goto yy81;
	goto yy18;
yy62:
	yych = *++YYCURSOR;
	if (yych == '"') goto yy82;
	goto yy18;
yy63:
	++YYCURSOR;
#line 626 "ext/standard/var_unserializer.re"
	{
	zend_long id;

 	*p = YYCURSOR;
	if (!var_hash) return 0;

	id = parse_iv(start + 2) - 1;
	if (id == -1 || (rval_ref = var_access(var_hash, id)) == NULL) {
		return 0;
	}

	zval_ptr_dtor(rval);
	if (Z_ISUNDEF_P(rval_ref) || (Z_ISREF_P(rval_ref) && Z_ISUNDEF_P(Z_REFVAL_P(rval_ref)))) {
		ZVAL_UNDEF(rval);
		return 1;
	}
	if (Z_ISREF_P(rval_ref)) {
		ZVAL_COPY(rval, rval_ref);
	} else {
		ZVAL_NEW_REF(rval_ref, rval_ref);
		ZVAL_COPY(rval, rval_ref);
	}

	return 1;
}
#line 1029 "ext/standard/var_unserializer.c"
yy65:
	yych = *++YYCURSOR;
	if (yych == '"') goto yy84;
	goto yy18;
yy66:
	yych = *++YYCURSOR;
	if (yych == '{') goto yy86;
	goto yy18;
yy67:
	++YYCURSOR;
#line 683 "ext/standard/var_unserializer.re"
	{
	*p = YYCURSOR;
	ZVAL_BOOL(rval, parse_iv(start + 2));
	return 1;
}
#line 1046 "ext/standard/var_unserializer.c"
yy69:
	++YYCURSOR;
	if ((YYLIMIT - YYCURSOR) < 3) YYFILL(3);
	yych = *YYCURSOR;
	if (yych <= ';') {
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy69;
		if (yych <= ':') goto yy18;
	} else {
		if (yych <= 'E') {
			if (yych <= 'D') goto yy18;
			goto yy73;
		} else {
			if (yych == 'e') goto yy73;
			goto yy18;
		}
	}
yy71:
	++YYCURSOR;
#line 731 "ext/standard/var_unserializer.re"
	{
#if SIZEOF_ZEND_LONG == 4
use_double:
#endif
	*p = YYCURSOR;
	ZVAL_DOUBLE(rval, zend_strtod((const char *)start + 2, NULL));
	return 1;
}
#line 1075 "ext/standard/var_unserializer.c"
yy73:
	yych = *++YYCURSOR;
	if (yych <= ',') {
		if (yych == '+') goto yy88;
		goto yy18;
	} else {
		if (yych <= '-') goto yy88;
		if (yych <= '/') goto yy18;
		if (yych <= '9') goto yy89;
		goto yy18;
	}
yy74:
	yych = *++YYCURSOR;
	if (yych == 'F') goto yy91;
	goto yy18;
yy75:
	yych = *++YYCURSOR;
	if (yych == 'N') goto yy91;
	goto yy18;
yy76:
	++YYCURSOR;
#line 689 "ext/standard/var_unserializer.re"
	{
#if SIZEOF_ZEND_LONG == 4
	int digits = YYCURSOR - start - 3;

	if (start[2] == '-' || start[2] == '+') {
		digits--;
	}

	/* Use double for large zend_long values that were serialized on a 64-bit system */
	if (digits >= MAX_LENGTH_OF_LONG - 1) {
		if (digits == MAX_LENGTH_OF_LONG - 1) {
			int cmp = strncmp((char*)YYCURSOR - MAX_LENGTH_OF_LONG, long_min_digits, MAX_LENGTH_OF_LONG - 1);

			if (!(cmp < 0 || (cmp == 0 && start[2] == '-'))) {
				goto use_double;
			}
		} else {
			goto use_double;
		}
	}
#endif
	*p = YYCURSOR;
	ZVAL_LONG(rval, parse_iv(start + 2));
	return 1;
}
#line 1123 "ext/standard/var_unserializer.c"
yy78:
	yych = *++YYCURSOR;
	if (yych == '"') goto yy92;
	goto yy18;
yy79:
	++YYCURSOR;
#line 652 "ext/standard/var_unserializer.re"
	{
	zend_long id;

 	*p = YYCURSOR;
	if (!var_hash) return 0;

	id = parse_iv(start + 2) - 1;
	if (id == -1 || (rval_ref = var_access(var_hash, id)) == NULL) {
		return 0;
	}

	if (rval_ref == rval) {
		return 0;
	}

	if (Z_ISUNDEF_P(rval_ref) || (Z_ISREF_P(rval_ref) && Z_ISUNDEF_P(Z_REFVAL_P(rval_ref)))) {
		ZVAL_UNDEF(rval);
		return 1;
	}

	ZVAL_COPY(rval, rval_ref);

	return 1;
}
#line 1155 "ext/standard/var_unserializer.c"
yy81:
	yych = *++YYCURSOR;
	if (yych == '"') goto yy94;
	goto yy18;
yy82:
	++YYCURSOR;
#line 847 "ext/standard/var_unserializer.re"
	{
	size_t len, len2, len3, maxlen;
	zend_long elements;
	char *str;
	zend_string *class_name;
	zend_class_entry *ce;
	int incomplete_class = 0;

	int custom_object = 0;

	zval user_func;
	zval retval;
	zval args[1];

    if (!var_hash) return 0;
	if (*start == 'C') {
		custom_object = 1;
	}

	len2 = len = parse_uiv(start + 2);
	maxlen = max - YYCURSOR;
	if (maxlen < len || len == 0) {
		*p = start + 2;
		return 0;
	}

	str = (char*)YYCURSOR;

	YYCURSOR += len;

	if (*(YYCURSOR) != '"') {
		*p = YYCURSOR;
		return 0;
	}
	if (*(YYCURSOR+1) != ':') {
		*p = YYCURSOR+1;
		return 0;
	}

	len3 = strspn(str, "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\177\200\201\202\203\204\205\206\207\210\211\212\213\214\215\216\217\220\221\222\223\224\225\226\227\230\231\232\233\234\235\236\237\240\241\242\243\244\245\246\247\250\251\252\253\254\255\256\257\260\261\262\263\264\265\266\267\270\271\272\273\274\275\276\277\300\301\302\303\304\305\306\307\310\311\312\313\314\315\316\317\320\321\322\323\324\325\326\327\330\331\332\333\334\335\336\337\340\341\342\343\344\345\346\347\350\351\352\353\354\355\356\357\360\361\362\363\364\365\366\367\370\371\372\373\374\375\376\377\\");
	if (len3 != len)
	{
		*p = YYCURSOR + len3 - len;
		return 0;
	}

	class_name = zend_string_init(str, len, 0);

	do {
		if(!unserialize_allowed_class(class_name, var_hash)) {
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
			break;
		}

		/* Try to find class directly */
		BG(serialize_lock)++;
		ce = zend_lookup_class(class_name);
		if (ce) {
			BG(serialize_lock)--;
			if (EG(exception)) {
				zend_string_release(class_name);
				return 0;
			}
			break;
		}
		BG(serialize_lock)--;

		if (EG(exception)) {
			zend_string_release(class_name);
			return 0;
		}

		/* Check for unserialize callback */
		if ((PG(unserialize_callback_func) == NULL) || (PG(unserialize_callback_func)[0] == '\0')) {
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
			break;
		}

		/* Call unserialize callback */
		ZVAL_STRING(&user_func, PG(unserialize_callback_func));

		ZVAL_STR_COPY(&args[0], class_name);
		BG(serialize_lock)++;
		if (call_user_function_ex(CG(function_table), NULL, &user_func, &retval, 1, args, 0, NULL) != SUCCESS) {
			BG(serialize_lock)--;
			if (EG(exception)) {
				zend_string_release(class_name);
				zval_ptr_dtor(&user_func);
				zval_ptr_dtor(&args[0]);
				return 0;
			}
			php_error_docref(NULL, E_WARNING, "defined (%s) but not found", Z_STRVAL(user_func));
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
			zval_ptr_dtor(&user_func);
			zval_ptr_dtor(&args[0]);
			break;
		}
		BG(serialize_lock)--;
		zval_ptr_dtor(&retval);
		if (EG(exception)) {
			zend_string_release(class_name);
			zval_ptr_dtor(&user_func);
			zval_ptr_dtor(&args[0]);
			return 0;
		}

		/* The callback function may have defined the class */
		BG(serialize_lock)++;
		if ((ce = zend_lookup_class(class_name)) == NULL) {
			php_error_docref(NULL, E_WARNING, "Function %s() hasn't defined the class it was called for", Z_STRVAL(user_func));
			incomplete_class = 1;
			ce = PHP_IC_ENTRY;
		}
		BG(serialize_lock)--;

		zval_ptr_dtor(&user_func);
		zval_ptr_dtor(&args[0]);
		break;
	} while (1);

	*p = YYCURSOR;

	if (custom_object) {
		int ret;

		ret = object_custom(UNSERIALIZE_PASSTHRU, ce);

		if (ret && incomplete_class) {
			php_store_class_name(rval, ZSTR_VAL(class_name), len2);
		}
		zend_string_release(class_name);
		return ret;
	}

	elements = object_common1(UNSERIALIZE_PASSTHRU, ce);

	if (elements < 0) {
	   zend_string_release(class_name);
	   return 0;
	}

	if (incomplete_class) {
		php_store_class_name(rval, ZSTR_VAL(class_name), len2);
	}
	zend_string_release(class_name);

	return object_common2(UNSERIALIZE_PASSTHRU, elements);
}
#line 1314 "ext/standard/var_unserializer.c"
yy84:
	++YYCURSOR;
#line 772 "ext/standard/var_unserializer.re"
	{
	size_t len, maxlen;
	zend_string *str;

	len = parse_uiv(start + 2);
	maxlen = max - YYCURSOR;
	if (maxlen < len) {
		*p = start + 2;
		return 0;
	}

	if ((str = unserialize_str(&YYCURSOR, len, maxlen)) == NULL) {
		return 0;
	}

	if (*(YYCURSOR) != '"') {
		zend_string_free(str);
		*p = YYCURSOR;
		return 0;
	}

	if (*(YYCURSOR + 1) != ';') {
		efree(str);
		*p = YYCURSOR + 1;
		return 0;
	}

	YYCURSOR += 2;
	*p = YYCURSOR;

	ZVAL_STR(rval, str);
	return 1;
}
#line 1351 "ext/standard/var_unserializer.c"
yy86:
	++YYCURSOR;
#line 806 "ext/standard/var_unserializer.re"
	{
	zend_long elements = parse_iv(start + 2);
	/* use iv() not uiv() in order to check data range */
	*p = YYCURSOR;
    if (!var_hash) return 0;

	if (elements < 0) {
		return 0;
	}

	array_init_size(rval, elements);
	if (elements) {
		/* we can't convert from packed to hash during unserialization, because
		   reference to some zvals might be keept in var_hash (to support references) */
		zend_hash_real_init(Z_ARRVAL_P(rval), 0);
	}

	/* The array may contain references to itself, in which case we'll be modifying an
	 * rc>1 array. This is okay, since the array is, ostensibly, only visible to
	 * unserialize (in practice unserialization handlers also see it). Ideally we should
	 * prohibit "r:" references to non-objects, as we only generate them for objects. */
	HT_ALLOW_COW_VIOLATION(Z_ARRVAL_P(rval));

	if (!process_nested_data(UNSERIALIZE_PASSTHRU, Z_ARRVAL_P(rval), elements, 0)) {
		return 0;
	}

	return finish_nested_data(UNSERIALIZE_PASSTHRU);
}
#line 1384 "ext/standard/var_unserializer.c"
yy88:
	yych = *++YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych >= ':') goto yy18;
yy89:
	++YYCURSOR;
	if (YYLIMIT <= YYCURSOR) YYFILL(1);
	yych = *YYCURSOR;
	if (yych <= '/') goto yy18;
	if (yych <= '9') goto yy89;
	if (yych == ';') goto yy71;
	goto yy18;
yy91:
	yych = *++YYCURSOR;
	if (yych == ';') goto yy96;
	goto yy18;
yy92:
	++YYCURSOR;
#line 836 "ext/standard/var_unserializer.re"
	{
	long elements;
    if (!var_hash) return 0;

	elements = object_common1(UNSERIALIZE_PASSTHRU, ZEND_STANDARD_CLASS_DEF_PTR);
	if (elements < 0) {
		return 0;
	}
	return object_common2(UNSERIALIZE_PASSTHRU, elements);
}
#line 1414 "ext/standard/var_unserializer.c"
yy94:
	++YYCURSOR;
#line 740 "ext/standard/var_unserializer.re"
	{
	size_t len, maxlen;
	char *str;

	len = parse_uiv(start + 2);
	maxlen = max - YYCURSOR;
	if (maxlen < len) {
		*p = start + 2;
		return 0;
	}

	str = (char*)YYCURSOR;

	YYCURSOR += len;

	if (*(YYCURSOR) != '"') {
		*p = YYCURSOR;
		return 0;
	}

	if (*(YYCURSOR + 1) != ';') {
		*p = YYCURSOR + 1;
		return 0;
	}

	YYCURSOR += 2;
	*p = YYCURSOR;

	ZVAL_STRINGL(rval, str, len);
	return 1;
}
#line 1449 "ext/standard/var_unserializer.c"
yy96:
	++YYCURSOR;
#line 715 "ext/standard/var_unserializer.re"
	{
	*p = YYCURSOR;

	if (!strncmp((char*)start + 2, "NAN", 3)) {
		ZVAL_DOUBLE(rval, ZEND_NAN);
	} else if (!strncmp((char*)start + 2, "INF", 3)) {
		ZVAL_DOUBLE(rval, ZEND_INFINITY);
	} else if (!strncmp((char*)start + 2, "-INF", 4)) {
		ZVAL_DOUBLE(rval, -ZEND_INFINITY);
	} else {
		ZVAL_NULL(rval);
	}

	return 1;
}
#line 1468 "ext/standard/var_unserializer.c"
}
#line 1007 "ext/standard/var_unserializer.re"


	return 0;
}
