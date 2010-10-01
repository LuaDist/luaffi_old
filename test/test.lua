#!/usr/bin/lua5.1

package.cpath = ";;../?.so"
require "luaffi"


assert(ffi.Tint == ffi.Tsint)


--libc = "/lib/libc.so.6"
--libc = "/lib/tls/i686/cmov/libc.so.6"
-- lua VM uses the libc, so we can just use the main program lib
libcpath = nil


if 1 then
   cif = ffi.prep_cif(ffi.DEFAULT_ABI, ffi.Tint, ffi.Tpointer, ffi.Tint)
   local libc = ffi.open_lib(libcpath)
   print("libc", libc)
   printf = ffi.get_symbol(libc, "printf")
   print("printf", printf)
   
   ffi.call(cif, printf, "hello %d\n", 42)
end

local function tracecall(f, name)
   return function(...)
      print("calling", name, "with args")
      for i, v in ipairs({...}) do
	 print(">>>", i, v)
      end
      
      local res = { f(...) }
      
      for i, v in ipairs(res) do
	 print("<<<", i, v)
      end
      
      return unpack(res)
   end
end

-- libs cache
local libs = { }
setmetatable(libs, { __mode = "v" }) -- this way, libs are closed (collected) after a while

-- used to keep libraries alive as long as we have some function pointer referencing them
local libref = { }
setmetatable(libref, { __mode = "k" })

function makefun(libname, funcname, rtype, ...)
   local cif = ffi.prep_cif(ffi.DEFAULT_ABI, rtype, ...)
   if not cif then return end

   local lib = libs[libname or "default"]
   if not lib then
      lib = ffi.open_lib(libname)
      if not lib then 
	 print("couldn't open lib", libname)
	 return 
      end
      libs[libname or "default"] = lib
      print("OPENED LIB", libname, lib)
   end

   local pfunc = ffi.get_symbol(lib, funcname)
   if not pfunc then 
      print("couln't find function", funcname, "in lib", libname)
      return 
   end

   local res
   if rtype == ffi.Tvoid then
      res = function(...)
	       ffi.call(cif, pfunc, ...)
	    end
   else
      res = function(...)
	       return ffi.call(cif, pfunc, ...)
	    end
   end

   --res = tracecall(res, funcname)

   libref[res] = lib

   return res
end

testlib = "./test.so"

makefun(testlib, "testme", ffi.Tshort, ffi.Tpointer)
collectgarbage "collect"
collectgarbage "collect"

testme = makefun(testlib, "testme", ffi.Tshort, ffi.Tpointer)
print("testme returns", testme("hello world"))

testdouble = makefun(testlib, "doubletest", ffi.Tvoid, ffi.Tdouble)
collectgarbage "collect"
collectgarbage "collect"
testfloat = makefun(testlib, "floattest", ffi.Tvoid, ffi.Tfloat)
testchar = makefun(testlib, "chartest", ffi.Tvoid, ffi.Tchar)

testdouble(332.42)
testfloat(332.42)
testchar(42)

-- struct
Ttest = ffi.struct_new(ffi.Tint, ffi.Tint)
print("Ttest", Ttest)
teststruct = makefun(testlib, "structtest", Ttest, ffi.Tsint, ffi.Tuint)
test2struct = makefun(testlib, "structtest2", ffi.Tvoid, Ttest)
test3struct = makefun(testlib, "structtest3", ffi.Tvoid, ffi.Tpointer)

struct = teststruct(42, 332)
print(struct)
test2struct(struct)
test3struct(struct)


-- string example with the libc
malloc =     makefun(libc, "malloc",   ffi.Tpointer, ffi.Tulong)
free =       makefun(libc, "free",     ffi.Tvoid, ffi.Tpointer)
strlen =     makefun(libc, "strlen",   ffi.Tint, ffi.Tpointer)
snprintf_i = makefun(libc, "snprintf", ffi.Tint, ffi.Tpointer, ffi.Tint, ffi.Tpointer, ffi.Tint)

str = malloc(1024)
print(snprintf_i(str, 1024, "hello %d", 42))
print(ffi.tostring(str))
print(strlen(str))
print(strlen("hello"))

free(str)



-- closure
local function func()
   print("closure called")
end

closure = ffi.closure_new(ffi.prep_cif(ffi.DEFAULT_ABI, ffi.Tvoid), func)
ffi.call(closure.cif, closure.func)


-- closure with arguments
local function func(a, b)
   return a + b
end

closure = ffi.closure_new(ffi.prep_cif(ffi.DEFAULT_ABI, ffi.Tdouble, ffi.Tdouble, ffi.Tint), func)
print(ffi.call(closure.cif, closure.func, 3.2, 4.3))


-- closure with struct argument
local function func(a)
   print("closure received struct", a)
   test3struct(a)
   return a
end

closure = ffi.closure_new(ffi.prep_cif(ffi.DEFAULT_ABI, Ttest, Ttest), func)
struct2 = ffi.call(closure.cif, closure.func, struct)
print("struct2", struct2)
test3struct(struct)
test3struct(struct2)
