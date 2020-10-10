local runtime = {
	luarocks = {
		lua = os.getenv("LUA53_BIN"),
		cfgpattern = "local %1 =",
		gettime = [[
			(function ()
				package.cpath = os.getenv("LUAROCKS_CPATH").."/?.so;;"
				package.path = os.getenv("LUAROCKS_LPATH").."/?.lua;;"

				return require("luv").hrtime
			end)()
		]],
	},
	coutil = {
		lua = os.getenv("LUA54_BIN"),
		cfgpattern = "local %1<const> =",
		gettime = [[
			(function ()
				package.cpath = os.getenv("COUTIL_CPATH").."/?.so;"..
				                os.getenv("LUAMEM_CPATH").."/?.so;;"
				package.path = os.getenv("COUTIL_LPATH").."/?.lua;;"

				return require("coutil.system").nanosecs
			end)()
		]],
	},
}

local function readdemo(path)
	local file = assert(io.open("../demo/"..path))
	return assert(file:read("a")), file:close()
end

local function descibetests(tests)
	local list = {}
	for name, cases in pairs(tests) do
		local feature = assert(name:match("^[^/]+"))
		local cfgchunk = readdemo(feature.."/configs.lua")
		local test = {
			id = name:gsub("[^%w_]", ""),
			cases = {},
		}
		for _, case in ipairs(cases) do
			local config = runtime[case:match("[^/]+$")] or runtime.luarocks
			local cfgscript = cfgchunk:gsub("([%a_][%w_]*)%s*=", config.cfgpattern)
			local path = name.."/"..case..".lua"
			local script, loop = readdemo(path):match(
				'^dofile "configs.lua"%s*(.-)([%a_][%w_%.]*%s*%(%s*%))%s*$')
			test.cases[case:gsub("[^%w_]", "_")] = {
				lua = config.lua,
				gettime = config.gettime,
				setup = cfgscript..script,
				test = loop,
			}
		end
		table.insert(list, test)
	end
	return table.unpack(list)
end

return descibetests{
	["idle"] = {
		"copas",
		"coutil",
		"luvcoro",
		"luv",
	},
	["tcp/upload"] = {
		"copas",
		"coutil",
		"luv",
	},
	["tcp/reqreply"] = {
		"multiplex/copas",
		"multiplex/coutil",
		"multiplex/luv",
		"serial/copas",
		"serial/coutil",
		"serial/luv",
	},
}
