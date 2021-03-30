local system = require "coutil.system"

print(string.format([[
System
  Kernel         : %s
  Version        : %s
  Release        : %s
  Platform       : %s
  Network name   : %s
  Temporary dir. : %s
  Physical memory: %12d bytes
  Free memory    : %12d bytes
  Uptime         : %12.4f seconds
  Load           : %12.4f (last 1 minute)
                   %12.4f (last 5 minutes)
                   %12.4f (last 10 minutes)

User
  Login: %s (UID=%d, GID=%d)
  Home: %s
  Shell: %s

Process
  Executable path             : %s
  Process identifier          : %20d
  Parent process identifier   : %20d
  Available virtual memory    : %20d bytes
  Resident memory             : %20d bytes
  Maximum resident memory     : %20d bytes
  Integral shared memory      : %20d bytes
  Integral unshared data      : %20d bytes
  Integral unshared stack     : %20d bytes
  User CPU time               : %20.4f seconds
  System CPU time             : %20.4f seconds
  Page reclaims               : %20d
  Page faults                 : %20d
  Swaps                       : %20d
  Block input operations      : %20d
  Block output operations     : %20d
  IPC messages sent           : %20d
  IPC messages received       : %20d
  Signals received            : %20d
  Voluntary context switches  : %20d
  Involuntary context switches: %20d
]], system.info("kVvhnTbft1lLUugH$e#^MrRmd=cspPwio><SxX")))

print("CPU")
print("  # |    Speed     |    User     |    Nice     |   System    |    Idle     |     IRQ     | Name")
local rowfmt = " %2d | %8d MHz | "..string.rep("%8d ms | ", 5).."%s"
local cpuinfo = system.cpuinfo()
for i = 1, cpuinfo:count() do
	print(string.format(rowfmt, i, cpuinfo:stats(i, "cunsidm")))
end
print()

print("Network interfaces")
print("  # |     Name     |        MAC        | Internal | Domain | Address")
local rowfmt = " %2d | %-12s | %17s |   %-3s    |  %-4s  | %s/%d"
local netifaces = system.netifaces()
for i = 1, netifaces:count() do
  local address, mask = netifaces:getaddress(i);
  print(string.format(rowfmt, i, netifaces:getname(i),
                                 netifaces:getmac(i),
                                 netifaces:isinternal(i) and "yes" or "",
                                 netifaces:getdomain(i),
                                 address.literal,
                                 mask))
end
print()
