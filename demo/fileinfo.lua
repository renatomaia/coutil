local system = require "coutil.system"
local filebits = system.filebits

local id,
      device,
      user,
      group,
      filedev,
      created,
      modified,
      stchanged,
      accessed,
      generation,
      type,
      mode,
      flags,
      bytes,
      blocks,
      tranfsz,
      aliases,
      fs_type,
      fs_tnum,
      fs_freen,
      fs_totaln,
      fs_avail,
      fs_freeb,
      fs_totalb,
      fs_tranfsz,
      path,
      linked = system.fileinfo(..., "~l#Dugdcmsav?M_Bbi*@NftAFTIp=")

local details
if type == "link" then
  details = " ("..linked..")"
elseif type == "character" or type == "block" then
  details = " device (ID="..filedev..")"
else
  details = ""
end

local function todate(epoch)
  if epoch == 0 then
    return "<not available>"
  end
  return os.date("%Y-%m-%d %H:%M:%S", math.floor(epoch))
end

print(string.format([[
Real Path           : %s
Type                : %s%s
Mode                : %s%s%s%s%s%s%s%s%s%s%s%s%s (%06o)
Owner               : UID=%d, GID=%d
Device              : %d
ID                  : %d
Creation date       : %s
Last modification   : %s
Last status change  : %s
Last access         : %s
Generation          : %d
User Flags          : 0x%x
Hard Links          : %d
Size                : %d blocks (%d bytes)
Ideal Transfer Block: %d bytes (file)
                    : %d bytes (filesystem)
File System         : %s (0x%x)
  Total             : %d blocks (%d inodes)
  Free              : %d blocks (%d inodes)
  Available         : %d blocks
]],
  path,
  type, details,
  (type == "regular" and "-") or (type == "unknown" and "?") or type:sub(1, 1),
  mode&filebits.setuid > 0 and "u" or "-",
  mode&filebits.setgid > 0 and "g" or "-",
  mode&filebits.sticky > 0 and "s" or "-",
  mode&filebits.ruser > 0 and "r" or "-",
  mode&filebits.wuser > 0 and "w" or "-",
  mode&filebits.xuser > 0 and "x" or "-",
  mode&filebits.rgroup > 0 and "r" or "-",
  mode&filebits.wgroup > 0 and "w" or "-",
  mode&filebits.xgroup > 0 and "x" or "-",
  mode&filebits.rother > 0 and "r" or "-",
  mode&filebits.wother > 0 and "w" or "-",
  mode&filebits.xother > 0 and "x" or "-",
  mode,
  user, group,
  device,
  id,
  todate(created),
  todate(modified),
  todate(stchanged),
  todate(accessed),
  generation,
  flags,
  aliases,
  blocks, bytes,
  tranfsz, fs_tranfsz,
  fs_type, fs_tnum,
  fs_totalb, fs_totaln,
  fs_freeb, fs_freen,
  fs_avail
))
