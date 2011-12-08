#if !defined(X3_CORE_PLUGINIMPL_H) && !defined(_LIB)
#define X3_CORE_PLUGINIMPL_H

#include "classentry.h"
#include "moduleitem.h"
#ifdef X3_CORE_PORTABILITY_H
#include "../portability/portimpl.h"
#endif

BEGIN_NAMESPACE_X3

static HMODULE  s_hmod = NULL;
static HMODULE  s_manager = NULL;
static bool     s_loadmgr = false;
static long     s_refcount = 0;

OUTAPI bool x3InitializePlugin();
OUTAPI void x3UninitializePlugin();

HMODULE getModuleHandle()
{
    return s_hmod;
}

HMODULE getManagerModule()
{
    return s_manager;
}

static int getClassCount(int minType = 0)
{
    int count = 0;

    for (const ClassEntry* const* arr = ClassEntry::classes; *arr; arr++)
    {
        for (const ClassEntry* cls = *arr; cls->creator; cls++)
        {
            if (cls->type >= minType)
                count++;
        }
    }

    return count;
}

static const char** getClassIDs(const char** clsids, int count)
{
    const ClassEntry* const* arr = ClassEntry::classes;
    int i = 0;

    for (; *arr && i < count; arr++)
    {
        for (const ClassEntry* cls = *arr; cls->creator; cls++, i++)
        {
            clsids[i++] = cls->clsid;
        }
    }
    clsids[i] = NULL;

    return clsids;
}

static bool getDefaultClassID(long iid, const char*& clsid)
{
    for (const ClassEntry* const* arr = ClassEntry::classes; *arr; arr++)
    {
        for (const ClassEntry* cls = *arr; cls->creator; cls++)
        {
            if (cls->hasiid(iid))
            {
                clsid = cls->clsid;
                return true;
            }
        }
    }

    return false;
}

OUTAPI bool x3InternalCreate(const char* clsid, long iid, IObject** p)
{
    *p = NULL;

    if (0 == *clsid)
    {
        getDefaultClassID(iid, clsid);
    }
    for (const ClassEntry* const* arr = ClassEntry::classes; *arr; arr++)
    {
        for (const ClassEntry* cls = *arr; cls->creator; cls++)
        {
            if (strcmp(cls->clsid, clsid) == 0)
            {
                *p = cls->creator(iid);
                return *p != NULL;
            }
        }
    }

    return false;
}

OUTAPI void x3FreePlugin()
{
    if (--s_refcount != 0)
        return;

    x3UninitializePlugin();

    if (s_manager && s_manager != s_hmod)
    {
        typedef bool (*CF)(const char*, long, IObject**);
        typedef void (*UF)(CF);
        UF f = (UF)GetProcAddress(s_manager, "x3UnregisterPlugin");
        if (f) f(x3InternalCreate);
    }
    if (s_loadmgr)
    {
        x3FreeLibrary(s_manager);
        s_loadmgr = false;
    }

    ModuleItem::free();
}

#ifndef X3_CLASS_MAXCOUNT
#define X3_CLASS_MAXCOUNT 64
#endif

OUTAPI bool x3InitPlugin(HMODULE hmod, HMODULE hmanager)
{
    if (++s_refcount != 1)
        return true;

    int singletonClassCount = getClassCount(MIN_SINGLETON_TYPE);
    ModuleItem::init(singletonClassCount);

    // Call all interfaces' getIID() to set static const value.
    const char* tmpclsid;
    getDefaultClassID(IObject::getIID(), tmpclsid);

    s_hmod = hmod;

    if (!s_manager)
    {
        hmanager = hmanager ? hmanager : GetModuleHandleA("x3manager.pln");
        if (!hmanager)
        {
            hmanager = x3LoadLibrary("x3manager.pln");
            s_loadmgr = true;
        }
        s_manager = hmanager;
    }

    bool needInit = true;

    if (s_manager && s_manager != s_hmod)
    {
        typedef bool (*CF)(const char*, long, IObject**);
        typedef bool (*RF)(CF, HMODULE, const char**);
        RF freg = (RF)GetProcAddress(s_manager, "x3RegisterPlugin");

        const char* clsids[X3_CLASS_MAXCOUNT + 1] = { NULL };
        needInit = !freg || freg(x3InternalCreate, s_hmod, 
            getClassIDs(clsids, X3_CLASS_MAXCOUNT));
    }

    return !needInit || x3InitializePlugin();
}

bool createObject(const char* clsid, long iid, IObject** p)
{
    if (!x3InternalCreate(clsid, iid, p) && *clsid && s_manager)
    {
        typedef bool (*F)(const char*, long, IObject**);
        F f = (F)GetProcAddress(s_manager, "x3CreateObject");
        return f && f(clsid, iid, p);
    }

    return *p != NULL;
}

#ifndef X3_EXCLUDE_CREATEOBJECT
OUTAPI bool x3CreateObject(const char* clsid, long iid, IObject** p)
{
    return createObject(clsid, iid, p);
}
#endif

END_NAMESPACE_X3
#endif