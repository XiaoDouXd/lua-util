
print(">> hello world!")

print(">> call Ext.Str.FuncC(0.72, \"ext str param2\", false, \"const char* param4\"):")
local data = Ext.Str.FuncC(0.72, "ext str param2", false, "const char* param4")
print(">> Ext.Str.FuncC return: " .. (data or 'nil'))

print(">> call Ext.FuncA(\"str aa\", 0.618):")
local ret = Ext.FuncA("str aa", 0.618)
print(">> Ext.FuncA return: "..(ret or "nil"))

print(">> call Ext.FuncB(3.18, 2897.1):")
local ret = Ext.FuncB(3.18, 2897.1)
print(">> call Ext.FuncB return:" .. (ret or "nil"))

function LuaFunc(doubeVal, strVal)
  print("LuaFunc called: doubleVal = "..doubeVal..", strVal="..strVal)
end
