local info = require "coutil.info"

print(string.format([[
System Information
  Physical memory: %12d bytes
  Free memory    : %12d bytes
  System uptime  : %12d seconds
  System load    : %12.4f (last 1 minute)
                   %12.4f (last 5 minutes)
                   %12.4f (last 10 minutes)
]], info.getsystem("pfu1lL")))

print(string.format([[
Process Usage
  Available virtual  memory   : %20d bytes
  Resident memory             : %20d bytes
  User CPU time               : %20.4f seconds
  System CPU time             : %20.4f seconds
  Maximum resident memory     : %20d bytes
  Integral shared memory      : %20d bytes
  Integral unshared data      : %20d bytes
  Integral unshared stack     : %20d bytes
  Page reclaims               : %20d
  Page faults                 : %20d
  Page swaps                  : %20d
  Block input operations      : %20d
  Block output operations     : %20d
  IPC messages sent           : %20d
  IPC messages received       : %20d
  Signals received            : %20d
  Voluntary context switches  : %20d
  Involuntary context switches: %20d
]], info.getusage("cmUSTMd=pPwio><sxX")))

print("CPU Statistics")
print("  |  # |  Speed   |   User   |   Nice   |  System  |   Idle   |   IRQ    | Name")
local rowfmt = "  | %2d | "..string.rep("%8d | ", 6).."%s"
local cpustat = info.getcpustat()
for i = 1, cpustat:count() do
	print(string.format(rowfmt, i, cpustat:speed(i),
	                               cpustat:usertime(i),
	                               cpustat:nicetime(i),
	                               cpustat:systemtime(i),
	                               cpustat:idletime(i),
	                               cpustat:irqtime(i),
	                               cpustat:model(i)))
end