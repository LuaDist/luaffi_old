/*
 * luaffi.c - libffi for lua
 *
 * Copyright (C) 2008 Vincent Penne (ziggy at sahipa dot com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 */

#include <lua.h>
#include <lauxlib.h>

#if !defined(LUA_VERSION_NUM) || (LUA_VERSION_NUM < 501)
#include "compat-5.1.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ffi.h>

#define REG(name) { #name, lua_##name }

typedef struct {
    const char *name;
    void *ptr;
} udatareg_t;

typedef struct {
    const char *name;
    int value;
} intreg_t;

typedef struct {
    const char *name;
    int (* func)(lua_State *);
} funcreg_t;

typedef struct {
    const char *name;
    const char *string;
} stringreg_t;

static udatareg_t abis[] = {
    { "FIRST_ABI", (void *) FFI_FIRST_ABI },

  /* ---- Intel x86 Win32 ---------- */
#ifdef X86_WIN32
    { "SYSV", (void *) FFI_SYSV },
    { "STDCALL", (void *) FFI_STDCALL },
#endif
    
    /* ---- Intel x86 and AMD x86-64 - */
#if !defined(X86_WIN32) && (defined(__i386__) || defined(__x86_64__))
    { "SYSV", (void *) FFI_SYSV },
    { "UNIX64", (void *) FFI_UNIX64 },
#endif

    { "DEFAULT_ABI", (void *) FFI_DEFAULT_ABI },

    { "LAST_ABI", (void *) FFI_LAST_ABI },
    NULL
};


static udatareg_t types[] = {
    { "Tchar", &ffi_type_schar },
    { "Tschar", &ffi_type_schar },
    { "Tuchar", &ffi_type_uchar },
    { "Tshort", &ffi_type_sshort },
    { "Tsshort", &ffi_type_sshort },
    { "Tushort", &ffi_type_ushort },
    { "Tint", &ffi_type_sint },
    { "Tsint", &ffi_type_sint },
    { "Tuint", &ffi_type_uint },
    { "Tlong", &ffi_type_slong },
    { "Tslong", &ffi_type_slong },
    { "Tulong", &ffi_type_ulong },
    { "Tvoid", &ffi_type_void },
    { "Tuint8", &ffi_type_uint8 },
    { "Tsint8", &ffi_type_sint8 },
    { "Tuint16", &ffi_type_uint16 },
    { "Tsint16", &ffi_type_sint16 },
    { "Tuint32", &ffi_type_uint32 },
    { "Tsint32", &ffi_type_sint32 },
    { "Tuint64", &ffi_type_uint64 },
    { "Tsint64", &ffi_type_sint64 },
    { "Tfloat", &ffi_type_float },
    { "Tdouble", &ffi_type_double },
    { "Tpointer", &ffi_type_pointer },
    { "Tlongdouble", &ffi_type_longdouble },
    NULL
};

static int lua_prep_cif(lua_State *L)
{
    ffi_cif *cif;
    int nargs = lua_gettop(L) - 2;
    ffi_type **types;
    int i;

    if (nargs < 0)
        return 0;
    
    cif = lua_newuserdata(L, sizeof(ffi_cif) + sizeof(ffi_type *) * nargs);

    luaL_getmetatable(L, "ffi_cif");
    lua_setmetatable(L, -2);

    types = (ffi_type **) (cif + 1);

    for (i = 0; i < nargs; i++)
        types[i] = (ffi_type *) luaL_checkudata(L, i + 3, "ffi_type");

    if (ffi_prep_cif(cif, (ffi_abi) lua_touserdata(L, 1), nargs, (ffi_type *) luaL_checkudata(L, 2, "ffi_type"), types) != FFI_OK)
        return 0;

    return 1;
}

static int lua_struct_new(lua_State *L)
{
    ffi_type *type;
    int nargs = lua_gettop(L);
    ffi_type **types;
    int i;

    if (nargs < 0)
        return 0;
    
    type = lua_newuserdata(L, sizeof(ffi_type) + sizeof(ffi_type *) * (nargs + 1));

    luaL_getmetatable(L, "ffi_type");
    lua_setmetatable(L, -2);

    types = (ffi_type **) (type + 1);

    for (i = 0; i < nargs; i++)
        types[i] = (ffi_type *) luaL_checkudata(L, i + 1, "ffi_type");
    types[i] = NULL;

    type->type = FFI_TYPE_STRUCT;
    type->alignment = type->size = 0;
    type->elements = types;

    return 1;
}

static int lua_ffi_call(lua_State *L)
{
    ffi_cif *cif = lua_touserdata(L, 1);
    void *f = lua_touserdata(L, 2);
    int i, nargs = lua_gettop(L) - 2, j;
    void **pargs, *arg;
    void *rval;
    uint8_t *ptr;
    int sp;

    /* commented out for efficiency purpose, anyway, we're supposed to know what we do here */
    /*luaL_checkudata(L, 1, "ffi_cif");*/
        
    if (nargs > cif->nargs)
        nargs = cif->nargs;

    pargs = alloca(sizeof(void *) * nargs);

    for (i = 0, j = 3; i < nargs; i++, j++) {
#       define putarg(type, val) ((pargs[i] = arg = alloca(sizeof(type))), *(type *)arg = (val))
        switch (cif->arg_types[i]->type) {
            case FFI_TYPE_INT:
                putarg(int, lua_tonumber(L, j));
                break;
                
            case FFI_TYPE_SINT8:
                putarg(int8_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_SINT16:
                putarg(int16_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_SINT32:
                putarg(int32_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_SINT64:
                putarg(int64_t, lua_tonumber(L, j));
                break;
                
            case FFI_TYPE_UINT8:
                putarg(uint8_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_UINT16:
                putarg(uint16_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_UINT32:
                putarg(uint32_t, lua_tonumber(L, j));
                break;
            case FFI_TYPE_UINT64:
                putarg(uint64_t, lua_tonumber(L, j));
                break;
                
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
            case FFI_TYPE_LONGDOUBLE:
                putarg(long double, lua_tonumber(L, j));
                break;
#endif
                
            case FFI_TYPE_DOUBLE:
                putarg(double, lua_tonumber(L, j));
                break;

            case FFI_TYPE_FLOAT:
                putarg(float, lua_tonumber(L, j));
                break;

            case FFI_TYPE_STRUCT:
                pargs[i] = lua_touserdata(L, j);
                break;
                
            case FFI_TYPE_POINTER:
                if (lua_isstring(L, j))
                    putarg(const char *, lua_tostring(L, j));
                else
                    putarg(void *, lua_touserdata(L, j));
                break;
        }
#       undef putarg
    }

    if (cif->rtype->type == FFI_TYPE_STRUCT) {
        rval = lua_newuserdata(L, cif->rtype->size);
        sp = lua_gettop(L);
    } else
        rval = alloca(cif->rtype->size);

    ffi_call(cif, f, rval, pargs);

    switch (cif->rtype->type) {
        case FFI_TYPE_INT:
            lua_pushnumber(L, *(int *) rval);
            return 1;
            
        case FFI_TYPE_SINT8:
            lua_pushnumber(L, *(int8_t *) rval);
            return 1;
        case FFI_TYPE_SINT16:
            lua_pushnumber(L, *(int16_t *) rval);
            return 1;
        case FFI_TYPE_SINT32:
            lua_pushnumber(L, *(int32_t *) rval);
            return 1;
        case FFI_TYPE_SINT64:
            lua_pushnumber(L, *(int64_t *) rval);
            return 1;
            
        case FFI_TYPE_UINT8:
            lua_pushnumber(L, *(uint8_t *) rval);
            return 1;
        case FFI_TYPE_UINT16:
            lua_pushnumber(L, *(uint16_t *) rval);
            return 1;
        case FFI_TYPE_UINT32:
            lua_pushnumber(L, *(uint32_t *) rval);
            return 1;
        case FFI_TYPE_UINT64:
            lua_pushnumber(L, *(uint64_t *) rval);
            return 1;
            
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
        case FFI_TYPE_LONGDOUBLE:
            lua_pushnumber(L, *(long double *) rval);
            return 1;
#endif
        case FFI_TYPE_DOUBLE:
            lua_pushnumber(L, *(double *) rval);
            return 1;
        case FFI_TYPE_FLOAT:
            lua_pushnumber(L, *(float *) rval);
            return 1;

        case FFI_TYPE_STRUCT:
            /* the result is already in the stack */
            lua_settop(L, sp);
            return 1;
            
        case FFI_TYPE_POINTER:
            lua_pushlightuserdata(L, *(void * *) rval);
            return 1;
    }

    return 0;
}

static stringreg_t cif_metastrings[] = {
    { "type", "ffi_cif" },
    NULL
};

static stringreg_t type_metastrings[] = {
    { "type", "ffi_type" },
    NULL
};


/* closures */

typedef struct {
    lua_State *L;
    ffi_closure *writable;
    void *f;
} closure_t;

static void lua_ffi_closure(ffi_cif *cif, void *resp, void **args,
                            void *userdata)
{
    closure_t *c = (closure_t *) userdata;
    lua_State *L = c->L;
    int i;
    int sp = lua_gettop(L);

    luaL_getmetatable(L, "ffi_closure");
    lua_pushlightuserdata(L, c);
    lua_rawget(L, -2);

    for (i = 0; i < cif->nargs; i++) {
        void *arg = args[i];
        
        switch (cif->arg_types[i]->type) {
        case FFI_TYPE_INT:
            lua_pushnumber(L, *(int *) arg);
            break;
            
        case FFI_TYPE_SINT8:
            lua_pushnumber(L, *(int8_t *) arg);
            break;
        case FFI_TYPE_SINT16:
            lua_pushnumber(L, *(int16_t *) arg);
            break;
        case FFI_TYPE_SINT32:
            lua_pushnumber(L, *(int32_t *) arg);
            break;
        case FFI_TYPE_SINT64:
            lua_pushnumber(L, *(int64_t *) arg);
            break;
            
        case FFI_TYPE_UINT8:
            lua_pushnumber(L, *(uint8_t *) arg);
            break;
        case FFI_TYPE_UINT16:
            lua_pushnumber(L, *(uint16_t *) arg);
            break;
        case FFI_TYPE_UINT32:
            lua_pushnumber(L, *(uint32_t *) arg);
            break;
        case FFI_TYPE_UINT64:
            lua_pushnumber(L, *(uint64_t *) arg);
            break;
            
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
        case FFI_TYPE_LONGDOUBLE:
            lua_pushnumber(L, *(long double *) arg);
            break;
#endif
        case FFI_TYPE_DOUBLE:
            lua_pushnumber(L, *(double *) arg);
            break;
        case FFI_TYPE_FLOAT:
            lua_pushnumber(L, *(float *) arg);
            break;

        case FFI_TYPE_STRUCT:
            lua_pushlightuserdata(L, arg);
            break;
            
        case FFI_TYPE_POINTER:
            lua_pushlightuserdata(L, *(void * *) arg);
            break;
        }
    }
    
    lua_call(L, cif->nargs, cif->rtype->type == FFI_TYPE_VOID? 0 : 1);

#   define putres(type, val) (*(type *)resp = (val))
    switch (cif->rtype->type) {
        case FFI_TYPE_INT:
            putres(int, lua_tonumber(L, -1));
            break;
            
        case FFI_TYPE_SINT8:
            putres(int8_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_SINT16:
            putres(int16_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_SINT32:
            putres(int32_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_SINT64:
            putres(int64_t, lua_tonumber(L, -1));
            break;
            
        case FFI_TYPE_UINT8:
            putres(uint8_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_UINT16:
            putres(uint16_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_UINT32:
            putres(uint32_t, lua_tonumber(L, -1));
            break;
        case FFI_TYPE_UINT64:
            putres(uint64_t, lua_tonumber(L, -1));
            break;
            
#if FFI_TYPE_LONGDOUBLE != FFI_TYPE_DOUBLE
        case FFI_TYPE_LONGDOUBLE:
            putres(long double, lua_tonumber(L, -1));
            break;
#endif
            
        case FFI_TYPE_DOUBLE:
            putres(double, lua_tonumber(L, -1));
            break;
            
        case FFI_TYPE_FLOAT:
            putres(float, lua_tonumber(L, -1));
            break;
            
        case FFI_TYPE_STRUCT:
            memcpy(resp, lua_touserdata(L, -1), cif->rtype->size);
            break;
            
        case FFI_TYPE_POINTER:
            if (lua_isstring(L, -1))
                putres(const char *, lua_tostring(L, -1));
            else
                putres(void *, lua_touserdata(L, -1));
            break;
    }
#   undef putres

    /* restore stack balance */
    lua_settop(L, sp);
}

static int lua_closure_new(lua_State *L)
{
    closure_t *c;

    c = lua_newuserdata(L, sizeof(closure_t));
    if (!c)
        return 0;

    c->L = L;
    c->writable = ffi_closure_alloc(sizeof(ffi_closure), &c->f);

    if (!c->writable)
        return 0;

    luaL_getmetatable(L, "ffi_closure");

    lua_pushlightuserdata(L, c);
    lua_pushvalue(L, 2);
    lua_rawset(L, -3);
    
    /* make sure the cif isn't gc'ed during the closure's lifetime */
    lua_pushlightuserdata(L, c->writable);
    lua_pushvalue(L, 1);
    lua_rawset(L, -3);
    
    lua_setmetatable(L, -2);
    
    if (ffi_prep_closure(c->writable, (ffi_cif *) luaL_checkudata(L, 1, "ffi_cif"),
                         lua_ffi_closure, c) != FFI_OK)
        return 0;

    return 1;
}

static int lua_closure_gc(lua_State *L)
{
    closure_t *c = lua_touserdata(L, 1);

    ffi_closure_free(c->writable);

    lua_getmetatable(L, 1);
    lua_pushlightuserdata(L, c);
    lua_pushnil(L);
    lua_rawset(L, -3);

    if (c->writable) {
        /* unreference the cif */
        lua_pushlightuserdata(L, c->writable);
        lua_pushnil(L);
        lua_rawset(L, -3);
    }
    
    return 0;
}

static int lua_closure_index(lua_State *L)
{
    closure_t *c = lua_touserdata(L, 1);
    const char *index = lua_tostring(L, 2);

    if (!strcmp(index, "func")) {
        /* hmmm, under x86 linux, if I try to call c->f, it crashes, so
         * I simply return c->writable, is it be a problem under other architectures ? */
        lua_pushlightuserdata(L, c->writable);
        //lua_pushlightuserdata(L, c->f);
        return 1;
    } else if (!strcmp(index, "cif")) {
        lua_getmetatable(L, 1);
        lua_pushlightuserdata(L, c->writable);
        lua_rawget(L, -2);
        return 1;
    } else if (!strcmp(index, "writable")) {
        lua_pushlightuserdata(L, c->writable);
        return 1;
    }

    return 0;
}


static stringreg_t closure_metastrings[] = {
    { "type", "ffi_closure" },
    NULL
};
static funcreg_t closure_metafuncs[] = {
    { "__gc", lua_closure_gc },
    { "__index", lua_closure_index },
    NULL
};


/* misc. function */
/* pointer arithmetic + some fundamental types read/write functions */

static int lua_ffi_tostring(lua_State *L)
{
    lua_pushstring(L, lua_touserdata(L, 1));
    return 1;
}

static void *ptradd(lua_State *L, int i)
{
    int n = lua_gettop(L);
    uint8_t *res = lua_touserdata(L, i++);

    for ( ; i < n; i++)
        if (lua_isnumber(L, i))
            res += (int) lua_tonumber(L, i);
        else
            res += (int) lua_touserdata(L, i);

    return res;
}

static void *ptrsub(lua_State *L, int i)
{
    int n = lua_gettop(L);
    uint8_t *res = lua_touserdata(L, i++);

    for ( ; i < n; i++)
        if (lua_isnumber(L, i))
            res -= (int) lua_tonumber(L, i);
        else
            res -= (int) lua_touserdata(L, i);

    return res;
}

static int lua_ptradd(lua_State *L)
{
    lua_pushlightuserdata(L, ptradd(L, 1));
    return 1;
}

static int lua_ptrsub(lua_State *L)
{
    lua_pushlightuserdata(L, ptrsub(L, 1));
    return 1;
}

#define RWTYPE(type) \
    static int lua_w##type(lua_State *L) { *(type *)ptradd(L, 2) = lua_tonumber(L, 1); return 0; } \
    static int lua_r##type(lua_State *L) { lua_pushnumber(L, *(type *)ptradd(L, 2)); return 1; }
#define RWTYPE2(type) \
    static int lua_w##type(lua_State *L) { *(type##_t *)ptradd(L, 2) = lua_tonumber(L, 1); return 0; } \
    static int lua_r##type(lua_State *L) { lua_pushnumber(L, *(type##_t *)ptradd(L, 2)); return 1; }

RWTYPE(int)
RWTYPE(uint)
RWTYPE2(int8)
RWTYPE2(uint8)
RWTYPE2(int16)
RWTYPE2(uint16)
RWTYPE2(int32)
RWTYPE2(uint32)
RWTYPE2(int64)
RWTYPE2(uint64)
RWTYPE(double)
RWTYPE(float)

static int lua_wptr(lua_State *L) { *(void **)ptradd(L, 2) = lua_touserdata(L, 1); return 0; }
static int lua_rptr(lua_State *L) { lua_pushlightuserdata(L, *(void **)ptradd(L, 2)); return 1; }


/* dynamic library handling (dlfnc under unix only for now) */
/* TODO put this in a separate lib */

#include <dlfcn.h>
static int lua_open_lib(lua_State *L)
{
    void *h, **ph;

    /* TODO make the mode configurable */
    h = dlopen(lua_tostring(L, 1), RTLD_LAZY);
    if (!h)
        return 0;

    ph = lua_newuserdata(L, sizeof(void *));
    *ph = h;
    luaL_getmetatable(L, "ffi_lib");
    lua_setmetatable(L, -2);

    return 1;
}

static int lua_lib_gc(lua_State *L)
{
    void *h = *(void * *) lua_touserdata(L, 1);

    printf("closing lib %p\n", h);
    dlclose(h);

    return 0;
}

static int lua_get_symbol(lua_State *L)
{
    void *h;

    h = *(void * *) luaL_checkudata(L, 1, "ffi_lib");

    h = dlsym(h, lua_tostring(L, 2));
    if (!h)
        return 0;

    lua_pushlightuserdata(L, h);

    return 1;
}

static funcreg_t lib_metafuncs[] = {
    { "__gc", lua_lib_gc },
    NULL
};
static stringreg_t lib_metastrings[] = {
    { "type", "ffi_lib" },
    NULL
};


/* building the ffi module */

static luaL_reg func[] = {
    REG(prep_cif),
    REG(struct_new),
    { "call", lua_ffi_call },
    REG(closure_new),
    REG(open_lib),
    REG(get_symbol),
    { "tostring", lua_ffi_tostring },
    REG(rint), REG(wint),
    REG(rint8), REG(wint8),
    REG(rint16), REG(wint16),
    REG(rint32), REG(wint32),
    REG(rint64), REG(wint64),
    REG(ruint8), REG(wuint8),
    REG(ruint16), REG(wuint16),
    REG(ruint32), REG(wuint32),
    REG(ruint64), REG(wuint64),
    REG(rfloat), REG(wfloat),
    REG(rdouble), REG(wdouble),
    REG(rptr), REG(wptr),
    NULL
};

static void register_lightuserdata(lua_State *L, udatareg_t *reg, int index)
{
    int i;

    for (i = 0; reg[i].name; i++) {
        lua_pushstring(L, reg[i].name);
        lua_pushlightuserdata(L, reg[i].ptr);
        lua_rawset(L, index-2);
    }
}

static void register_funcs(lua_State *L, funcreg_t *reg, int index)
{
    int i;

    for (i = 0; reg[i].name; i++) {
        lua_pushstring(L, reg[i].name);
        lua_pushcfunction(L, reg[i].func);
        lua_rawset(L, index-2);
    }
}

static void register_strings(lua_State *L, stringreg_t *reg, int index)
{
    int i;

    for (i = 0; reg[i].name; i++) {
        lua_pushstring(L, reg[i].name);
        lua_pushstring(L, reg[i].string);
        lua_rawset(L, index-2);
    }
}

int luaopen_luaffi(lua_State *L)
{
    int i;

    if (!luaL_newmetatable(L, "ffi_cif"))
        goto error;
    register_strings(L, cif_metastrings, -1);
    lua_settop(L, -2); /* pop */
    
    if (!luaL_newmetatable(L, "ffi_type"))
        goto error;
    register_strings(L, type_metastrings, -1);
    lua_settop(L, -2); /* pop */
    
    if (!luaL_newmetatable(L, "ffi_closure"))
        goto error;
    register_funcs(L, closure_metafuncs, -1);
    register_strings(L, closure_metastrings, -1);
    lua_settop(L, -2); /* pop */
    
    if (!luaL_newmetatable(L, "ffi_lib"))
        goto error;
    register_funcs(L, lib_metafuncs, -1);
    register_strings(L, lib_metastrings, -1);
    lua_settop(L, -2); /* pop */
    
    luaL_openlib(L, "ffi", func, 0);

    /* duplicate the ffi base types into lua userdata */
    for (i = 0; types[i].name; i++) {
        ffi_type *type;
        int j;
        
        lua_pushstring(L, types[i].name);
        for (j = 0; types[j].name; j++)
            if (j < i && types[j].ptr == types[i].ptr) {
                /* this type exists already, let's use it */
                lua_pushstring(L, types[j].name);
                lua_rawget(L, -3);
                goto exists;
            }
        /* new type, let's duplicate it into a userdata */
        type = lua_newuserdata(L, sizeof(ffi_type));
        memcpy(type, types[i].ptr, sizeof(ffi_type));
exists:
        luaL_getmetatable(L, "ffi_type");
        lua_setmetatable(L, -2);
        lua_rawset(L, -3);
    }

    /* ABI's are light user data (and not lua numbers) to avoid double to
     * int conversion later (void * to int is much faster) */
    register_lightuserdata(L, abis, -1);
    
    return 1;

error:
    return 0;
}

